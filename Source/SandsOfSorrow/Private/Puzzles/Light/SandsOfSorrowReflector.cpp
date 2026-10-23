#include "Puzzles/Light/SandsOfSorrowReflector.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "SandsOfSorrow.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "SandsReflector"

ASandsOfSorrowReflector::ASandsOfSorrowReflector()
{
	PrimaryActorTick.bCanEverTick = false;
	InteractionText = LOCTEXT("Rotate", "Повернуть отражатель");
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
	BaseMesh->SetupAttachment(SceneRoot);
	BaseMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 40.0f));
	BaseMesh->SetRelativeScale3D(FVector(0.6f, 0.6f, 0.8f));
	BaseMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	HeadPivot = CreateDefaultSubobject<USceneComponent>(TEXT("HeadPivot"));
	HeadPivot->SetupAttachment(SceneRoot);
	HeadPivot->SetRelativeLocation(FVector(0.0f, 0.0f, 150.0f));
	HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMesh"));
	HeadMesh->SetupAttachment(HeadPivot);
	HeadMesh->SetRelativeScale3D(FVector(0.12f, 1.0f, 1.0f));
	// The moving head is selectable but cannot sweep into or trap a character.
	HeadMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HeadMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	HeadMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	NormalMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NormalMarker"));
	NormalMarker->SetupAttachment(HeadPivot);
	NormalMarker->SetRelativeLocation(FVector(40.0f, 0.0f, 0.0f));
	NormalMarker->SetRelativeScale3D(FVector(0.6f, 0.08f, 0.08f));
	NormalMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NormalMarkerTip = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NormalMarkerTip"));
	NormalMarkerTip->SetupAttachment(HeadPivot);
	NormalMarkerTip->SetRelativeLocation(FVector(75.0f, 0.0f, 0.0f));
	NormalMarkerTip->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	NormalMarkerTip->SetRelativeScale3D(FVector(0.2f, 0.2f, 0.3f));
	NormalMarkerTip->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (Cube.Succeeded())
	{
		BaseMesh->SetStaticMesh(Cube.Object);
		HeadMesh->SetStaticMesh(Cube.Object);
		NormalMarker->SetStaticMesh(Cube.Object);
	}
	if (Cone.Succeeded())
	{
		NormalMarkerTip->SetStaticMesh(Cone.Object);
	}
	ApplyHeadRotation();
}

void ASandsOfSorrowReflector::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyHeadRotation();
}

void ASandsOfSorrowReflector::ApplyHeadRotation()
{
	const FQuat StepRotation(FVector::UpVector, FMath::DegreesToRadians(90.0f * RotationIndex));
	HeadPivot->SetRelativeRotation(InitialHeadRotation.Quaternion() * StepRotation);
}

FVector ASandsOfSorrowReflector::GetFaceNormal() const
{
	return HeadPivot->GetForwardVector();
}

bool ASandsOfSorrowReflector::CanInteract_Implementation(AActor* Interactor) const
{
	return bInteractionEnabled && IsValid(Interactor) && Interactor != this;
}

FText ASandsOfSorrowReflector::GetInteractionText_Implementation(AActor* Interactor) const
{
	return InteractionText;
}

bool ASandsOfSorrowReflector::Interact_Implementation(AActor* Interactor)
{
	if (!CanInteract_Implementation(Interactor))
	{
		return false;
	}
	RotationIndex = (RotationIndex + 1) % 4;
	ApplyHeadRotation();
	UE_LOG(LogSandsOfSorrow, Display, TEXT("%s: reflector orientation %d/4, normal %s"),
		*GetNameSafe(this), RotationIndex, *GetFaceNormal().ToCompactString());
	return true;
}

#undef LOCTEXT_NAMESPACE
