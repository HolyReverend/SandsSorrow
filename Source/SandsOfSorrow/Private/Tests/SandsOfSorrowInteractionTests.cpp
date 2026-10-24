#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Camera/CameraActor.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/SandsOfSorrowInteractionComponent.h"
#include "Puzzles/Light/SandsOfSorrowReflector.h"

namespace SandsInteractionTests
{
// Изолированный игровой мир с реальными акторами и трассировками, без зависимости от карты.
struct FTestScene
{
	UWorld* World = nullptr;
	APawn* Pawn = nullptr;
	APlayerController* Controller = nullptr;
	ACameraActor* Camera = nullptr;
	USandsOfSorrowInteractionComponent* Interaction = nullptr;
	ASandsOfSorrowReflector* First = nullptr;
	ASandsOfSorrowReflector* Second = nullptr;

	// Создаёт локального владельца, отдельную камеру и два устройства для проверки смены цели.
	FTestScene()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
		Pawn = World->SpawnActor<APawn>();
		UBoxComponent* Root = NewObject<UBoxComponent>(Pawn);
		Pawn->SetRootComponent(Root);
		Root->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Root->RegisterComponent();
		Pawn->SetActorLocation(FVector(0.0f, 0.0f, 98.0f));
		Pawn->BaseEyeHeight = 64.0f;
		Controller = World->SpawnActor<APlayerController>();
		Controller->Player = NewObject<ULocalPlayer>(GEngine);
		Controller->Possess(Pawn);
		Interaction = NewObject<USandsOfSorrowInteractionComponent>(Pawn);
		Interaction->RegisterComponent();
		Camera = World->SpawnActor<ACameraActor>(FVector(0.0f, 0.0f, 150.0f), FRotator::ZeroRotator);
		Controller->SetViewTarget(Camera);
		First = World->SpawnActor<ASandsOfSorrowReflector>(FVector(200.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
		Second = World->SpawnActor<ASandsOfSorrowReflector>(FVector(170.0f, 150.0f, 0.0f), FRotator::ZeroRotator);
	}

	// Освобождает владение и контекст мира после каждого теста.
	~FTestScene()
	{
		Controller->UnPossess();
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	}

	// Снимок контекста и первого попадания помогает разобрать отказ теста.
	FString Describe() const
	{
		FVector View; FRotator Rotation;
		Controller->GetPlayerViewPoint(View, Rotation);
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(SandsInteractionTest), false, Pawn);
		const bool bHit = World->LineTraceSingleByChannel(Hit, View, View + Rotation.Vector() * 1000.0f, ECC_Visibility, Params);
		return FString::Printf(TEXT("Test scene: active=%d local=%d possessed=%d ignoreMove=%d view=%s rotation=%s hit=%d actor=%s point=%s"),
			Interaction->IsActive(), Pawn->IsLocallyControlled(), Controller->GetPawn() == Pawn, Controller->IsMoveInputIgnored(),
			*View.ToCompactString(), *Rotation.ToCompactString(), bHit, *GetNameSafe(Hit.GetActor()), *Hit.ImpactPoint.ToCompactString());
	}

	// Меняет направление камеры независимо от положения тестового Pawn.
	void AimAt(const FVector& Point)
	{
		Camera->SetActorRotation((Point - Camera->GetActorLocation()).Rotation());
	}

	// Добавляет препятствие только для Visibility, не вмешиваясь в движение.
	AActor* Block(const FVector& Center, const FVector& Extent)
	{
		AActor* Wall = World->SpawnActor<AActor>();
		UBoxComponent* Box = NewObject<UBoxComponent>(Wall);
		Wall->SetRootComponent(Box);
		Box->SetBoxExtent(Extent);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Box->SetCollisionResponseToAllChannels(ECR_Ignore);
		Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Box->RegisterComponent();
		Wall->SetActorLocation(Center);
		return Wall;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSandsInteractionRevalidationTest,
	"SandsOfSorrow.Interaction.RequestRevalidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Проверяет отказ при устаревшем фокусе: запрет, новая стена, смена или уничтожение цели.
bool FSandsInteractionRevalidationTest::RunTest(const FString& Parameters)
{
	using namespace SandsInteractionTests;
	FTestScene Scene;
	AddInfo(Scene.Describe());
	Scene.Interaction->RefreshFocus();
	if (!TestEqual(TEXT("Nearby first blocking reflector is focused"), Scene.Interaction->GetFocusedActor(), static_cast<AActor*>(Scene.First)))
	{
		return false;
	}
	TestTrue(TEXT("Valid request succeeds"), Scene.Interaction->RequestInteraction());
	TestEqual(TEXT("Valid request changes the actual device"), Scene.First->GetRotationIndex(), 1);

	Scene.First->bInteractionEnabled = false;
	TestFalse(TEXT("Availability changed after focus rejects request"), Scene.Interaction->RequestInteraction());
	TestEqual(TEXT("Disabled device state unchanged"), Scene.First->GetRotationIndex(), 1);
	TestNull(TEXT("Disabled focus cleared"), Scene.Interaction->GetFocusedActor());
	Scene.First->bInteractionEnabled = true;
	Scene.Interaction->RefreshFocus();

	AActor* Wall = Scene.Block(FVector(100.0f, 0.0f, 150.0f), FVector(5.0f, 30.0f, 40.0f));
	TestFalse(TEXT("New obstruction rejects request at allowed distance"), Scene.Interaction->RequestInteraction());
	TestEqual(TEXT("Obstructed device state unchanged"), Scene.First->GetRotationIndex(), 1);
	Wall->Destroy();
	Scene.Interaction->RefreshFocus();

	Scene.AimAt(FVector(170.0f, 150.0f, 150.0f));
	TestFalse(TEXT("Target change cannot activate the neighbor on this press"), Scene.Interaction->RequestInteraction());
	TestEqual(TEXT("Previous device state unchanged"), Scene.First->GetRotationIndex(), 1);
	TestEqual(TEXT("New device state unchanged"), Scene.Second->GetRotationIndex(), 0);
	TestEqual(TEXT("New focus is available for the next press"), Scene.Interaction->GetFocusedActor(), static_cast<AActor*>(Scene.Second));
	TestTrue(TEXT("Next press activates that device"), Scene.Interaction->RequestInteraction());
	TestEqual(TEXT("Neighbor changed only on next press"), Scene.Second->GetRotationIndex(), 1);

	Scene.Second->Destroy();
	TestFalse(TEXT("Destroyed focused actor rejects request safely"), Scene.Interaction->RequestInteraction());
	TestNull(TEXT("Destroyed focus cleared"), Scene.Interaction->GetFocusedActor());
	TestEqual(TEXT("Other device state unchanged by destroyed request"), Scene.First->GetRotationIndex(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSandsInteractionReachTest,
	"SandsOfSorrow.Interaction.ReachAndBodyVisibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Проверяет расстояние от персонажа и запрет взаимодействия камерой из-за угла.
bool FSandsInteractionReachTest::RunTest(const FString& Parameters)
{
	using namespace SandsInteractionTests;
	FTestScene Scene;
	AddInfo(Scene.Describe());
	Scene.Interaction->RefreshFocus();
	if (!TestEqual(TEXT("Initial focus exists"), Scene.Interaction->GetFocusedActor(), static_cast<AActor*>(Scene.First)))
	{
		return false;
	}
	Scene.Pawn->SetActorLocation(FVector(-400.0f, 0.0f, 98.0f));
	TestFalse(TEXT("Camera reach cannot bypass character distance"), Scene.Interaction->RequestInteraction());
	TestEqual(TEXT("Distant reflector unchanged"), Scene.First->GetRotationIndex(), 0);
	Scene.Pawn->SetActorLocation(FVector(0.0f, 0.0f, 98.0f));
	Scene.Camera->SetActorLocation(FVector(0.0f, 100.0f, 150.0f));
	Scene.AimAt(FVector(200.0f, 0.0f, 150.0f));
	Scene.Interaction->RefreshFocus();
	TestEqual(TEXT("Side camera sees unobstructed target"), Scene.Interaction->GetFocusedActor(), static_cast<AActor*>(Scene.First));
	// Луч камеры проходит при y ≈ 50, луч тела — при y = 0; стена перекрывает только второй.
	Scene.Block(FVector(100.0f, 0.0f, 156.0f), FVector(5.0f, 12.0f, 20.0f));
	TestFalse(TEXT("Camera peeking around corner cannot interact through body obstruction"), Scene.Interaction->RequestInteraction());
	TestEqual(TEXT("Body-obstructed reflector unchanged"), Scene.First->GetRotationIndex(), 0);
	TestNull(TEXT("Body-obstructed focus cleared"), Scene.Interaction->GetFocusedActor());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSandsInteractionFocusLifecycleTest,
	"SandsOfSorrow.Interaction.FocusLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Проверяет обновление текста, сброс фокуса при блокировке и восстановление после активации.
bool FSandsInteractionFocusLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace SandsInteractionTests;
	FTestScene Scene;
	Scene.Interaction->RefreshFocus();
	TestEqual(TEXT("Initial focus"), Scene.Interaction->GetFocusedActor(), static_cast<AActor*>(Scene.First));
	Scene.First->InteractionText = FText::FromString(TEXT("Inspect reflector"));
	Scene.Interaction->RefreshFocus();
	TestEqual(TEXT("Text change preserves the same actor"), Scene.Interaction->GetFocusedActor(), static_cast<AActor*>(Scene.First));
	TestTrue(TEXT("Text refreshes on unchanged actor"), Scene.Interaction->GetFocusedText().EqualTo(Scene.First->InteractionText));

	// Блокировка ввода и деактивация компонента должны одинаково убирать устаревшую подсказку.
	Scene.Interaction->SetInteractionInputEnabled(false);
	TestNull(TEXT("Suspension immediately clears focus"), Scene.Interaction->GetFocusedActor());
	TestTrue(TEXT("Suspension clears text"), Scene.Interaction->GetFocusedText().IsEmpty());
	TestFalse(TEXT("Suspended request rejected"), Scene.Interaction->RequestInteraction());
	TestEqual(TEXT("Suspension leaves device unchanged"), Scene.First->GetRotationIndex(), 0);
	Scene.Interaction->SetInteractionInputEnabled(true);
	TestEqual(TEXT("Resume revalidates focus"), Scene.Interaction->GetFocusedActor(), static_cast<AActor*>(Scene.First));

	Scene.Interaction->Deactivate();
	TestNull(TEXT("Deactivation immediately clears focus"), Scene.Interaction->GetFocusedActor());
	TestFalse(TEXT("Inactive request rejected"), Scene.Interaction->RequestInteraction());
	TestEqual(TEXT("Inactive request leaves device unchanged"), Scene.First->GetRotationIndex(), 0);
	Scene.Interaction->Activate();
	Scene.Interaction->RefreshFocus();
	TestTrue(TEXT("Active current target can act again"), Scene.Interaction->RequestInteraction());
	TestEqual(TEXT("Exactly one accepted state change"), Scene.First->GetRotationIndex(), 1);
	Scene.First->Destroy();
	Scene.Interaction->RefreshFocus();
	TestNull(TEXT("Destroy clears actor"), Scene.Interaction->GetFocusedActor());
	TestTrue(TEXT("Destroy clears action text"), Scene.Interaction->GetFocusedText().IsEmpty());
	return true;
}

#endif
