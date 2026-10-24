#include "SandsOfSorrowLightStateTestListener.h"

#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Puzzles/Light/SandsOfSorrowLightEmitter.h"
#include "Puzzles/Light/SandsOfSorrowLightPuzzle.h"
#include "Puzzles/Light/SandsOfSorrowLightReceiver.h"

void USandsOfSorrowLightStateTestListener::HandleIllumination(bool bIlluminated)
{
	++IlluminationEvents;
	// Обработчик видит уже зафиксированный результат и не может войти в тот же проход.
	bCoherent &= Puzzle && Receiver && Puzzle->IsSolved() && Receiver->IsIlluminated() == bIlluminated
		&& ((Puzzle->LastTraceResult.ReachedReceiver.Get() == Receiver) == bIlluminated);
	bReentryRejected &= Puzzle && !Puzzle->RecalculateNow();
	if (bDisableEmitterOnFirstLight && bIlluminated && IlluminationEvents == 1 && Emitter)
	{
		Emitter->bEnabled = false;
	}
}

void USandsOfSorrowLightStateTestListener::HandleSolved()
{
	++SolvedEvents;
	bCoherent &= Puzzle && Receiver && Puzzle->IsSolved() && Receiver->IsIlluminated()
		&& Puzzle->LastTraceResult.ReachedReceiver.Get() == Receiver;
	bReentryRejected &= Puzzle && !Puzzle->RecalculateNow();
}

#if WITH_DEV_AUTOMATION_TESTS

namespace SandsLightStateTests
{
// Отдельный игровой мир использует настоящие устройства и физические запросы.
struct FStateScene
{
	UWorld* World = nullptr;
	ASandsOfSorrowLightPuzzle* Puzzle = nullptr;
	ASandsOfSorrowLightEmitter* Emitter = nullptr;
	ASandsOfSorrowLightReceiver* Receiver = nullptr;
	USandsOfSorrowLightStateTestListener* Listener = nullptr;

	FStateScene()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
		Puzzle = World->SpawnActor<ASandsOfSorrowLightPuzzle>();
		Emitter = World->SpawnActor<ASandsOfSorrowLightEmitter>(FVector(-40, 0, 50), FRotator::ZeroRotator);
		Receiver = World->SpawnActor<ASandsOfSorrowLightReceiver>(FVector(400, 0, 50), FRotator(0, 180, 0));
		Puzzle->Emitter = Emitter;
		Puzzle->Receiver = Receiver;
		Puzzle->MaxReflections = 0;
		Listener = NewObject<USandsOfSorrowLightStateTestListener>(GetTransientPackage());
		Listener->AddToRoot();
		Listener->Puzzle = Puzzle;
		Listener->Receiver = Receiver;
		Listener->Emitter = Emitter;
		Receiver->OnIlluminationChanged.AddDynamic(Listener, &USandsOfSorrowLightStateTestListener::HandleIllumination);
		Puzzle->OnSolved.AddDynamic(Listener, &USandsOfSorrowLightStateTestListener::HandleSolved);
	}

	~FStateScene()
	{
		Listener->RemoveFromRoot();
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	}

	AActor* Wall()
	{
		AActor* Actor = World->SpawnActor<AActor>();
		UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Box);
		Box->SetBoxExtent(FVector(1, 40, 40));
		Box->SetCollisionProfileName(TEXT("BlockAll"));
		Box->RegisterComponent();
		Actor->SetActorLocation(FVector(200, 0, 150));
		return Actor;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSandsLightStateTransitionsTest,
	"SandsOfSorrow.LightPuzzle.StateTransitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Обрыв и восстановление меняют только текущее освещение; решение фиксируется один раз.
bool FSandsLightStateTransitionsTest::RunTest(const FString& Parameters)
{
	SandsLightStateTests::FStateScene Scene;
	TestFalse(TEXT("Pure trace does not apply state"), Scene.Receiver->IsIlluminated());
	TestTrue(TEXT("First application completed"), Scene.Puzzle->RecalculateNow());
	TestTrue(TEXT("Receiver illuminated"), Scene.Receiver->IsIlluminated());
	TestTrue(TEXT("Puzzle solved"), Scene.Puzzle->IsSolved());
	TestEqual(TEXT("First illumination event"), Scene.Listener->IlluminationEvents, 1);
	TestEqual(TEXT("Solved event once"), Scene.Listener->SolvedEvents, 1);
	Scene.Puzzle->RecalculateNow();
	TestEqual(TEXT("Identical frame has no new event"), Scene.Listener->IlluminationEvents, 1);
	AActor* Wall = Scene.Wall();
	Scene.Puzzle->RecalculateNow();
	TestFalse(TEXT("Unannounced wall extinguishes receiver next frame"), Scene.Receiver->IsIlluminated());
	TestTrue(TEXT("Solution remains after obstruction"), Scene.Puzzle->IsSolved());
	TestTrue(TEXT("Current result ends on obstacle"), Scene.Puzzle->LastTraceResult.Termination == ELightBeamTermination::Blocked);
	TestEqual(TEXT("One off transition"), Scene.Listener->IlluminationEvents, 2);
	Wall->SetActorLocation(FVector(200, 1000, 150));
	Scene.Puzzle->RecalculateNow();
	TestTrue(TEXT("Moving wall restores light"), Scene.Receiver->IsIlluminated());
	TestEqual(TEXT("One restore transition"), Scene.Listener->IlluminationEvents, 3);
	Scene.Receiver->SetActorRotation(FRotator::ZeroRotator);
	Scene.Puzzle->RecalculateNow();
	TestFalse(TEXT("Back face extinguishes receiver"), Scene.Receiver->IsIlluminated());
	Scene.Receiver->SetActorRotation(FRotator(0, 180, 0));
	Scene.Puzzle->RecalculateNow();
	TestTrue(TEXT("Corrected face restores light"), Scene.Receiver->IsIlluminated());
	TestEqual(TEXT("Solution notification did not repeat"), Scene.Listener->SolvedEvents, 1);
	TestTrue(TEXT("Callbacks read committed state"), Scene.Listener->bCoherent);
	TestTrue(TEXT("Callback reentry rejected"), Scene.Listener->bReentryRejected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSandsLightStateLifecycleTest,
	"SandsOfSorrow.LightPuzzle.StateLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Переназначение цели, ошибка ссылки и изменение из callback не оставляют прежнюю активацию.
bool FSandsLightStateLifecycleTest::RunTest(const FString& Parameters)
{
	SandsLightStateTests::FStateScene Scene;
	Scene.Listener->bDisableEmitterOnFirstLight = true;
	Scene.Puzzle->RecalculateNow();
	TestTrue(TEXT("Initial callback did not trigger recursive pass"), Scene.Receiver->IsIlluminated());
	Scene.Puzzle->RecalculateNow();
	TestFalse(TEXT("Next frame sees callback change"), Scene.Receiver->IsIlluminated());
	TestTrue(TEXT("Disabled result recorded"), Scene.Puzzle->LastTraceResult.Termination == ELightBeamTermination::SourceDisabled);
	Scene.Emitter->bEnabled = true;
	Scene.Puzzle->RecalculateNow();
	Scene.Receiver->SetActorLocation(FVector(1000, 1000, 50));
	ASandsOfSorrowLightReceiver* NewReceiver = Scene.World->SpawnActor<ASandsOfSorrowLightReceiver>(FVector(400, 0, 50), FRotator(0, 180, 0));
	Scene.Puzzle->Receiver = NewReceiver;
	Scene.Puzzle->RecalculateNow();
	TestFalse(TEXT("Old receiver extinguished on retarget"), Scene.Receiver->IsIlluminated());
	TestTrue(TEXT("New receiver illuminated on retarget"), NewReceiver->IsIlluminated());
	TestEqual(TEXT("Old receiver emitted off event"), Scene.Listener->IlluminationEvents, 4);
	Scene.Puzzle->Receiver = nullptr;
	Scene.Puzzle->RecalculateNow();
	TestFalse(TEXT("Missing target extinguishes last receiver"), NewReceiver->IsIlluminated());
	TestTrue(TEXT("Missing target has invalid configuration"), Scene.Puzzle->LastTraceResult.Termination == ELightBeamTermination::InvalidConfiguration);
	TestEqual(TEXT("Solved remains one transition"), Scene.Listener->SolvedEvents, 1);
	TestTrue(TEXT("Callbacks coherent through retarget"), Scene.Listener->bCoherent);
	TestTrue(TEXT("Callback reentry rejected"), Scene.Listener->bReentryRejected);
	return true;
}

#endif
