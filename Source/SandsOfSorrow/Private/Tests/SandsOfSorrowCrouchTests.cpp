#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Characters/SandsOfSorrowPlayerCharacter.h"
#include "Characters/SandsOfSorrowCharacterMovementSettings.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

namespace SandsCrouchTests
{
// Настоящая коллизия и физика UE; после прогрева двигаем только компонент без повторного World Tick.
struct FScene
{
    UWorld* World = nullptr;
    ASandsOfSorrowPlayerCharacter* Character = nullptr;
    UCharacterMovementComponent* Movement = nullptr;
    AActor* Floor = nullptr;

    FScene()
    {
        World = UWorld::CreateWorld(EWorldType::Game, false);
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
        World->InitializeActorsForPlay(FURL());
        Floor = Block(FVector(0.0, 0.0, -10.0), FVector(1000.0, 1000.0, 10.0));
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Character = World->SpawnActor<ASandsOfSorrowPlayerCharacter>(FVector(0.0, 0.0, 100.0), FRotator::ZeroRotator, Spawn);
        Movement = Character->GetCharacterMovement();
        // Изолированный Pawn без Controller должен выполнять штатную физику движения.
        Movement->bRunPhysicsWithNoController = true;
        World->BeginPlay();
        World->GetWorldSettings()->NotifyBeginPlay();
        for (int32 Index = 0; Index < 12; ++Index) { World->Tick(LEVELTICK_All, 0.1f); }
        Character->SetActorLocation(FVector(0.0, 0.0, 98.0), false, nullptr, ETeleportType::TeleportPhysics);
        Movement->SetMovementMode(MOVE_Walking);
        Movement->JumpZVelocity = 500.0f;
        Movement->GravityScale = 2.0f;
        Character->JumpMaxCount = 1;
        Character->JumpMaxHoldTime = 0.0f;
        Step();
    }

    ~FScene()
    {
        World->EndPlay(EEndPlayReason::Quit);
        GEngine->DestroyWorldContext(World);
        World->DestroyWorld(false);
    }

    // Блокирующий примитив участвует в штатных проверках капсулы и опоры.
    AActor* Block(const FVector& Center, const FVector& Extent)
    {
        AActor* Actor = World->SpawnActor<AActor>();
        UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
        Actor->SetRootComponent(Box);
        Box->SetBoxExtent(Extent);
        Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Box->SetCollisionObjectType(ECC_WorldStatic);
        Box->SetCollisionResponseToAllChannels(ECR_Block);
        Box->RegisterComponent();
        Actor->SetActorLocation(Center);
        return Actor;
    }

    void Step(int32 Count = 1)
    {
        for (int32 Index = 0; Index < Count; ++Index)
        {
            Movement->TickComponent(1.0f / 120.0f, LEVELTICK_All, &Movement->PrimaryComponentTick);
        }
    }

    void ProcessJump() { static_cast<ACharacter*>(Character)->CheckJumpInput(1.0f / 120.0f); }
    void Hold() { Character->SetCrouchHeld(true); Step(); }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSandsCrouchStanceTest, "SandsOfSorrow.Crouch.StanceAndBlockedStand",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Отпускание под потолком сохраняет малую капсулу; освобождение пространства завершает вставание.
bool FSandsCrouchStanceTest::RunTest(const FString& Parameters)
{
    SandsCrouchTests::FScene Scene;
    Scene.Hold();
    TestTrue(TEXT("Ground hold physically crouches"), Scene.Character->bIsCrouched);
    TestEqual(TEXT("Crouched unscaled half-height"), Scene.Character->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(), 52.0f);
    TestEqual(TEXT("Crouch speed at default base"), Scene.Movement->GetMaxSpeed(), 250.0f);
    Scene.Movement->MaxWalkSpeed = 640.0f;
    TestEqual(TEXT("Crouch speed follows changed base"), Scene.Movement->GetMaxSpeed(), 320.0f);
    TestEqual(TEXT("Crouch ground acceleration"), Scene.Movement->GetMaxAcceleration(), 2400.0f);
    TestEqual(TEXT("Crouch ground braking"), Scene.Movement->GetMaxBrakingDeceleration(), 2000.0f);
    AActor* Ceiling = Scene.Block(FVector(0.0, 0.0, 150.0), FVector(200.0, 200.0, 5.0));
    Scene.Character->SetCrouchHeld(false);
    Scene.Step(3);
    TestFalse(TEXT("Release clears held intent"), Scene.Character->IsCrouchHeld());
    TestTrue(TEXT("Blocked standing retains actual crouch"), Scene.Character->bIsCrouched);
    TestEqual(TEXT("Blocked standing keeps small capsule"), Scene.Character->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(), 52.0f);
    Ceiling->Destroy();
    Scene.Step(3);
    TestFalse(TEXT("Clearance automatically completes released stand"), Scene.Character->bIsCrouched);
    TestEqual(TEXT("Standing restores native height"), Scene.Character->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(), 96.0f);
    TestEqual(TEXT("Standing acceleration restores base"), Scene.Movement->GetMaxAcceleration(), Scene.Movement->MaxAcceleration);
    TestEqual(TEXT("Standing braking restores base"), Scene.Movement->GetMaxBrakingDeceleration(), Scene.Movement->BrakingDecelerationWalking);
    Scene.Character->SetActorScale3D(FVector(0.8));
    Scene.Character->SetActorLocation(FVector(0.0, 0.0, 78.8), false, nullptr, ETeleportType::TeleportPhysics);
    Scene.Movement->bForceNextFloorCheck = true;
    Scene.Hold();
    TestTrue(TEXT("Scaled capsule physically crouches"), Scene.Character->bIsCrouched);
    TestTrue(TEXT("Scaled crouch height follows scale"), FMath::IsNearlyEqual(Scene.Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), 41.6f, 0.01f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSandsCrouchJumpTest, "SandsOfSorrow.Crouch.JumpTransactionAndLanding",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Места для стояния недостаточно для прыжка: отказ атомарен и не превращается в отложенный отрыв.
bool FSandsCrouchJumpTest::RunTest(const FString& Parameters)
{
    SandsCrouchTests::FScene Scene;
    Scene.Hold();
    AActor* Ceiling = Scene.Block(FVector(0.0, 0.0, 215.0), FVector(200.0, 200.0, 5.0));
    Scene.Character->Jump();
    Scene.ProcessJump();
    TestTrue(TEXT("Rejected jump keeps actual crouch"), Scene.Character->bIsCrouched);
    TestTrue(TEXT("Rejected jump keeps held intent"), Scene.Character->IsCrouchHeld());
    TestFalse(TEXT("Rejected jump never takes off"), Scene.Movement->IsFalling());
    TestEqual(TEXT("Rejected jump has no impulse"), Scene.Movement->Velocity.Z, 0.0);
    TestEqual(TEXT("Rejected jump consumes no count"), Scene.Character->JumpCurrentCount, 0);
    Ceiling->Destroy();
    Scene.ProcessJump();
    Scene.Step(2);
    TestFalse(TEXT("Obstacle removal does not retry jump"), Scene.Movement->IsFalling());
    Scene.Movement->Velocity = FVector(100.0, 35.0, 0.0);
    Scene.Character->StopJumping();
    Scene.Character->Jump();
    Scene.ProcessJump();
    TestTrue(TEXT("Fresh jump takes off"), Scene.Movement->IsFalling());
    TestFalse(TEXT("Successful takeoff stands"), Scene.Character->bIsCrouched);
    TestTrue(TEXT("Successful jump preserves hold"), Scene.Character->IsCrouchHeld());
    TestEqual(TEXT("Takeoff preserves X velocity"), Scene.Movement->Velocity.X, 100.0);
    TestEqual(TEXT("Takeoff preserves Y velocity"), Scene.Movement->Velocity.Y, 35.0);
    TestEqual(TEXT("Air acceleration uses base"), Scene.Movement->GetMaxAcceleration(), Scene.Movement->MaxAcceleration);
    TestEqual(TEXT("Air braking uses base"), Scene.Movement->GetMaxBrakingDeceleration(), Scene.Movement->BrakingDecelerationFalling);
    for (int32 Index = 0; Index < 240 && Scene.Movement->IsFalling(); ++Index) { Scene.Step(); }
    TestTrue(TEXT("Jump lands on real floor"), Scene.Movement->IsMovingOnGround());
    Scene.Step();
    TestTrue(TEXT("Held landing restores crouch"), Scene.Character->bIsCrouched);
    Scene.Character->Jump();
    Scene.ProcessJump();
    TestTrue(TEXT("Second held jump takes off"), Scene.Movement->IsFalling());
    Scene.Character->SetCrouchHeld(false);
    Scene.Step();
    TestFalse(TEXT("Air release clears intent"), Scene.Character->IsCrouchHeld());
    TestEqual(TEXT("Air release retains standing flight volume"), Scene.Character->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(), 96.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSandsCrouchLedgeTest, "SandsOfSorrow.Crouch.LedgeAirVolume",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Реальный сход с конечной платформы проверяет политику края; воздушный отпуск не расширяет капсулу.
bool FSandsCrouchLedgeTest::RunTest(const FString& Parameters)
{
    SandsCrouchTests::FScene Scene;
    Scene.Hold();
    Scene.Block(FVector(1400.0, 0.0, -210.0), FVector(2000.0, 1000.0, 10.0));
    Scene.Character->SetActorLocation(FVector(950.0, 0.0, Scene.Character->GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
    Scene.Movement->bForceNextFloorCheck = true;
    Scene.Movement->Velocity = FVector(250.0, 0.0, 0.0);
    for (int32 Index = 0; Index < 240 && !Scene.Movement->IsFalling(); ++Index)
    {
        Scene.Character->AddMovementInput(FVector::ForwardVector);
        Scene.Step();
    }
    TestTrue(TEXT("Crouched movement walks off actual platform edge"), Scene.Movement->IsFalling());
    TestTrue(TEXT("Walk-off crosses platform edge"), Scene.Character->GetActorLocation().X > 1000.0);
    TestTrue(TEXT("Ledge retains crouch stance"), Scene.Character->bIsCrouched);
    Scene.Character->SetCrouchHeld(false);
    Scene.Step();
    TestEqual(TEXT("Air release keeps small volume"), Scene.Character->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(), 52.0f);
    TestEqual(TEXT("Crouched falling uses base acceleration"), Scene.Movement->GetMaxAcceleration(), Scene.Movement->MaxAcceleration);
    TestEqual(TEXT("Crouched falling uses air braking"), Scene.Movement->GetMaxBrakingDeceleration(), Scene.Movement->BrakingDecelerationFalling);
    for (int32 Index = 0; Index < 240 && Scene.Movement->IsFalling(); ++Index) { Scene.Step(); }
    Scene.Step();
    TestTrue(TEXT("Ledge fall lands on lower support"), Scene.Movement->IsMovingOnGround());
    TestFalse(TEXT("Released landing safely stands"), Scene.Character->bIsCrouched);
    TestEqual(TEXT("Released landing restores standing height"), Scene.Character->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(), 96.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSandsCrouchSettingsTest, "SandsOfSorrow.Crouch.SettingsAtomicity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Неверный поздний параметр или геометрия не должны частично применять ранние поля настроек.
bool FSandsCrouchSettingsTest::RunTest(const FString& Parameters)
{
    SandsCrouchTests::FScene Scene;
    TStrongObjectPtr<USandsOfSorrowCharacterMovementSettings> Settings(NewObject<USandsOfSorrowCharacterMovementSettings>());
    FString Error;
    const float OriginalSpeed = Scene.Movement->MaxWalkSpeed;
    const float OriginalHeight = Scene.Movement->GetCrouchedHalfHeight();
    Settings->MaxWalkSpeed = 600.0f;
    for (float* Field : {&Settings->CrouchedHalfHeight, &Settings->CrouchMaxAcceleration, &Settings->CrouchBrakingDeceleration, &Settings->CrouchGroundFriction})
    {
        const float OriginalValue = *Field;
        for (const float Invalid : {-1.0f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()})
        {
            *Field = Invalid;
            TestFalse(TEXT("Invalid crouch field rejects whole set"), Settings->ApplyTo(*Scene.Character, Error));
            TestFalse(TEXT("Invalid configuration explains refusal"), Error.IsEmpty());
            TestEqual(TEXT("Invalid set preserves speed"), Scene.Movement->MaxWalkSpeed, OriginalSpeed);
            TestEqual(TEXT("Invalid set preserves crouch height"), Scene.Movement->GetCrouchedHalfHeight(), OriginalHeight);
        }
        *Field = OriginalValue;
    }
    for (const float InvalidHeight : {41.0f, 97.0f})
    {
        Settings->CrouchedHalfHeight = InvalidHeight;
        TestFalse(TEXT("Radius and standing bounds reject geometry"), Settings->ApplyTo(*Scene.Character, Error));
        TestEqual(TEXT("Geometry failure preserves speed"), Scene.Movement->MaxWalkSpeed, OriginalSpeed);
    }
    Settings->CrouchedHalfHeight = 52.0f;
    Settings->MinAnalogWalkSpeed = 301.0f;
    TestFalse(TEXT("Analog minimum above crouch maximum rejects"), Settings->ApplyTo(*Scene.Character, Error));
    TestEqual(TEXT("Analog refusal preserves speed"), Scene.Movement->MaxWalkSpeed, OriginalSpeed);
    Settings->MinAnalogWalkSpeed = 300.0f;
    TestTrue(TEXT("Analog boundary applies valid whole set"), Settings->ApplyTo(*Scene.Character, Error));
    TestEqual(TEXT("Native crouch speed derives from base"), Scene.Movement->MaxWalkSpeedCrouched, 300.0f);
    Scene.Hold();
    Settings->CrouchedHalfHeight = 60.0f;
    TestTrue(TEXT("Crouched actor validates against standing CDO height"), Settings->ApplyTo(*Scene.Character, Error));
    TestEqual(TEXT("Reconfiguration preserves actual capsule"), Scene.Character->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(), 52.0f);
    return true;
}

#endif
