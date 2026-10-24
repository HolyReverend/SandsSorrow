#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Puzzles/Light/SandsOfSorrowLightTypes.h"
#include "SandsOfSorrowLightPuzzle.generated.h"

class ASandsOfSorrowLightEmitter;
class ASandsOfSorrowLightReceiver;
class ASandsOfSorrowReflector;
class ASandsOfSorrowPuzzleDoor;
class APointLight;
class UMaterialInterface;
class USoundBase;
class UStaticMeshComponent;
class UStaticMesh;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSandsLightPuzzleSolved);

// Координатор рассчитывает свет раз за игровой кадр и фиксирует решение один раз.
UCLASS(Blueprintable)
class SANDSOFSORROW_API ASandsOfSorrowLightPuzzle : public AActor
{
	GENERATED_BODY()

public:
	ASandsOfSorrowLightPuzzle();

	// Возвращает путь без побочных эффектов; можно использовать для диагностики.
	UFUNCTION(BlueprintCallable, Category = "Light")
	FLightBeamTraceResult TraceBeam() const;

	// Один защищённый от повторного входа расчёт с применением состояния комнаты.
	UFUNCTION(BlueprintCallable, Category = "Light")
	bool RecalculateNow();

	UFUNCTION(BlueprintPure, Category = "Light")
	bool IsSolved() const { return bIsSolved; }

	UPROPERTY(BlueprintAssignable, Category = "Light")
	FSandsLightPuzzleSolved OnSolved;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Light")
	FLightBeamTraceResult LastTraceResult;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Light")
	TObjectPtr<ASandsOfSorrowLightEmitter> Emitter;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Light")
	TObjectPtr<ASandsOfSorrowLightReceiver> Receiver;

	// Награда связывается с координатором экземпляра комнаты.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Light|Reward")
	TObjectPtr<ASandsOfSorrowPuzzleDoor> Door;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Light|Reward")
	TArray<TObjectPtr<APointLight>> RewardLights;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light|Visual")
	TObjectPtr<UMaterialInterface> BeamMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light|Audio")
	TObjectPtr<USoundBase> SolvedSound;

	// Назначенные устройства комнаты; пустые и повторные ссылки отклоняются.
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Light")
	TArray<TObjectPtr<ASandsOfSorrowReflector>> Reflectors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light", meta = (ClampMin = "0.001"))
	double MaxTotalDistance = 10000.0;

	// Общий бюджет ограничивает также замкнутый маршрут; максимум 17 основных трассировок.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light", meta = (ClampMin = "0", ClampMax = "16"))
	int32 MaxReflections = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light", meta = (ClampMin = "0.001", ClampMax = "0.1"))
	double SurfaceOffset = 0.05;
protected:
	// Реестр содержит только живые координаторы, без поиска всех Actor при каждом расчёте.
	virtual void PostRegisterAllComponents() override;
	virtual void PostUnregisterAllComponents() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Light")
	bool bIsSolved = false;

private:
	// Слабая ссылка позволяет погасить прежнюю цель после переназначения или обрыва.
	TWeakObjectPtr<ASandsOfSorrowLightReceiver> AppliedReceiver;
	// Компоненты удерживаются координатором и переиспользуются между расчётами.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> BeamSegments;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> BeamMesh;

	void UpdateBeamVisuals(const FLightBeamTraceResult& Trace);
	void ApplySolvedReward();
	bool bIsRecalculating = false;
};
