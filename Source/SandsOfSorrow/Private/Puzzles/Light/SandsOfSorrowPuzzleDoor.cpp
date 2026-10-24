#include "Puzzles/Light/SandsOfSorrowPuzzleDoor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

// Коллизия является корнем: sweep проверяет весь путь створки, а не только её меш.
ASandsOfSorrowPuzzleDoor::ASandsOfSorrowPuzzleDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	CollisionRoot = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionRoot"));
	SetRootComponent(CollisionRoot);
	CollisionRoot->SetMobility(EComponentMobility::Movable);
	CollisionRoot->SetBoxExtent(FVector(12.0, 60.0, 120.0));
	CollisionRoot->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(CollisionRoot);
	DoorMesh->SetRelativeScale3D(FVector(0.24, 1.2, 2.4));
	DoorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		DoorMesh->SetStaticMesh(Cube.Object);
	}
}

void ASandsOfSorrowPuzzleDoor::BeginPlay()
{
	Super::BeginPlay();
	ClosedWorldLocation = GetActorLocation();
	SetActorTickEnabled(false);
}

// Принимает только одностороннее открытие; некорректная настройка не запускает движение.
void ASandsOfSorrowPuzzleDoor::SetOpen(bool bOpen)
{
	if (!bOpen || bOpenRequested || bFullyOpen || !HasActorBegunPlay()
		|| !FMath::IsFinite(OpenDuration) || OpenDuration < 0.0f || OpenOffset.ContainsNaN())
	{
		return;
	}
	bOpenRequested = true;
	SetActorTickEnabled(true);
	AdvanceOpening(0.0f);
}

void ASandsOfSorrowPuzzleDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AdvanceOpening(DeltaSeconds);
}

// Sweep останавливает створку у преграды; следующий кадр повторяет попытку без потери цели.
void ASandsOfSorrowPuzzleDoor::AdvanceOpening(float DeltaSeconds)
{
	if (!bOpenRequested || bFullyOpen)
	{
		return;
	}
	const FVector Goal = ClosedWorldLocation + GetActorQuat().RotateVector(OpenOffset);
	const double Distance = FVector::Distance(ClosedWorldLocation, Goal);
	const double Speed = OpenDuration <= 0.0f ? Distance : Distance / OpenDuration;
	const FVector Next = OpenDuration <= 0.0f ? Goal
		: FMath::VInterpConstantTo(GetActorLocation(), Goal, FMath::Max(0.0f, DeltaSeconds), Speed);
	FHitResult Hit;
	SetActorLocation(Next, true, &Hit);
	if (FVector::DistSquared(GetActorLocation(), Goal) > FMath::Square(0.1))
	{
		return;
	}
	// Доступный проход подтверждается только реальным конечным положением створки.
	bFullyOpen = true;
	SetActorTickEnabled(false);
	OnOpened.Broadcast();
}
