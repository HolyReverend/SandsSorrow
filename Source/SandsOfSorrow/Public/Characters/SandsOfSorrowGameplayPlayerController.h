#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SandsOfSorrowGameplayPlayerController.generated.h"

class UEnhancedInputLocalPlayerSubsystem;
class UInputMappingContext;
class USandsOfSorrowInteractionComponent;
class USandsOfSorrowInteractionPromptWidget;
struct FInputKeyEventArgs;

/** Owns the local context, one prompt and the lifetime of the current Pawn subscription. */
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

	/** Called only by the Character's Started handler; one physical press can be consumed once. */
	bool ConsumeInteractionPress();

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsGameplayInteractionAvailable() const;

	/** Notify interaction before opening UI and after closing it; input mode remains the UI owner's job. */
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
	void HandleApplicationActivation(bool bActive);

	UFUNCTION()
	void HandleFocusChanged(AActor* Target, FText InteractionText);

	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> MappingSubsystem;
	TWeakObjectPtr<USandsOfSorrowInteractionComponent> ObservedInteraction;
	FDelegateHandle ApplicationActivationHandle;
	bool bInteractionUIActive = false;
	bool bApplicationActive = true;
	bool bFreshInteractionPress = false;
	bool bReportedPromptError = false;

	UPROPERTY(Transient)
	TObjectPtr<USandsOfSorrowInteractionPromptWidget> InteractionPrompt;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> OwnedMappingContext;
};
