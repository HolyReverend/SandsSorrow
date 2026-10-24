#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "SandsOfSorrowLightTypes.generated.h"

class ASandsOfSorrowLightReceiver;

// Причина остановки расчёта; лимит отражений не выдаётся за распознанный цикл.
UENUM(BlueprintType)
enum class ELightBeamTermination : uint8
{
	ReceiverHit,
	Blocked,
	DistanceLimit,
	ReflectionLimit,
	NoProgress,
	InvalidConfiguration,
	SourceDisabled
};

// Геометрия одного отрезка доступна BP для диагностики и будущей визуализации.
USTRUCT(BlueprintType)
struct SANDSOFSORROW_API FLightBeamSegment
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Light")
	FVector Start = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Light")
	FVector End = FVector::ZeroVector;

	// Слабые ссылки внутри FHitResult не удерживают уничтоженные компоненты мира.
	FHitResult Hit;
};

// Полный результат одного прохода; сам по себе не меняет состояния устройств.
USTRUCT(BlueprintType)
struct SANDSOFSORROW_API FLightBeamTraceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Light")
	TArray<FLightBeamSegment> Segments;

	UPROPERTY(BlueprintReadOnly, Category = "Light")
	TWeakObjectPtr<ASandsOfSorrowLightReceiver> ReachedReceiver;

	UPROPERTY(BlueprintReadOnly, Category = "Light")
	ELightBeamTermination Termination = ELightBeamTermination::InvalidConfiguration;

	UPROPERTY(BlueprintReadOnly, Category = "Light")
	int32 ReflectionCount = 0;

	// Счётчики позволяют проверить верхнюю границу физических запросов.
	UPROPERTY(BlueprintReadOnly, Category = "Light")
	int32 PrimaryTraceCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Light")
	int32 OffsetTraceCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Light")
	double TravelledDistance = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Light")
	FString Diagnostic;
};
