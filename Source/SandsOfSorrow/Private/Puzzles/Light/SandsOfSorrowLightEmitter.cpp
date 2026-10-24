#include "Puzzles/Light/SandsOfSorrowLightEmitter.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Puzzles/Light/SandsOfSorrowLightCollision.h"
#include "UObject/ConstructorHelpers.h"

// Временный корпус блокирует луч при возврате, но не закрывает выходную точку +X.
ASandsOfSorrowLightEmitter::ASandsOfSorrowLightEmitter()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(SceneRoot);
	BodyMesh->SetRelativeLocation(FVector(0, 0, 100));
	BodyMesh->SetRelativeScale3D(FVector(0.6));
	BodyMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	BeamOrigin = CreateDefaultSubobject<USceneComponent>(TEXT("BeamOrigin"));
	BeamOrigin->SetupAttachment(SceneRoot);
	BeamOrigin->SetRelativeLocation(FVector(40, 0, 100));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		BodyMesh->SetStaticMesh(Cube.Object);
	}
}

FVector ASandsOfSorrowLightEmitter::GetBeamOrigin() const
{
	return BeamOrigin->GetComponentLocation();
}

FVector ASandsOfSorrowLightEmitter::GetBeamDirection() const
{
	return BeamOrigin->GetForwardVector();
}

bool ASandsOfSorrowLightEmitter::HasValidBeamTransform() const
{
	return SandsOfSorrowLightCollision::HasSupportedTransform(BeamOrigin)
		&& !GetBeamOrigin().ContainsNaN() && !GetBeamDirection().ContainsNaN()
		&& !GetBeamDirection().IsNearlyZero();
}
