#include "Puzzles/Light/SandsOfSorrowLightPuzzle.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/PointLight.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "Puzzles/Light/SandsOfSorrowPuzzleDoor.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "Puzzles/Light/SandsOfSorrowLightCollision.h"
#include "Puzzles/Light/SandsOfSorrowLightEmitter.h"
#include "Puzzles/Light/SandsOfSorrowLightReceiver.h"
#include "Puzzles/Light/SandsOfSorrowReflector.h"

namespace
{
// Единый предел для расчёта и числа отображаемых сегментов.
constexpr int32 MaxSupportedReflections = 16;
// Слабые записи не удерживают мир или Actor; регистрация/выгрузка не зависят от BeginPlay.
TArray<TWeakObjectPtr<ASandsOfSorrowLightPuzzle>>& RegisteredPuzzles()
{
	static TArray<TWeakObjectPtr<ASandsOfSorrowLightPuzzle>> Puzzles;
	return Puzzles;
}
}

void ASandsOfSorrowLightPuzzle::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();
	if (GetWorld() && !IsTemplate())
	{
		TArray<TWeakObjectPtr<ASandsOfSorrowLightPuzzle>>& Puzzles = RegisteredPuzzles();
		Puzzles.RemoveAll([](const TWeakObjectPtr<ASandsOfSorrowLightPuzzle>& Entry) { return !Entry.IsValid(); });
		Puzzles.AddUnique(this);
	}
}

void ASandsOfSorrowLightPuzzle::PostUnregisterAllComponents()
{
	RegisteredPuzzles().RemoveAll([this](const TWeakObjectPtr<ASandsOfSorrowLightPuzzle>& Entry) { return !Entry.IsValid() || Entry.Get() == this; });
	Super::PostUnregisterAllComponents();
}

ASandsOfSorrowLightPuzzle::ASandsOfSorrowLightPuzzle()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		BeamMesh = Cylinder.Object;
	}
}

void ASandsOfSorrowLightPuzzle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Одного прохода после обновления геометрии достаточно и для движения препятствий.
	RecalculateNow();
}

bool ASandsOfSorrowLightPuzzle::RecalculateNow()
{
	if (bIsRecalculating || !IsValid(this) || IsActorBeingDestroyed() || !GetWorld())
	{
		return false;
	}
	TGuardValue<bool> Guard(bIsRecalculating, true);
	const FLightBeamTraceResult NewTrace = TraceBeam();
	ASandsOfSorrowLightReceiver* NewReceiver = NewTrace.Termination == ELightBeamTermination::ReceiverHit
		? NewTrace.ReachedReceiver.Get() : nullptr;
	if (!IsValid(NewReceiver))
	{
		NewReceiver = nullptr;
	}
	ASandsOfSorrowLightReceiver* OldReceiver = AppliedReceiver.Get();
	const bool bOldChanged = IsValid(OldReceiver) && OldReceiver != NewReceiver
		&& OldReceiver->SetIlluminatedSilently(false);
	const bool bNewChanged = NewReceiver && NewReceiver->SetIlluminatedSilently(true);
	const bool bJustSolved = !bIsSolved && NewReceiver != nullptr;
	if (bJustSolved)
	{
		bIsSolved = true;
	}
	// Все наблюдатели видят уже согласованные путь, приёмники и факт решения.
	LastTraceResult = NewTrace;
	AppliedReceiver = NewReceiver;
	UpdateBeamVisuals(LastTraceResult);
	const TWeakObjectPtr<ASandsOfSorrowLightPuzzle> Self(this);
	if (bJustSolved)
	{
		ApplySolvedReward();
	}
	// Мгновенно открывшаяся дверь могла уничтожить координатор через OnOpened.
	if (!Self.IsValid() || IsActorBeingDestroyed())
	{
		return true;
	}
	if (bOldChanged && IsValid(OldReceiver))
	{
		OldReceiver->NotifyIlluminationChanged();
	}
	if (!Self.IsValid() || IsActorBeingDestroyed())
	{
		return true;
	}
	if (bNewChanged && IsValid(NewReceiver))
	{
		NewReceiver->NotifyIlluminationChanged();
	}
	if (Self.IsValid() && !IsActorBeingDestroyed() && bJustSolved)
	{
		OnSolved.Broadcast();
	}
	return true;
}

void ASandsOfSorrowLightPuzzle::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Завершение мира не выдаёт переходов игровым слушателям и не оставляет старую цель.
	if (ASandsOfSorrowLightReceiver* OldReceiver = AppliedReceiver.Get())
	{
		if (IsValid(OldReceiver))
		{
			OldReceiver->SetIlluminatedSilently(false);
		}
	}
	AppliedReceiver.Reset();
	LastTraceResult = FLightBeamTraceResult();
	for (UStaticMeshComponent* Segment : BeamSegments)
	{
		if (IsValid(Segment))
		{
			Segment->SetVisibility(false);
		}
	}
	Super::EndPlay(EndPlayReason);
}

FLightBeamTraceResult ASandsOfSorrowLightPuzzle::TraceBeam() const
{
	FLightBeamTraceResult Result;
	const auto Fail = [&Result](const TCHAR* Message)
	{
		Result.Termination = ELightBeamTermination::InvalidConfiguration;
		Result.Diagnostic = Message;
	};
	// Проверяем актуальные ссылки и бюджет до любых запросов в физическую сцену.
	if (!GetWorld() || !IsValid(Emitter) || !IsValid(Receiver)
		|| Emitter->GetWorld() != GetWorld() || Receiver->GetWorld() != GetWorld())
	{
		Fail(TEXT("Assign valid emitter and receiver from this world."));
		return Result;
	}
	if (!FMath::IsFinite(MaxTotalDistance) || MaxTotalDistance <= 0.0
		|| MaxReflections < 0 || MaxReflections > MaxSupportedReflections
		|| !FMath::IsFinite(SurfaceOffset) || SurfaceOffset < 0.001 || SurfaceOffset > 0.1)
	{
		Fail(TEXT("Check distance, offset and reflection budget (0..16)."));
		return Result;
	}
	if (!Emitter->HasValidBeamTransform() || !Receiver->HasSupportedLightTransform())
	{
		Fail(TEXT("Unsupported emitter/receiver transform."));
		return Result;
	}
	TSet<const ASandsOfSorrowReflector*> UniqueReflectors;
	for (const ASandsOfSorrowReflector* Reflector : Reflectors)
	{
		if (!IsValid(Reflector) || Reflector->GetWorld() != GetWorld() || !Reflector->HasSupportedLightTransform())
		{
			Fail(TEXT("Invalid reflector reference or unsupported surface transform."));
			return Result;
		}
		if (UniqueReflectors.Contains(Reflector))
		{
			Fail(TEXT("Duplicate reflector reference."));
			return Result;
		}
		UniqueReflectors.Add(Reflector);
	}
	// Сравниваем явные назначения только зарегистрированных координаторов того же мира.
	// Нет поиска всех Actor, копирования устройств или изменения их владельца в расчёте.
	for (const TWeakObjectPtr<ASandsOfSorrowLightPuzzle>& Entry : RegisteredPuzzles())
	{
		const ASandsOfSorrowLightPuzzle* Other = Entry.Get();
		if (!IsValid(Other) || Other == this || Other->IsActorBeingDestroyed() || Other->GetWorld() != GetWorld())
		{
			continue;
		}
		if (Other->Emitter == Emitter || Other->Receiver == Receiver)
		{
			Fail(TEXT("Emitter or receiver is assigned to another puzzle."));
			return Result;
		}
		for (const ASandsOfSorrowReflector* Reflector : Other->Reflectors)
		{
			if (UniqueReflectors.Contains(Reflector))
			{
				Fail(TEXT("Reflector is assigned to another puzzle."));
				return Result;
			}
		}
	}
	if (!Emitter->bEnabled)
	{
		Result.Termination = ELightBeamTermination::SourceDisabled;
		return Result;
	}

	// Один классификатор используется также для короткой проверки после отражения.
	const auto ClassifyHit = [this, &Result, &Fail](const FHitResult& Hit, const FVector& Direction) -> ASandsOfSorrowReflector*
	{
		if (Hit.bStartPenetrating)
		{
			Fail(TEXT("Beam starts inside blocking collision."));
			return nullptr;
		}
		if (ASandsOfSorrowLightReceiver* HitReceiver = Cast<ASandsOfSorrowLightReceiver>(Hit.GetActor()))
		{
			if (HitReceiver != Receiver)
			{
				Fail(TEXT("Beam hit a receiver not assigned to this puzzle."));
			}
			else if (HitReceiver->IsWorkingFaceHit(Hit, Direction))
			{
				Result.Termination = ELightBeamTermination::ReceiverHit;
				Result.ReachedReceiver = HitReceiver;
			}
			else
			{
				Result.Termination = ELightBeamTermination::Blocked;
			}
			return nullptr;
		}
		if (ASandsOfSorrowReflector* HitReflector = Cast<ASandsOfSorrowReflector>(Hit.GetActor()))
		{
			if (!Reflectors.Contains(HitReflector))
			{
				Fail(TEXT("Beam hit a reflector not assigned to this puzzle."));
				return nullptr;
			}
			if (HitReflector->IsWorkingFaceHit(Hit, Direction))
			{
				if (Result.ReflectionCount >= MaxReflections)
				{
					Result.Termination = ELightBeamTermination::ReflectionLimit;
					return nullptr;
				}
				return HitReflector;
			}
		}
		Result.Termination = ELightBeamTermination::Blocked;
		return nullptr;
	};
	const auto AddSegment = [&Result](const FVector& Start, const FVector& End, const FHitResult& Hit)
	{
		FLightBeamSegment& Segment = Result.Segments.AddDefaulted_GetRef();
		Segment.Start = Start;
		Segment.End = End;
		Segment.Hit = Hit;
	};

	FVector Start = Emitter->GetBeamOrigin();
	FVector VisibleStart = Start;
	FVector Direction = Emitter->GetBeamDirection().GetSafeNormal();
	double Remaining = MaxTotalDistance;
	FHitResult PendingHit;
	bool bHasPendingHit = false;
	// Каждая итерация завершает сегмент: максимум MaxReflections + 1, включая последний.
	for (int32 Pass = 0; Pass <= MaxReflections; ++Pass)
	{
		const FVector End = Start + Direction * Remaining;
		if (End.ContainsNaN())
		{
			Fail(TEXT("Non-finite trace endpoint."));
			return Result;
		}
		FHitResult Hit;
		bool bHit = bHasPendingHit;
		if (bHasPendingHit)
		{
			// Близкое устройство внутри epsilon проходит тот же классификатор и может отразить луч.
			Hit = PendingHit;
			bHasPendingHit = false;
		}
		else
		{
			++Result.PrimaryTraceCount;
			bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, SandsOfSorrowLightCollision::Channel);
		}
		const FVector Point = bHit ? FVector(Hit.ImpactPoint) : End;
		AddSegment(VisibleStart, Point, Hit);
		const double Travel = FVector::Distance(Start, Point);
		Result.TravelledDistance += Travel;
		Remaining = FMath::Max(0.0, Remaining - Travel);
		if (!bHit)
		{
			Result.Termination = ELightBeamTermination::DistanceLimit;
			return Result;
		}
		if (!Hit.bStartPenetrating && Travel <= UE_KINDA_SMALL_NUMBER)
		{
			Result.Termination = ELightBeamTermination::NoProgress;
			return Result;
		}
		ASandsOfSorrowReflector* Reflector = ClassifyHit(Hit, Direction);
		if (!Reflector)
		{
			return Result;
		}
		const FVector Normal = Reflector->GetFaceNormal();
		Direction = (Direction - 2.0 * FVector::DotProduct(Direction, Normal) * Normal).GetSafeNormal();
		++Result.ReflectionCount;
		if (Remaining <= UE_KINDA_SMALL_NUMBER)
		{
			Result.Termination = ELightBeamTermination::DistanceLimit;
			return Result;
		}

		// Проверяем весь epsilon, исключая только покидаемый рабочий компонент.
		// Основной следующий запрос снова учитывает его, корпус и остальные устройства.
		const double Offset = FMath::Min(SurfaceOffset, Remaining);
		const FVector OffsetEnd = Point + Direction * Offset;
		FCollisionQueryParams OffsetParams(SCENE_QUERY_STAT(SandsLightOffset), false);
		OffsetParams.AddIgnoredComponent(Reflector->GetWorkingSurface());
		FHitResult OffsetHit;
		++Result.OffsetTraceCount;
		if (GetWorld()->LineTraceSingleByChannel(OffsetHit, Point, OffsetEnd, SandsOfSorrowLightCollision::Channel, OffsetParams))
		{
			// Длину этого короткого сегмента спишет следующая итерация ровно один раз.
			PendingHit = OffsetHit;
			bHasPendingHit = true;
			Start = Point;
			VisibleStart = Point;
			continue;
		}
		Result.TravelledDistance += Offset;
		Remaining = FMath::Max(0.0, Remaining - Offset);
		if (Remaining <= UE_KINDA_SMALL_NUMBER)
		{
			AddSegment(Point, OffsetEnd, FHitResult());
			Result.Termination = ELightBeamTermination::DistanceLimit;
			return Result;
		}
		VisibleStart = Point;
		Start = OffsetEnd;
	}
	Result.Termination = ELightBeamTermination::ReflectionLimit;
	return Result;
}

// Только сохранённые отрезки расчёта становятся видимыми; лишние компоненты скрываются.
void ASandsOfSorrowLightPuzzle::UpdateBeamVisuals(const FLightBeamTraceResult& Trace)
{
	// При отсутствии ресурса скрываем прежние сегменты тем же проходом очистки.
	const int32 Count = IsValid(BeamMesh) && GetWorld()
		? FMath::Min(Trace.Segments.Num(), MaxSupportedReflections + 1) : 0;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVector Start = Trace.Segments[Index].Start;
		const FVector End = Trace.Segments[Index].End;
		const FVector Delta = End - Start;
		if (Start.ContainsNaN() || End.ContainsNaN() || Delta.ContainsNaN() || Delta.IsNearlyZero())
		{
			if (BeamSegments.IsValidIndex(Index) && BeamSegments[Index])
			{
				BeamSegments[Index]->SetVisibility(false);
			}
			continue;
		}
		while (BeamSegments.Num() <= Index)
		{
			UStaticMeshComponent* Segment = NewObject<UStaticMeshComponent>(this);
			Segment->SetupAttachment(GetRootComponent());
			Segment->SetStaticMesh(BeamMesh);
			Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Segment->SetGenerateOverlapEvents(false);
			Segment->SetCastShadow(false);
			Segment->PrimaryComponentTick.bCanEverTick = false;
			if (BeamMaterial)
			{
				Segment->SetMaterial(0, BeamMaterial);
			}
			Segment->RegisterComponent();
			BeamSegments.Add(Segment);
		}
		UStaticMeshComponent* Segment = BeamSegments[Index];
		// Настройки могут измениться после создания пула, включая сброс материала.
		if (Segment->GetMaterial(0) != BeamMaterial.Get())
		{
			Segment->SetMaterial(0, BeamMaterial);
		}
		Segment->SetWorldLocation((Start + End) * 0.5);
		Segment->SetWorldRotation(FRotationMatrix::MakeFromZ(Delta).Rotator());
		Segment->SetWorldScale3D(FVector(0.025, 0.025, Delta.Length() / 100.0));
		Segment->SetVisibility(true);
	}
	for (int32 Index = Count; Index < BeamSegments.Num(); ++Index)
	{
		if (IsValid(BeamSegments[Index]))
		{
			BeamSegments[Index]->SetVisibility(false);
		}
	}
}

// Решение открывает назначенную дверь и включает заранее поставленный свет один раз.
void ASandsOfSorrowLightPuzzle::ApplySolvedReward()
{
	if (IsValid(Door) && Door->GetWorld() == GetWorld())
	{
		Door->SetOpen(true);
		// SetOpen может синхронно вызвать чужой Blueprint через OnOpened.
		if (!IsValid(this) || IsActorBeingDestroyed())
		{
			return;
		}
	}
	for (APointLight* Light : RewardLights)
	{
		if (IsValid(Light) && Light->GetWorld() == GetWorld() && Light->GetLightComponent())
		{
			Light->GetLightComponent()->SetVisibility(true);
		}
	}
	if (SolvedSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, SolvedSound, GetActorLocation());
	}
}
