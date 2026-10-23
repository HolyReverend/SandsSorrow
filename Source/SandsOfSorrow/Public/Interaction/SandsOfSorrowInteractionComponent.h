#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SandsOfSorrowInteractionComponent.generated.h"

class APawn;
class APlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSandsInteractionFocusChanged, AActor*, Target, FText, InteractionText);

/** One camera-selected target, with independent reach and body-to-hit visibility checks. */
UCLASS(ClassGroup = (Interaction), meta = (BlueprintSpawnableComponent))
class SANDSOFSORROW_API USandsOfSorrowInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USandsOfSorrowInteractionComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Controller suspends selection during UI, pause, loss of focus or possession. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetInteractionInputEnabled(bool bEnabled);

	virtual void Deactivate() override;

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void RefreshFocus();

	/** Rechecks the world, but never activates a different target on the same press. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	bool RequestInteraction();

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void ClearFocus();

	UFUNCTION(BlueprintPure, Category = "Interaction")
	AActor* GetFocusedActor() const { return FocusedActor.Get(); }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	FText GetFocusedText() const { return FocusedText; }

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FSandsInteractionFocusChanged OnFocusChanged;

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
	bool bRequestInProgress = false;
};
