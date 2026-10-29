#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Characters/SandsOfSorrowCharacterMovementComponent.h"
#include "Characters/SandsOfSorrowPlayerCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSandsStrafeFreeLookTest,
	"SandsOfSorrow.Movement.StrafeFreeLook",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Проверяет фактический yaw стандартного компонента, не дублируя формулу разворота.
bool FSandsStrafeFreeLookTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	ASandsOfSorrowPlayerCharacter* Character = World->SpawnActor<ASandsOfSorrowPlayerCharacter>();
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	Controller->Possess(Character);
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	TestTrue(TEXT("Native Pawn installs movement gate"), Movement->IsA<USandsOfSorrowCharacterMovementComponent>());
	Movement->bOrientRotationToMovement = false;
	Movement->bUseControllerDesiredRotation = true;
	Character->bUseControllerRotationYaw = false;
	Movement->RotationRate = FRotator(0.0, 500.0, 0.0);
	Movement->SetMovementMode(MOVE_Walking);

	for (const double CameraYaw : {90.0, 179.0, -90.0, 0.0})
	{
		Controller->SetControlRotation(FRotator(0.0, CameraYaw, 0.0));
		Movement->Velocity = FVector::ZeroVector;
		Movement->PhysicsRotation(0.1f);
		TestEqual(TEXT("Idle orbit preserves body yaw"), Character->GetActorRotation().Yaw, 0.0, 0.001);
		TestEqual(TEXT("Idle orbit preserves camera yaw"), Controller->GetControlRotation().Yaw, CameraYaw, 0.001);
	}

	Controller->SetControlRotation(FRotator(0.0, 90.0, 0.0));
	for (const FVector Velocity : {FVector(100.0, 0.0, 0.0), FVector(-100.0, 0.0, 0.0), FVector(0.0, 100.0, 0.0), FVector(0.0, -100.0, 0.0)})
	{
		Character->SetActorRotation(FRotator::ZeroRotator);
		Movement->Velocity = Velocity;
		Movement->PhysicsRotation(0.1f);
		TestEqual(TEXT("Moving strafe smoothly approaches camera yaw"), Character->GetActorRotation().Yaw, 50.0, 0.001);
		Movement->PhysicsRotation(0.1f);
		TestEqual(TEXT("Moving strafe reaches camera yaw"), Character->GetActorRotation().Yaw, 90.0, 0.001);
	}

	Movement->Velocity = FVector::ZeroVector;
	Controller->SetControlRotation(FRotator(0.0, -90.0, 0.0));
	Movement->PhysicsRotation(0.1f);
	TestEqual(TEXT("Stop freezes current body yaw"), Character->GetActorRotation().Yaw, 90.0, 0.001);
	Movement->Velocity = FVector(0.5, 0.0, 0.0);
	Movement->PhysicsRotation(0.1f);
	TestEqual(TEXT("Sub-tolerance drift does not turn"), Character->GetActorRotation().Yaw, 90.0, 0.001);
	Movement->SetMovementMode(MOVE_Falling);
	Movement->Velocity = FVector(0.0, 0.0, 500.0);
	Movement->PhysicsRotation(0.1f);
	TestEqual(TEXT("Vertical-only jump preserves yaw"), Character->GetActorRotation().Yaw, 90.0, 0.001);
	Movement->Velocity = FVector(10.0, 0.0, 500.0);
	Controller->SetControlRotation(FRotator(0.0, 0.0, 0.0));
	Movement->PhysicsRotation(0.1f);
	TestEqual(TEXT("Horizontal airborne motion resumes smooth turn"), Character->GetActorRotation().Yaw, 40.0, 0.001);

	// В резервной схеме штатный путь по-прежнему работает при включённом orient-to-movement.
	Movement->SetMovementMode(MOVE_Walking);
	Movement->bOrientRotationToMovement = true;
	Movement->Velocity = FVector::ZeroVector;
	Character->SetActorRotation(FRotator(0.0, 30.0, 0.0));
	Movement->PhysicsRotation(0.1f);
	TestEqual(TEXT("Fallback preserves its native idle orientation"), Character->GetActorRotation().Yaw, 30.0, 0.001);
	TestTrue(TEXT("Controller rotation setting remains configured"), Movement->bUseControllerDesiredRotation);
	TestEqual(TEXT("Rotation rate preserved"), Movement->RotationRate.Yaw, 500.0);
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif
