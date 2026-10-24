#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SandsOfSorrowInteractionComponent.generated.h"

class APawn;
class APlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSandsInteractionFocusChanged, AActor*, Target, FText, InteractionText);

/** Одна цель по взгляду камеры с отдельной проверкой расстояния и видимости от тела. */
UCLASS(ClassGroup = (Interaction), meta = (BlueprintSpawnableComponent))
class SANDSOFSORROW_API USandsOfSorrowInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USandsOfSorrowInteractionComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Контроллер блокирует выбор при UI, паузе, потере фокуса или управления Pawn. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetInteractionInputEnabled(bool bEnabled);

	virtual void Deactivate() override;

	/** Обновляет цель и текст и сообщает подписчикам об изменениях. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void RefreshFocus();

	/** Перепроверяет мир; подмена цели не активирует соседнее устройство тем же нажатием. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	bool RequestInteraction();

	/** Сбрасывает цель и текст, уведомляя UI через OnFocusChanged. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void ClearFocus();

	UFUNCTION(BlueprintPure, Category = "Interaction")
	AActor* GetFocusedActor() const { return FocusedActor.Get(); }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	FText GetFocusedText() const { return FocusedText; }

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FSandsInteractionFocusChanged OnFocusChanged;

	// Досягаемость от позиции персонажа до точки попадания; не длина луча камеры.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction", meta = (ClampMin = "1.0", Units = "cm"))
	float InteractionDistance = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction", meta = (ClampMin = "1.0", Units = "cm"))
	float CameraTraceDistance = 1000.0f;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool GetInteractionContext(APawn*& Pawn, APlayerController*& Controller) const;
	AActor* FindTarget(FText& OutText) const;
	void SetFocus(AActor* Target, const FText& Text);

	TWeakObjectPtr<AActor> FocusedActor;
	FText FocusedText;
	bool bInteractionInputEnabled = true;
	// Защищает от вложенного запроса из подписчиков изменения фокуса.
	bool bRequestInProgress = false;
};
