#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Puzzles/Light/SandsOfSorrowPuzzleDoor.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSandsPuzzleDoorMovementTest,
	"SandsOfSorrow.LightPuzzle.DoorMovement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Проверяет настоящий sweep, повторный запрос, блокировку и завершение после освобождения пути.
bool FSandsPuzzleDoorMovementTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	ASandsOfSorrowPuzzleDoor* Door = World->SpawnActor<ASandsOfSorrowPuzzleDoor>(FVector(0, 0, 120), FRotator::ZeroRotator);
	AActor* Obstacle = World->SpawnActor<AActor>();
	UBoxComponent* Blocker = NewObject<UBoxComponent>(Obstacle);
	Obstacle->SetRootComponent(Blocker);
	Blocker->SetBoxExtent(FVector(70, 20, 15));
	Blocker->SetCollisionProfileName(TEXT("BlockAll"));
	Blocker->RegisterComponent();
	Obstacle->SetActorLocation(FVector(0, 0, 420));
	Door->DispatchBeginPlay();
	Door->OpenDuration = 1.0f;
	Door->SetOpen(true);
	Door->Tick(0.5f);
	const double FirstHeight = Door->GetActorLocation().Z;
	TestTrue(TEXT("Door moves toward obstruction"), FirstHeight > 120.0);
	TestFalse(TEXT("Blocked door is not fully open"), Door->IsFullyOpen());
	Door->SetOpen(true);
	Door->Tick(0.5f);
	TestTrue(TEXT("Repeat request preserves progress"), Door->GetActorLocation().Z >= FirstHeight);
	TestFalse(TEXT("Blocked sweep cannot mark passage open"), Door->IsFullyOpen());
	Obstacle->Destroy();
	Door->Tick(1.0f);
	TestTrue(TEXT("Door completes after path clears"), Door->IsFullyOpen());
	TestTrue(TEXT("Door reaches full offset"), FMath::IsNearlyEqual(Door->GetActorLocation().Z, 400.0, 0.1));
	Door->SetOpen(false);
	TestTrue(TEXT("Game request cannot close solved door"), Door->IsFullyOpen());
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}
#endif
