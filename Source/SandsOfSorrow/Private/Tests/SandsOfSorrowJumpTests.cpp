#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Characters/SandsOfSorrowPlayerCharacter.h"
#include "Camera/CameraActor.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "UObject/UnrealType.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/SandsOfSorrowInteractionComponent.h"
#include "Puzzles/Light/SandsOfSorrowReflector.h"

namespace SandsJumpTests
{
// Отдельный мир содержит настоящую капсулу персонажа и блокирующие примитивы.
struct FTestScene
{
    UWorld* World = nullptr;
    ASandsOfSorrowPlayerCharacter* Character = nullptr;
    UCharacterMovementComponent* Movement = nullptr;
    float HalfHeight = 0.0f;
    float Radius = 0.0f;

    FTestScene()
    {
        World = UWorld::CreateWorld(EWorldType::Game, false);
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
        World->InitializeActorsForPlay(FURL());
        Block(FVector(0.0, 0.0, -10.0), FVector(1000.0, 1000.0, 10.0));
        FActorSpawnParameters SpawnParameters;
        SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Character = World->SpawnActor<ASandsOfSorrowPlayerCharacter>(FVector(0.0, 0.0, 100.0), FRotator::ZeroRotator, SpawnParameters);
        Movement = Character->GetCharacterMovement();
        // CreateWorld создаёт сцену запросов; изолированный мир не имеет GameMode для запуска Actor.
        World->BeginPlay();
        World->GetWorldSettings()->NotifyBeginPlay();
        // UE в редакторе удерживает Walking у края в первую секунду загрузки мира (StartFalling).
        for (int32 Index = 0; Index < 12; ++Index)
        {
            World->Tick(LEVELTICK_All, 0.1f);
        }
        Character->GetCapsuleComponent()->GetScaledCapsuleSize(Radius, HalfHeight);
        Character->SetActorLocation(FVector(0.0, 0.0, HalfHeight + 2.0), false, nullptr, ETeleportType::TeleportPhysics);
        Movement->SetMovementMode(MOVE_Walking);
        Movement->JumpZVelocity = 500.0f;
        Movement->GravityScale = 2.0f;
        Character->JumpMaxCount = 1;
        Character->JumpMaxHoldTime = 0.0f;
    }

    ~FTestScene()
    {
        World->EndPlay(EEndPlayReason::Quit);
        GEngine->DestroyWorldContext(World);
        World->DestroyWorld(false);
    }

    // Геометрия блокирует канал капсулы и участвует в обычных запросах мира.
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

    // Вызов через публичный API базового класса сохраняет виртуальную диспетчеризацию.
    void ProcessRequest()
    {
        static_cast<ACharacter*>(Character)->CheckJumpInput(1.0f / 60.0f);
    }

    AActor* Ceiling(float HorizontalOffset, float BottomOffset)
    {
        const FVector Center = Character->GetActorLocation() + FVector(HorizontalOffset, 0.0, HalfHeight + BottomOffset + 5.0);
        return Block(Center, FVector(5.0, 20.0, 5.0));
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSandsJumpClearanceTest,
    "SandsOfSorrow.Jump.ClearanceAndRetry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Проверяет реальную геометрию, а не копию вычисления высоты или выставленный флаг.
bool FSandsJumpClearanceTest::RunTest(const FString& Parameters)
{
    using namespace SandsJumpTests;
    {
        FTestScene Scene;
        Scene.Character->Jump();
        Scene.ProcessRequest();
        TestTrue(TEXT("Clear space produces falling mode"), Scene.Movement->IsFalling());
        TestTrue(TEXT("Clear space produces vertical impulse"), Scene.Movement->Velocity.Z > 0.0);
        TestEqual(TEXT("Standing jump adds no horizontal impulse"), Scene.Movement->Velocity.X, 0.0);
        TestEqual(TEXT("Standing jump adds no lateral impulse"), Scene.Movement->Velocity.Y, 0.0);
        TestEqual(TEXT("Successful jump increments count"), Scene.Character->JumpCurrentCount, 1);
        Scene.ProcessRequest();
        TestEqual(TEXT("Held request does not add a second jump"), Scene.Character->JumpCurrentCount, 1);
    }
    {
        FTestScene Scene;
        Scene.Character->Jump();
        AActor* Ceiling = Scene.Ceiling(0.0f, 10.0f);
        Scene.ProcessRequest();
        TestFalse(TEXT("Ceiling added after request blocks takeoff"), Scene.Movement->IsFalling());
        TestEqual(TEXT("Refusal adds no vertical velocity"), Scene.Movement->Velocity.Z, 0.0);
        TestEqual(TEXT("Refusal consumes no jump count"), Scene.Character->JumpCurrentCount, 0);
        Ceiling->Destroy();
        Scene.ProcessRequest();
        TestFalse(TEXT("Removing ceiling does not replay held request"), Scene.Movement->IsFalling());
        Scene.Character->StopJumping();
        Scene.Character->Jump();
        Scene.ProcessRequest();
        TestTrue(TEXT("Fresh press succeeds after ceiling removal"), Scene.Movement->IsFalling());
        TestEqual(TEXT("Fresh successful press counts once"), Scene.Character->JumpCurrentCount, 1);
    }
    {
        FTestScene Scene;
        Scene.Ceiling(Scene.Radius - 2.0f, 10.0f);
        Scene.Character->Jump();
        Scene.ProcessRequest();
        TestFalse(TEXT("Beam at capsule edge blocks takeoff"), Scene.Movement->IsFalling());
        TestEqual(TEXT("Edge obstruction does not count"), Scene.Character->JumpCurrentCount, 0);
    }
    {
        FTestScene Scene;
        Scene.Ceiling(0.0f, -5.0f);
        Scene.Character->Jump();
        Scene.ProcessRequest();
        TestFalse(TEXT("Initial overhead penetration blocks takeoff"), Scene.Movement->IsFalling());
        TestEqual(TEXT("Initial penetration does not count"), Scene.Character->JumpCurrentCount, 0);
    }
    {
        FTestScene Scene;
        Scene.Character->SetActorScale3D(FVector(0.8));
        Scene.Character->GetCapsuleComponent()->GetScaledCapsuleSize(Scene.Radius, Scene.HalfHeight);
        Scene.Character->SetActorLocation(FVector(0.0, 0.0, Scene.HalfHeight + 2.0), false, nullptr, ETeleportType::TeleportPhysics);
        // Кандидат поднимается на 63.78 см: 67 см над верхом больше высоты и запаса 2 см.
        AActor* Ceiling = Scene.Block(Scene.Character->GetActorLocation() + FVector(0.0, 0.0, Scene.HalfHeight + 72.0), FVector(100.0, 100.0, 5.0));
        Scene.Character->Jump();
        Scene.ProcessRequest();
        TestTrue(TEXT("Scaled capsule accepts ceiling above ascent plus margin"), Scene.Movement->IsFalling());
        Ceiling->Destroy();
    }
    {
        FTestScene Scene;
        Scene.Character->SetActorScale3D(FVector(0.8));
        Scene.Character->GetCapsuleComponent()->GetScaledCapsuleSize(Scene.Radius, Scene.HalfHeight);
        Scene.Character->SetActorLocation(FVector(0.0, 0.0, Scene.HalfHeight + 2.0), false, nullptr, ETeleportType::TeleportPhysics);
        Scene.Block(Scene.Character->GetActorLocation() + FVector(0.0, 0.0, Scene.HalfHeight + 69.0), FVector(100.0, 100.0, 5.0));
        Scene.Character->Jump();
        Scene.ProcessRequest();
        TestFalse(TEXT("Scaled capsule refuses ceiling within ascent margin"), Scene.Movement->IsFalling());
    }
    {
        FTestScene Scene;
        Scene.Movement->GravityScale = 5e-8f;
        Scene.Ceiling(0.0f, 10.0f);
        Scene.Character->Jump();
        Scene.ProcessRequest();
        TestFalse(TEXT("Near-zero gravity cannot shrink clearance to zero"), Scene.Movement->IsFalling());
        TestEqual(TEXT("Invalid ascent consumes no count"), Scene.Character->JumpCurrentCount, 0);
    }
    {
        FTestScene Scene;
        Scene.Movement->Velocity.Z = 1000.0;
        Scene.Ceiling(0.0f, 100.0f);
        Scene.Character->Jump();
        Scene.ProcessRequest();
        TestFalse(TEXT("Clearance includes existing upward velocity"), Scene.Movement->IsFalling());
        TestEqual(TEXT("Refusal preserves external velocity"), Scene.Movement->Velocity.Z, 1000.0);
    }
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSandsJumpFlightInteractionTest,
    "SandsOfSorrow.Jump.FlightAndInteraction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Проверяет движение компонентом UE и взаимодействие с настоящей целью на всём пути прыжка.
bool FSandsJumpFlightInteractionTest::RunTest(const FString& Parameters)
{
    using namespace SandsJumpTests;
    FTestScene Scene;
    APlayerController* Controller = Scene.World->SpawnActor<APlayerController>();
    Controller->Player = NewObject<ULocalPlayer>(GEngine);
    // Нативный тестовый Pawn получает корректные временные действия перед обычным SetupPlayerInputComponent.
    for (const FName Name : {FName(TEXT("MoveAction")), FName(TEXT("LookAction")), FName(TEXT("JumpAction")), FName(TEXT("InteractAction")), FName(TEXT("CrouchAction"))})
    {
        UInputAction* Action = NewObject<UInputAction>(Scene.Character);
        Action->ValueType = Name == TEXT("MoveAction") || Name == TEXT("LookAction")
            ? EInputActionValueType::Axis2D : EInputActionValueType::Boolean;
        FObjectProperty* Property = FindFProperty<FObjectProperty>(ASandsOfSorrowPlayerCharacter::StaticClass(), Name);
        if (!TestNotNull(TEXT("Input action field exists"), Property))
        {
            return false;
        }
        Property->SetObjectPropertyValue_InContainer(Scene.Character, Action);
    }
    Controller->Possess(Scene.Character);
    ACameraActor* Camera = Scene.World->SpawnActor<ACameraActor>(FVector(0.0, 0.0, 150.0), FRotator::ZeroRotator);
    Controller->SetViewTarget(Camera);
    ASandsOfSorrowReflector* Reflector = Scene.World->SpawnActor<ASandsOfSorrowReflector>(FVector(200.0, 0.0, 0.0), FRotator::ZeroRotator);
    USandsOfSorrowInteractionComponent* Interaction = Scene.Character->GetInteractionComponent();
    Interaction->RefreshFocus();
    if (!TestEqual(TEXT("Ground focus exists"), Interaction->GetFocusedActor(), static_cast<AActor*>(Reflector)))
    {
        Controller->UnPossess();
        return false;
    }

    Scene.Movement->AirControl = 0.1f;
    Scene.Movement->AirControlBoostMultiplier = 1.0f;
    Scene.Movement->FallingLateralFriction = 0.0f;
    Scene.Movement->BrakingDecelerationFalling = 0.0f;
    Scene.Movement->Velocity = FVector(100.0, 0.0, 0.0);
    const double StartZ = Scene.Character->GetActorLocation().Z;
    Scene.Character->Jump();
    Scene.ProcessRequest();
    TestTrue(TEXT("Actual jump starts flight"), Scene.Movement->IsFalling());
    TestEqual(TEXT("Takeoff preserves horizontal velocity"), Scene.Movement->Velocity.X, 100.0);
    bool bAscent = false;
    bool bApex = false;
    bool bDescent = false;
    bool bInertiaPreserved = false;
    double HighestZ = StartZ;
    float FlightTime = 0.0f;
    const float Step = 1.0f / 120.0f;
    for (int32 Index = 0; Index < 240 && Scene.Movement->IsFalling(); ++Index)
    {
        const double VerticalVelocity = Scene.Movement->Velocity.Z;
        const bool bSampleAscent = !bAscent && VerticalVelocity > 200.0;
        const bool bSampleApex = !bApex && FMath::Abs(VerticalVelocity) < 30.0;
        const bool bSampleDescent = !bDescent && VerticalVelocity < -200.0;
        if (bSampleAscent || bSampleApex || bSampleDescent)
        {
            Interaction->RefreshFocus();
            TestNull(TEXT("Flight clears focus"), Interaction->GetFocusedActor());
            TestFalse(TEXT("Flight rejects direct request"), Interaction->RequestInteraction());
            TestEqual(TEXT("Flight never rotates target"), Reflector->GetRotationIndex(), 0);
            bAscent |= bSampleAscent;
            bApex |= bSampleApex;
            bDescent |= bSampleDescent;
            if (bSampleDescent)
            {
                bInertiaPreserved = Scene.Movement->Velocity.X > 90.0;
            }
        }
        // Публичный TickComponent использует обычный ControlledCharacterMove и физику движения.
        Scene.Movement->TickComponent(Step, LEVELTICK_All, &Scene.Movement->PrimaryComponentTick);
        FlightTime += Step;
        HighestZ = FMath::Max(HighestZ, Scene.Character->GetActorLocation().Z);
    }
    TestTrue(TEXT("Ascent, apex and descent sampled"), bAscent && bApex && bDescent);
    TestFalse(TEXT("Character lands"), Scene.Movement->IsFalling());
    TestTrue(TEXT("Measured apex is near candidate 64 cm"), HighestZ - StartZ > 55.0 && HighestZ - StartZ < 72.0);
    TestTrue(TEXT("Measured airtime is near candidate 0.51 s"), FlightTime > 0.43f && FlightTime < 0.60f);
    TestTrue(TEXT("Released-input flight retains horizontal inertia"), bInertiaPreserved);
    TestEqual(TEXT("No airborne request is replayed on landing"), Reflector->GetRotationIndex(), 0);
    Interaction->RefreshFocus();
    TestEqual(TEXT("Landing restores focus"), Interaction->GetFocusedActor(), static_cast<AActor*>(Reflector));
    TestTrue(TEXT("Fresh grounded request succeeds"), Interaction->RequestInteraction());
    TestEqual(TEXT("Fresh request rotates exactly once"), Reflector->GetRotationIndex(), 1);
    // Воздушная попытка прямо перед посадкой также потребляется без отложенного отрыва.
    Scene.Character->Jump();
    Scene.ProcessRequest();
    TestTrue(TEXT("New grounded press starts next jump"), Scene.Movement->IsFalling());
    bool bTriedNearLanding = false;
    for (int32 Index = 0; Index < 120 && Scene.Movement->IsFalling(); ++Index)
    {
        if (!bTriedNearLanding && Scene.Movement->Velocity.Z < -300.0)
        {
            Scene.Character->Jump();
            bTriedNearLanding = true;
        }
        Scene.Movement->TickComponent(Step, LEVELTICK_All, &Scene.Movement->PrimaryComponentTick);
    }
    TestTrue(TEXT("Near-landing press actually attempted"), bTriedNearLanding);
    Scene.Movement->TickComponent(Step, LEVELTICK_All, &Scene.Movement->PrimaryComponentTick);
    TestTrue(TEXT("No buffered bounce after landing"), Scene.Movement->IsMovingOnGround());
    // В прямом тике без PlayerInput задаём инерционное движение, чтобы изолировать сход с опоры.
    Scene.Movement->GroundFriction = 0.0f;
    Scene.Movement->BrakingDecelerationWalking = 0.0f;
    Scene.Movement->Velocity = FVector(500.0, 0.0, 0.0);
    // Выход за край настоящей опоры без Jump проверяет отдельно выбранное правило падения.
    Scene.Character->SetActorLocation(FVector(990.0, 500.0, Scene.HalfHeight + 2.0), false, nullptr, ETeleportType::TeleportPhysics);
    Scene.Movement->bForceNextFloorCheck = true;
    for (int32 Index = 0; Index < 120 && !Scene.Movement->IsFalling(); ++Index)
    {
        Scene.Character->AddMovementInput(FVector::ForwardVector);
        Scene.Movement->TickComponent(Step, LEVELTICK_All, &Scene.Movement->PrimaryComponentTick);
    }
    AddInfo(FString::Printf(TEXT("Walk-off: position=%s velocity=%s mode=%d"), *Scene.Character->GetActorLocation().ToString(), *Scene.Movement->Velocity.ToString(), static_cast<int32>(Scene.Movement->MovementMode)));
    TestTrue(TEXT("Walking off actual floor enters falling without jump"), Scene.Movement->IsFalling());
    TestFalse(TEXT("Walk-off falling blocks character interaction"), Scene.Character->IsGameplayInteractionAllowed());
    TestFalse(TEXT("Walk-off direct request cannot act"), Interaction->RequestInteraction());
    Controller->UnPossess();
    return true;
}

#endif
