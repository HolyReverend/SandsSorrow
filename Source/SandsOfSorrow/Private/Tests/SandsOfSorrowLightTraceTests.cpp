#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Puzzles/Light/SandsOfSorrowLightEmitter.h"
#include "Puzzles/Light/SandsOfSorrowLightPuzzle.h"
#include "Puzzles/Light/SandsOfSorrowLightReceiver.h"
#include "Puzzles/Light/SandsOfSorrowReflector.h"

namespace SandsLightTraceTests
{
// Изолированная физическая сцена с настоящими устройствами и препятствиями.
struct FTraceScene
{
	UWorld* World = nullptr;
	ASandsOfSorrowLightPuzzle* Puzzle = nullptr;
	ASandsOfSorrowLightEmitter* Emitter = nullptr;
	ASandsOfSorrowLightReceiver* Receiver = nullptr;

	FTraceScene()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
		Puzzle = World->SpawnActor<ASandsOfSorrowLightPuzzle>();
		Emitter = World->SpawnActor<ASandsOfSorrowLightEmitter>(FVector(-40, 0, 50), FRotator::ZeroRotator);
		Receiver = World->SpawnActor<ASandsOfSorrowLightReceiver>(FVector(400, 0, 50), FRotator(0, 180, 0));
		Puzzle->Emitter = Emitter;
		Puzzle->Receiver = Receiver;
	}

	~FTraceScene()
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	}

	// Препятствие без знания о головоломке должно остановить первый блокирующий запрос.
	AActor* Wall(const FVector& Location, const FVector& Extent)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Box);
		Box->SetBoxExtent(Extent);
		Box->SetCollisionProfileName(TEXT("BlockAll"));
		Box->RegisterComponent();
		Actor->SetActorLocation(Location);
		return Actor;
	}

	// Нормаль 135 градусов превращает вход +X в выход +Y.
	ASandsOfSorrowReflector* OneBounce()
	{
		Emitter->SetActorLocation(FVector(-440, 0, 50));
		ASandsOfSorrowReflector* Mirror = World->SpawnActor<ASandsOfSorrowReflector>(FVector::ZeroVector, FRotator(0, 90, 0));
		Puzzle->Reflectors.Add(Mirror);
		Receiver->SetActorLocation(FVector(-6.2 * FMath::Sqrt(2.0), 400, 50));
		Receiver->SetActorRotation(FRotator(0, -90, 0));
		return Mirror;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSandsLightDirectTraceTest,
	"SandsOfSorrow.LightPuzzle.DirectTrace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Проверяет реальные координаты, первый блок, обратную сторону и отсутствие применения состояния.
bool FSandsLightDirectTraceTest::RunTest(const FString& Parameters)
{
	using namespace SandsLightTraceTests;
	FTraceScene Scene;
	Scene.Puzzle->MaxReflections = 0;
	FLightBeamTraceResult Result = Scene.Puzzle->TraceBeam();
	TestTrue(TEXT("Direct receiver reached with zero reflection budget"), Result.Termination == ELightBeamTermination::ReceiverHit);
	TestTrue(TEXT("Reached receiver is explicit target"), Result.ReachedReceiver.Get() == Scene.Receiver);
	if (!TestEqual(TEXT("One direct segment"), Result.Segments.Num(), 1)) { return false; }
	TestTrue(TEXT("Direct start outside source body"), Result.Segments[0].Start.Equals(FVector(0, 0, 150), 0.01));
	TestTrue(TEXT("Direct end at receiver front"), Result.Segments[0].End.Equals(FVector(393.8, 0, 150), 0.01));
	TestTrue(TEXT("Direct distance"), FMath::IsNearlyEqual(Result.TravelledDistance, 393.8, 0.01));
	TestFalse(TEXT("Calculation does not illuminate receiver"), Scene.Receiver->IsIlluminated());
	Scene.Puzzle->TraceBeam();
	TestFalse(TEXT("Repeated calculation has no state effects"), Scene.Receiver->IsIlluminated());

	Scene.Receiver->SetActorRotation(FRotator::ZeroRotator);
	Result = Scene.Puzzle->TraceBeam();
	TestTrue(TEXT("Receiver back blocks"), Result.Termination == ELightBeamTermination::Blocked);
	TestFalse(TEXT("Back does not reach receiver"), Result.ReachedReceiver.IsValid());
	Scene.Receiver->SetActorRotation(FRotator(0, 180, 0));
	Scene.Puzzle->MaxTotalDistance = 100;
	Result = Scene.Puzzle->TraceBeam();
	TestTrue(TEXT("Miss ends at total distance"), Result.Termination == ELightBeamTermination::DistanceLimit);
	TestTrue(TEXT("Distance endpoint"), Result.Segments[0].End.Equals(FVector(100, 0, 150), 0.01));
	Scene.Puzzle->MaxTotalDistance = 10000;
	AActor* Wall = Scene.Wall(FVector(200, 0, 150), FVector(1, 40, 40));
	Result = Scene.Puzzle->TraceBeam();
	TestTrue(TEXT("Wall prevents receiver hit"), Result.Termination == ELightBeamTermination::Blocked && !Result.ReachedReceiver.IsValid());
	TestEqual(TEXT("Actual first blocking actor"), Result.Segments[0].Hit.GetActor(), Wall);
	TestTrue(TEXT("Wall endpoint"), Result.Segments[0].End.Equals(FVector(199, 0, 150), 0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSandsLightSingleReflectionTest,
	"SandsOfSorrow.LightPuzzle.SingleReflection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Проверяет закон отражения по двум отрезкам и сохранение всей длины, включая epsilon.
bool FSandsLightSingleReflectionTest::RunTest(const FString& Parameters)
{
	using namespace SandsLightTraceTests;
	FTraceScene Scene;
	ASandsOfSorrowReflector* Mirror = Scene.OneBounce();
	FLightBeamTraceResult Result = Scene.Puzzle->TraceBeam();
	TestTrue(TEXT("One bounce reaches receiver"), Result.Termination == ELightBeamTermination::ReceiverHit && Result.ReachedReceiver.Get() == Scene.Receiver);
	TestEqual(TEXT("Exactly one reflection"), Result.ReflectionCount, 1);
	if (!TestEqual(TEXT("Exactly two segments"), Result.Segments.Num(), 2)) { return false; }
	const double HitX = -6.2 * FMath::Sqrt(2.0);
	TestTrue(TEXT("Actual mirror front coordinate"), Result.Segments[0].End.Equals(FVector(HitX, 0, 150), 0.01));
	TestTrue(TEXT("Reflected endpoint"), Result.Segments[1].End.Equals(FVector(HitX, 393.8, 150), 0.01));
	TestTrue(TEXT("Visible segments are continuous"), Result.Segments[1].Start.Equals(Result.Segments[0].End, 0.0001));
	TestTrue(TEXT("Shared total distance includes offset exactly once"), FMath::IsNearlyEqual(Result.TravelledDistance, 400 + HitX + 393.8, 0.01));
	TestFalse(TEXT("No receiver state applied"), Scene.Receiver->IsIlluminated());
	TestEqual(TEXT("No device rotation changed"), Mirror->GetRotationIndex(), 0);
	Scene.Puzzle->MaxReflections = 0;
	Result = Scene.Puzzle->TraceBeam();
	TestTrue(TEXT("Zero budget stops on mirror"), Result.Termination == ELightBeamTermination::ReflectionLimit);
	TestEqual(TEXT("No outgoing segment beyond budget"), Result.Segments.Num(), 1);
	Scene.Puzzle->MaxReflections = 1;
	Mirror->Interact_Implementation(Scene.Emitter);
	Result = Scene.Puzzle->TraceBeam();
	TestFalse(TEXT("Wrong orientation cannot keep old receiver"), Result.ReachedReceiver.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSandsLightTraceSafetyTest,
	"SandsOfSorrow.LightPuzzle.TraceSafety",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Ошибки ссылок, тонкое препятствие в epsilon и выключение источника не дают ложного приёмника.
bool FSandsLightTraceSafetyTest::RunTest(const FString& Parameters)
{
	using namespace SandsLightTraceTests;
	FTraceScene Scene;
	Scene.Emitter->bEnabled = false;
	FLightBeamTraceResult Result = Scene.Puzzle->TraceBeam();
	TestTrue(TEXT("Disabled source gives empty path"), Result.Termination == ELightBeamTermination::SourceDisabled && Result.Segments.IsEmpty());
	Scene.Emitter->bEnabled = true;
	Scene.Puzzle->Receiver = nullptr;
	Result = Scene.Puzzle->TraceBeam();
	TestTrue(TEXT("Missing reference gives empty invalid result"), Result.Termination == ELightBeamTermination::InvalidConfiguration && Result.Segments.IsEmpty());
	Scene.Puzzle->Receiver = Scene.Receiver;
	ASandsOfSorrowLightReceiver* Foreign = Scene.World->SpawnActor<ASandsOfSorrowLightReceiver>(FVector(200, 0, 50), FRotator(0, 180, 0));
	Result = Scene.Puzzle->TraceBeam();
	TestTrue(TEXT("Unassigned receiver blocks with configuration diagnostic"), Result.Termination == ELightBeamTermination::InvalidConfiguration && !Result.ReachedReceiver.IsValid());
	Foreign->Destroy();
	ASandsOfSorrowReflector* Mirror = Scene.OneBounce();
	Scene.Puzzle->Reflectors.Empty();
	Result = Scene.Puzzle->TraceBeam();
	TestTrue(TEXT("Unassigned mirror is not traversed"), Result.Termination == ELightBeamTermination::InvalidConfiguration && Result.ReflectionCount == 0);
	Scene.Puzzle->Reflectors.Add(Mirror);
	const double HitX = -6.2 * FMath::Sqrt(2.0);
	AActor* ThinWall = Scene.Wall(FVector(HitX, 0.025, 150), FVector(0.1, 0.005, 20));
	Result = Scene.Puzzle->TraceBeam();
	TestTrue(TEXT("Thin epsilon obstacle blocks"), Result.Termination == ELightBeamTermination::Blocked && !Result.ReachedReceiver.IsValid());
	if (!TestEqual(TEXT("Thin obstacle keeps terminal segment"), Result.Segments.Num(), 2)) { return false; }
	TestEqual(TEXT("Actual epsilon blocker"), Result.Segments[1].Hit.GetActor(), ThinWall);
	TestTrue(TEXT("Epsilon obstacle endpoint"), Result.Segments[1].End.Equals(FVector(HitX, 0.02, 150), 0.001));
	Scene.Puzzle->MaxTotalDistance = 400 + HitX + 0.01;
	Result = Scene.Puzzle->TraceBeam();
	TestTrue(TEXT("Budget can end within offset"), Result.Termination == ELightBeamTermination::DistanceLimit);
	TestTrue(TEXT("Offset cannot exceed distance budget"), Result.TravelledDistance <= Scene.Puzzle->MaxTotalDistance + 0.0001);
	ThinWall->Destroy();
	Scene.Puzzle->MaxTotalDistance = 10000;
	Scene.Puzzle->MaxReflections = 17;
	Result = Scene.Puzzle->TraceBeam();
	TestTrue(TEXT("Reflection budget above hard limit rejected"), Result.Termination == ELightBeamTermination::InvalidConfiguration && Result.Segments.IsEmpty());
	Scene.Puzzle->MaxReflections = 1;
	Scene.Puzzle->Reflectors.Empty();
	USceneComponent* Origin = CastChecked<USceneComponent>(Scene.Emitter->GetDefaultSubobjectByName(TEXT("BeamOrigin")));
	Origin->SetRelativeLocation(FVector(0, 0, 100));
	Result = Scene.Puzzle->TraceBeam();
	TestTrue(TEXT("Source in its own body is invalid, not a free path"), Result.Termination == ELightBeamTermination::InvalidConfiguration && !Result.ReachedReceiver.IsValid());
	return true;
}

#endif
