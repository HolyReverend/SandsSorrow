#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Characters/SandsOfSorrowPlayerCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Puzzles/Light/SandsOfSorrowLightCollision.h"
#include "Puzzles/Light/SandsOfSorrowReflector.h"

namespace SandsLightSurfaceTests
{
// Изолированный мир с реальной физической сценой: проверки не подменяют попадания флагами.
struct FLightSurfaceTestScene
{
	UWorld* World = nullptr;
	ASandsOfSorrowReflector* Reflector = nullptr;

	FLightSurfaceTestScene()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
		Reflector = World->SpawnActor<ASandsOfSorrowReflector>();
	}

	~FLightSurfaceTestScene()
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	}

	// Одинаковый запрос использует проектный канал, без игнорирования посещённых устройств.
	FHitResult Trace(const FVector& Start, const FVector& End, ECollisionChannel Channel = SandsOfSorrowLightCollision::Channel)
	{
		FHitResult Hit;
		World->LineTraceSingleByChannel(Hit, Start, End, Channel);
		return Hit;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSandsLightSurfaceCollisionTest,
	"SandsOfSorrow.LightPuzzle.SurfaceCollision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Проверяет ближайшую блокировку, лицо/тыл/боковину, проникновение и сохранность Visibility.
bool FSandsLightSurfaceCollisionTest::RunTest(const FString& Parameters)
{
	using namespace SandsLightSurfaceTests;
	TestEqual(TEXT("Configured channel name"), UCollisionProfile::Get()->ReturnChannelNameFromContainerIndex(SandsOfSorrowLightCollision::Channel), FName(TEXT("PuzzleLight")));
	FLightSurfaceTestScene Scene;
	UBoxComponent* Surface = Scene.Reflector->GetWorkingSurface();
	const FVector Center = Surface->GetComponentLocation();
	const FVector Normal = Scene.Reflector->GetFaceNormal();
	const FVector Side = Surface->GetRightVector();
	FHitResult Hit = Scene.Trace(Center + Normal * 100.0f, Center - Normal * 30.0f);
	TestTrue(TEXT("Front trace blocks"), Hit.bBlockingHit);
	TestEqual(TEXT("Front reaches working component before head"), Hit.GetComponent(), static_cast<UPrimitiveComponent*>(Surface));
	TestTrue(TEXT("Front hit coordinate"), Hit.ImpactPoint.Equals(Center + Normal * 0.1f, 0.01f));
	TestTrue(TEXT("Front face accepted"), Scene.Reflector->IsWorkingFaceHit(Hit, -Normal));
	TestFalse(TEXT("Zero direction rejected"), Scene.Reflector->IsWorkingFaceHit(Hit, FVector::ZeroVector));
	TestFalse(TEXT("Outgoing direction rejected"), Scene.Reflector->IsWorkingFaceHit(Hit, Normal));

	Hit = Scene.Trace(Center - Normal * 100.0f, Center + Normal * 100.0f);
	TestTrue(TEXT("Back blocks at body"), Hit.bBlockingHit);
	TestFalse(TEXT("Back is not a working face"), Scene.Reflector->IsWorkingFaceHit(Hit, Normal));
	Hit = Scene.Trace(Center + Side * 60.0f, Center - Side * 60.0f);
	TestEqual(TEXT("Side hits the thin working box"), Hit.GetComponent(), static_cast<UPrimitiveComponent*>(Surface));
	TestFalse(TEXT("Side is not reflective"), Scene.Reflector->IsWorkingFaceHit(Hit, -Side));
	// Луч действительно достигает передней грани под почти касательным углом.
	const FVector Grazing = (-Normal * 0.0005f + Side).GetSafeNormal();
	const FVector GrazingStart = Center + Normal * 0.105f - Side * 30.0f;
	Hit = Scene.Trace(GrazingStart, GrazingStart + Grazing * 20.0f);
	TestEqual(TEXT("Grazing reaches working component"), Hit.GetComponent(), static_cast<UPrimitiveComponent*>(Surface));
	TestFalse(TEXT("Grazing is not reflective"), Scene.Reflector->IsWorkingFaceHit(Hit, Grazing));
	Hit = Scene.Trace(Center, Center + Normal * 100.0f);
	TestTrue(TEXT("Start inside is detected"), Hit.bStartPenetrating);
	TestFalse(TEXT("Penetration rejected"), Scene.Reflector->IsWorkingFaceHit(Hit, Normal));
	Hit = Scene.Trace(Center + Normal * 100.0f, Center - Normal * 30.0f, ECC_Visibility);
	TestTrue(TEXT("Interaction still sees reflector"), Hit.GetActor() == Scene.Reflector);
	TestTrue(TEXT("Working surface does not intercept interaction"), Hit.GetComponent() != Surface);
	const FVector BaseCenter(0.0f, 0.0f, 40.0f);
	Hit = Scene.Trace(BaseCenter + FVector(100, 0, 0), BaseCenter - FVector(100, 0, 0));
	TestTrue(TEXT("Base blocks"), Hit.bBlockingHit);
	TestFalse(TEXT("Base never reflects"), Scene.Reflector->IsWorkingFaceHit(Hit, FVector(-1, 0, 0)));

	// Новый обычный профиль должен блокировать PuzzleLight без специальной настройки канала.
	AActor* Wall = Scene.World->SpawnActor<AActor>();
	UBoxComponent* Box = NewObject<UBoxComponent>(Wall);
	Wall->SetRootComponent(Box);
	Box->SetBoxExtent(FVector(5.0f));
	Box->SetCollisionProfileName(TEXT("BlockAll"));
	Box->RegisterComponent();
	Wall->SetActorLocation(Center + Normal * 50.0f);
	Hit = Scene.Trace(Center + Normal * 100.0f, Center - Normal * 30.0f);
	TestEqual(TEXT("Wall before mirror wins first hit"), Hit.GetActor(), Wall);
	TestFalse(TEXT("Wall cannot activate mirror"), Scene.Reflector->IsWorkingFaceHit(Hit, -Normal));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSandsLightSurfaceTransformTest,
	"SandsOfSorrow.LightPuzzle.SurfaceTransform",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Проверяет мировую нормаль и реальные попадания после поворота Actor, шагов головки и масштаба.
bool FSandsLightSurfaceTransformTest::RunTest(const FString& Parameters)
{
	using namespace SandsLightSurfaceTests;
	FLightSurfaceTestScene Scene;
	AActor* Interactor = Scene.World->SpawnActor<AActor>();
	const FQuat ActorRotation = FRotator(20.0f, 37.0f, 15.0f).Quaternion();
	Scene.Reflector->SetActorRotation(ActorRotation);
	Scene.Reflector->SetActorScale3D(FVector(2.0f));
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const FVector Expected = ActorRotation.RotateVector(FRotator(0, 45.0f + Index * 90.0f, 0).Vector());
		const FVector Normal = Scene.Reflector->GetFaceNormal();
		TestTrue(TEXT("World normal includes tilted Actor"), Normal.Equals(Expected, 0.0001f));
		const FVector Center = Scene.Reflector->GetWorkingSurface()->GetComponentLocation();
		const FHitResult Hit = Scene.Trace(Center + Normal * 200.0f, Center - Normal * 50.0f);
		TestTrue(TEXT("Uniform positive scale is supported"), Scene.Reflector->HasSupportedLightTransform());
		TestTrue(TEXT("Transformed face accepts actual hit"), Scene.Reflector->IsWorkingFaceHit(Hit, -Normal));
		TestTrue(TEXT("Each interaction advances head"), Scene.Reflector->Interact_Implementation(Interactor));
	}
	TestEqual(TEXT("Four steps restore index"), Scene.Reflector->GetRotationIndex(), 0);
	Scene.Reflector->SetActorScale3D(FVector(1, 2, 1));
	TestFalse(TEXT("Nonuniform scale rejected"), Scene.Reflector->HasSupportedLightTransform());
	Scene.Reflector->SetActorScale3D(FVector(-1, 1, 1));
	TestFalse(TEXT("Mirrored scale rejected"), Scene.Reflector->HasSupportedLightTransform());
	Scene.Reflector->SetActorScale3D(FVector::ZeroVector);
	TestFalse(TEXT("Zero scale rejected"), Scene.Reflector->HasSupportedLightTransform());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSandsLightCharacterIgnoreTest,
	"SandsOfSorrow.LightPuzzle.CharacterIgnore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Компонент, собранный до PostInitializeComponents, моделирует дополнительную геометрию BP.
bool FSandsLightCharacterIgnoreTest::RunTest(const FString& Parameters)
{
	using namespace SandsLightSurfaceTests;
	FLightSurfaceTestScene Scene;
	const FTransform Transform(FVector(500, 0, 100));
	ASandsOfSorrowPlayerCharacter* Player = Scene.World->SpawnActorDeferred<ASandsOfSorrowPlayerCharacter>(
		ASandsOfSorrowPlayerCharacter::StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	UBoxComponent* Extra = NewObject<UBoxComponent>(Player);
	Extra->SetupAttachment(Player->GetRootComponent());
	Extra->SetBoxExtent(FVector(30));
	Extra->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Extra->RegisterComponent();
	UGameplayStatics::FinishSpawningActor(Player, Transform);
	TestEqual(TEXT("Capsule ignores light"), Player->GetCapsuleComponent()->GetCollisionResponseToChannel(SandsOfSorrowLightCollision::Channel), ECR_Ignore);
	TestEqual(TEXT("Mesh ignores light"), Player->GetMesh()->GetCollisionResponseToChannel(SandsOfSorrowLightCollision::Channel), ECR_Ignore);
	TestEqual(TEXT("Additional BP component ignores light"), Extra->GetCollisionResponseToChannel(SandsOfSorrowLightCollision::Channel), ECR_Ignore);
	const FHitResult Hit = Scene.Trace(FVector(300, 0, 100), FVector(700, 0, 100));
	TestFalse(TEXT("Light passes through player"), Hit.bBlockingHit);
	return true;
}

#endif
