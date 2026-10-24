#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Puzzles/Light/SandsOfSorrowLightEmitter.h"
#include "Puzzles/Light/SandsOfSorrowLightPuzzle.h"
#include "Puzzles/Light/SandsOfSorrowLightReceiver.h"
#include "Puzzles/Light/SandsOfSorrowReflector.h"
#include <limits>

namespace SandsLightChainTests
{
// Изолированный мир; два диагональных зеркала образуют +X → +Y → +X.
struct FChainScene
{
	UWorld* World = nullptr;
	ASandsOfSorrowLightPuzzle* Puzzle = nullptr;
	ASandsOfSorrowLightEmitter* Emitter = nullptr;
	ASandsOfSorrowLightReceiver* Receiver = nullptr;
	ASandsOfSorrowReflector* A = nullptr;
	ASandsOfSorrowReflector* B = nullptr;
	const double HitX = -6.2 * FMath::Sqrt(2.0);
	const double HitY = 400.0 - 6.2 * FMath::Sqrt(2.0);

	FChainScene()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
		Puzzle = World->SpawnActor<ASandsOfSorrowLightPuzzle>();
		Emitter = World->SpawnActor<ASandsOfSorrowLightEmitter>(FVector(-440, 0, 50), FRotator::ZeroRotator);
		Receiver = World->SpawnActor<ASandsOfSorrowLightReceiver>(FVector(400, HitY, 50), FRotator(0, 180, 0));
		A = World->SpawnActor<ASandsOfSorrowReflector>(FVector::ZeroVector, FRotator(0, 90, 0));
		B = World->SpawnActor<ASandsOfSorrowReflector>(FVector(HitX, 400, 0), FRotator(0, 270, 0));
		Puzzle->Emitter = Emitter;
		Puzzle->Receiver = Receiver;
		Puzzle->Reflectors = {A, B};
	}

	~FChainScene()
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSandsTwoReflectorsTest,
	"SandsOfSorrow.LightPuzzle.TwoReflectors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Координаты каждого попадания, исправление поворота и отсутствие побочных эффектов.
bool FSandsTwoReflectorsTest::RunTest(const FString& Parameters)
{
	using namespace SandsLightChainTests;
	FChainScene Scene;
	FLightBeamTraceResult Result = Scene.Puzzle->TraceBeam();
	TestTrue(TEXT("Two mirrors reach receiver"), Result.Termination == ELightBeamTermination::ReceiverHit && Result.ReachedReceiver.Get() == Scene.Receiver);
	TestEqual(TEXT("Two reflections"), Result.ReflectionCount, 2);
	if (!TestEqual(TEXT("Three segments"), Result.Segments.Num(), 3)) { return false; }
	TestTrue(TEXT("First actual point"), Result.Segments[0].End.Equals(FVector(Scene.HitX, 0, 150), 0.01));
	TestTrue(TEXT("Second actual point"), Result.Segments[1].End.Equals(FVector(Scene.HitX, Scene.HitY, 150), 0.01));
	TestTrue(TEXT("Receiver front point"), Result.Segments[2].End.Equals(FVector(393.8, Scene.HitY, 150), 0.01));
	TestEqual(TEXT("Three primary traces"), Result.PrimaryTraceCount, 3);
	TestEqual(TEXT("Two offset checks"), Result.OffsetTraceCount, 2);
	TestFalse(TEXT("Receiver state unchanged"), Scene.Receiver->IsIlluminated());
	Scene.B->Interact_Implementation(Scene.Emitter);
	TestFalse(TEXT("Wrong B clears result target"), Scene.Puzzle->TraceBeam().ReachedReceiver.IsValid());
	for (int32 Step = 0; Step < 3; ++Step) { Scene.B->Interact_Implementation(Scene.Emitter); }
	TestTrue(TEXT("Correction restores path"), Scene.Puzzle->TraceBeam().ReachedReceiver.Get() == Scene.Receiver);
	Scene.Puzzle->MaxReflections = 1;
	Result = Scene.Puzzle->TraceBeam();
	TestTrue(TEXT("Insufficient budget stops on second mirror"), Result.Termination == ELightBeamTermination::ReflectionLimit && Result.ReflectionCount == 1 && Result.Segments.Num() == 2);
	Scene.Puzzle->MaxReflections = 2;
	TestTrue(TEXT("Receiver beyond last permitted reflection still reachable"), Scene.Puzzle->TraceBeam().ReachedReceiver.Get() == Scene.Receiver);
	Scene.Puzzle->MaxTotalDistance = 600;
	Result = Scene.Puzzle->TraceBeam();
	TestTrue(TEXT("One shared distance budget"), Result.Termination == ELightBeamTermination::DistanceLimit && FMath::IsNearlyEqual(Result.TravelledDistance, 600.0, 0.01));
	TestTrue(TEXT("Distance endpoint after first reflection"), Result.Segments.Last().End.Equals(FVector(Scene.HitX, 600 - (400 + Scene.HitX), 150), 0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSandsClosedLightPathTest,
	"SandsOfSorrow.LightPuzzle.ClosedPathLimits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Две встречные плоскости допускают повторные посещения; останавливаемся по бюджету, не по Actor.
bool FSandsClosedLightPathTest::RunTest(const FString& Parameters)
{
	using namespace SandsLightChainTests;
	FChainScene Scene;
	Scene.A->SetActorRotation(FRotator(0, -45, 0));
	Scene.B->SetActorLocation(FVector(400, 0, 0));
	Scene.B->SetActorRotation(FRotator(0, 135, 0));
	Scene.Emitter->SetActorLocation(FVector(160, 0, 50));
	// Убираем корпус источника только в стенде: иначе он честно остановит обратный луч.
	Scene.Emitter->FindComponentByClass<UStaticMeshComponent>()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Scene.Receiver->SetActorLocation(FVector(1000, 1000, 50));
	FLightBeamTraceResult Result = Scene.Puzzle->TraceBeam();
	TestTrue(TEXT("Closed path reports budget, not a claimed cycle"), Result.Termination == ELightBeamTermination::ReflectionLimit);
	TestEqual(TEXT("All sixteen reflections allowed"), Result.ReflectionCount, 16);
	TestEqual(TEXT("Terminal seventeenth segment"), Result.Segments.Num(), 17);
	TestEqual(TEXT("Seventeen primary traces"), Result.PrimaryTraceCount, 17);
	TestEqual(TEXT("Sixteen offset checks"), Result.OffsetTraceCount, 16);
	TestFalse(TEXT("Closed path never reaches receiver"), Result.ReachedReceiver.IsValid());
	Scene.Puzzle->MaxTotalDistance = 1000;
	Result = Scene.Puzzle->TraceBeam();
	TestTrue(TEXT("Distance can terminate closed path first"), Result.Termination == ELightBeamTermination::DistanceLimit && FMath::IsNearlyEqual(Result.TravelledDistance, 1000.0, 0.01));
	TestTrue(TEXT("No query count exceeds guards"), Result.PrimaryTraceCount <= 17 && Result.OffsetTraceCount <= 16 && Result.Segments.Num() <= 17);
	Scene.Puzzle->MaxReflections = 0;
	Result = Scene.Puzzle->TraceBeam();
	TestTrue(TEXT("Zero reflection budget finite"), Result.Termination == ELightBeamTermination::ReflectionLimit && Result.Segments.Num() == 1 && Result.OffsetTraceCount == 0);
	Scene.Puzzle->MaxTotalDistance = std::numeric_limits<double>::infinity();
	Result = Scene.Puzzle->TraceBeam();
	TestTrue(TEXT("Infinite distance rejected before physics"), Result.Termination == ELightBeamTermination::InvalidConfiguration && Result.PrimaryTraceCount == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSandsLightDeviceReferencesTest,
	"SandsOfSorrow.LightPuzzle.DeviceReferences",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Дубликаты, совместное назначение и уничтожение устройств не оставляют успешный старый результат.
bool FSandsLightDeviceReferencesTest::RunTest(const FString& Parameters)
{
	using namespace SandsLightChainTests;
	FChainScene Scene;
	Scene.Puzzle->Reflectors = {Scene.A, Scene.A};
	TestTrue(TEXT("Duplicate reference rejected"), Scene.Puzzle->TraceBeam().Termination == ELightBeamTermination::InvalidConfiguration);
	Scene.Puzzle->Reflectors = {Scene.A, nullptr};
	TestTrue(TEXT("Null reflector rejected"), Scene.Puzzle->TraceBeam().Termination == ELightBeamTermination::InvalidConfiguration);
	Scene.Puzzle->Reflectors = {Scene.A, Scene.B};
	ASandsOfSorrowLightPuzzle* Other = Scene.World->SpawnActor<ASandsOfSorrowLightPuzzle>();
	Other->Emitter = Scene.Emitter;
	TestTrue(TEXT("Shared emitter rejected"), Scene.Puzzle->TraceBeam().Termination == ELightBeamTermination::InvalidConfiguration);
	Other->Emitter = nullptr;
	Other->Receiver = Scene.Receiver;
	TestTrue(TEXT("Shared receiver rejected"), Scene.Puzzle->TraceBeam().Termination == ELightBeamTermination::InvalidConfiguration);
	Other->Receiver = nullptr;
	Other->Reflectors = {Scene.B};
	TestTrue(TEXT("Shared mirror rejected"), Scene.Puzzle->TraceBeam().Termination == ELightBeamTermination::InvalidConfiguration);
	Other->Destroy();
	TestTrue(TEXT("Destroying other room releases its registration"), Scene.Puzzle->TraceBeam().ReachedReceiver.Get() == Scene.Receiver);
	Scene.B->Destroy();
	const FLightBeamTraceResult Result = Scene.Puzzle->TraceBeam();
	TestTrue(TEXT("Destroyed device invalidates full path"), Result.Termination == ELightBeamTermination::InvalidConfiguration && Result.Segments.IsEmpty() && !Result.ReachedReceiver.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSandsOffsetReflectionTest,
	"SandsOfSorrow.LightPuzzle.OffsetReflection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Второе зеркало внутри epsilon не пропускается и проходит обычную обработку отражения.
bool FSandsOffsetReflectionTest::RunTest(const FString& Parameters)
{
	using namespace SandsLightChainTests;
	FChainScene Scene;
	const FVector NearHit(Scene.HitX, 0.02, 150);
	const FVector Normal = Scene.B->GetFaceNormal();
	FVector Root = NearHit - Normal * 6.2;
	Root.Z = 0;
	Scene.B->SetActorLocation(Root);
	Scene.B->GetWorkingSurface()->SetBoxExtent(FVector(0.1, 0.001, 40));
	// Только этот тесный тестовый стенд исключает корпуса и сужает рабочую область.
	TInlineComponentArray<UStaticMeshComponent*> Meshes;
	Scene.B->GetComponents(Meshes);
	for (UStaticMeshComponent* Mesh : Meshes) { Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
	Scene.Puzzle->MaxReflections = 2;
	const FLightBeamTraceResult Result = Scene.Puzzle->TraceBeam();
	TestEqual(TEXT("Both nearby surfaces reflected"), Result.ReflectionCount, 2);
	TestTrue(TEXT("Terminal mirror respects reflection budget"), Result.Termination == ELightBeamTermination::ReflectionLimit);
	TestEqual(TEXT("No extra primary query for pending offset hits"), Result.PrimaryTraceCount, 1);
	TestEqual(TEXT("Two short checks"), Result.OffsetTraceCount, 2);
	TestEqual(TEXT("Three continuous segments"), Result.Segments.Num(), 3);
	if (Result.Segments.Num() == 3)
	{
		TestTrue(TEXT("Actual second hit within epsilon"), Result.Segments[1].End.Equals(NearHit, 0.001));
	}
	return true;
}

#endif
