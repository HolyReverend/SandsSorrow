#include "Puzzles/Light/SandsOfSorrowLightReceiver.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/PointLightComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Puzzles/Light/SandsOfSorrowLightCollision.h"
#include "UObject/ConstructorHelpers.h"

// Тонкая плоскость перед корпусом принимает свет, не влияя на выбор объектов через Visibility.
ASandsOfSorrowLightReceiver::ASandsOfSorrowLightReceiver()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(SceneRoot);
	BodyMesh->SetRelativeLocation(FVector(0, 0, 100));
	BodyMesh->SetRelativeScale3D(FVector(0.12, 1, 1));
	BodyMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	WorkingSurface = CreateDefaultSubobject<UBoxComponent>(TEXT("WorkingSurface"));
	WorkingSurface->SetupAttachment(SceneRoot);
	WorkingSurface->SetRelativeLocation(FVector(6.1, 0, 100));
	WorkingSurface->SetBoxExtent(FVector(0.1, 48, 48));
	WorkingSurface->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	WorkingSurface->SetCollisionResponseToAllChannels(ECR_Ignore);
	WorkingSurface->SetCollisionResponseToChannel(SandsOfSorrowLightCollision::Channel, ECR_Block);
	WorkingSurface->SetGenerateOverlapEvents(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	IndicatorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("IndicatorMesh"));
	IndicatorMesh->SetupAttachment(SceneRoot);
	IndicatorMesh->SetRelativeLocation(FVector(13.0, 0.0, 100.0));
	IndicatorMesh->SetRelativeScale3D(FVector(0.04, 0.18, 0.18));
	IndicatorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	IndicatorLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("IndicatorLight"));
	IndicatorLight->SetupAttachment(SceneRoot);
	IndicatorLight->SetRelativeLocation(FVector(20.0, 0.0, 100.0));
	IndicatorLight->SetIntensity(900.0f);
	IndicatorLight->SetAttenuationRadius(180.0f);
	IndicatorLight->SetVisibility(false);
	if (Cube.Succeeded())
	{
		BodyMesh->SetStaticMesh(Cube.Object);
		IndicatorMesh->SetStaticMesh(Cube.Object);
	}
}

FVector ASandsOfSorrowLightReceiver::GetFaceNormal() const
{
	return WorkingSurface->GetForwardVector();
}

bool ASandsOfSorrowLightReceiver::HasSupportedLightTransform() const
{
	return SandsOfSorrowLightCollision::HasSupportedTransform(WorkingSurface);
}

bool ASandsOfSorrowLightReceiver::IsWorkingFaceHit(const FHitResult& Hit, const FVector& IncomingDirection) const
{
	return SandsOfSorrowLightCollision::IsFrontFaceHit(WorkingSurface, Hit, IncomingDirection);
}

// Изменение возвращает факт перехода; повторное применение не вызывает событие.
bool ASandsOfSorrowLightReceiver::SetIlluminatedSilently(bool bNewValue)
{
	if (bIsIlluminated == bNewValue)
	{
		return false;
	}
	bIsIlluminated = bNewValue;
	// Размер и свет дают два независимых от цвета признака состояния.
	IndicatorMesh->SetRelativeScale3D(bNewValue ? FVector(0.04, 0.38, 0.38) : FVector(0.04, 0.18, 0.18));
	IndicatorLight->SetVisibility(bNewValue);
	if (UMaterialInterface* Material = bNewValue ? ActiveMaterial.Get() : InactiveMaterial.Get())
	{
		IndicatorMesh->SetMaterial(0, Material);
	}
	return true;
}

void ASandsOfSorrowLightReceiver::NotifyIlluminationChanged()
{
	if (TransitionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, TransitionSound, GetActorLocation());
	}
	OnIlluminationChanged.Broadcast(bIsIlluminated);
}
