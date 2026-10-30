#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SandsOfSorrowGameplayPlayerController.generated.h"

class UEnhancedInputLocalPlayerSubsystem;
class UInputMappingContext;
class USandsOfSorrowInteractionComponent;
class USandsOfSorrowInteractionPromptWidget;
struct FInputKeyEventArgs;

/** Владеет локальным контекстом ввода, одной подсказкой и подпиской на текущий Pawn. */
UCLASS(Blueprintable)
class SANDSOFSORROW_API ASandsOfSorrowGameplayPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ASandsOfSorrowGameplayPlayerController();
	virtual void SetPawn(APawn* InPawn) override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;
	virtual void FlushPressedKeys() override;
	virtual void PreProcessInput(float DeltaTime, bool bGamePaused) override;
	virtual void PostProcessInput(float DeltaTime, bool bGamePaused) override;

	/** Вызывается из Started персонажа; одно физическое нажатие разрешает только одно действие. */
	bool ConsumeInteractionPress();
	// Отдельное разрешение crouch не расходует свежий ввод взаимодействия.
	bool ConsumeCrouchPress();
	bool IsGameplayControlAvailable() const;

	/** Проверяет, доступны ли локальное управление и взаимодействие в игровом viewport. */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsGameplayInteractionAvailable() const;

	/** Сообщает об открытии и закрытии UI; режим ввода переключает сам владелец меню. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetInteractionUIActive(bool bActive);

	UFUNCTION(BlueprintPure, Category = "Interaction")
	USandsOfSorrowInteractionPromptWidget* GetInteractionPrompt() const { return InteractionPrompt; }

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> PlayerMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<USandsOfSorrowInteractionPromptWidget> InteractionPromptClass;

private:
	void RemoveOwnedMappingContext();
	void EnsureInteractionPrompt();
	void SynchronizeInteraction();
	void DetachInteraction();
	void UpdatePrompt();
	bool IsInteractionKey(const FKey& Key) const;
	bool IsCrouchKey(const FKey& Key) const;
	void CancelCrouchHold();
	void HandleApplicationActivation(bool bActive);

	UFUNCTION()
	void HandleFocusChanged(AActor* Target, FText InteractionText);

	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> MappingSubsystem;
	TWeakObjectPtr<USandsOfSorrowInteractionComponent> ObservedInteraction;
	FDelegateHandle ApplicationActivationHandle;
	bool bInteractionUIActive = false;
	bool bApplicationActive = true;
	// Одноразовое разрешение: только между IE_Pressed и концом обработки ввода в этом кадре.
	bool bFreshInteractionPress = false;
	bool bFreshCrouchPress = false;
	bool bReportedPromptError = false;

	UPROPERTY(Transient)
	TObjectPtr<USandsOfSorrowInteractionPromptWidget> InteractionPrompt;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> OwnedMappingContext;
};
