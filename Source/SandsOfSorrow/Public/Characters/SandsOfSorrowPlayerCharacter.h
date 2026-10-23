#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SandsOfSorrowPlayerCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class USpringArmComponent;
class USandsOfSorrowInteractionComponent;
struct FInputActionValue;

/** Gameplay character. Input assets and the temporary mesh are assigned in Blueprint. */
UCLASS(Blueprintable)
class SANDSOFSORROW_API ASandsOfSorrowPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ASandsOfSorrowPlayerCharacter();

	const UInputAction* GetInteractAction() const { return InteractAction; }

	UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	USandsOfSorrowInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<USandsOfSorrowInteractionComponent> InteractionComponent;

	/** Per-axis mouse multiplier. Mouse deltas are not multiplied by frame time. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "0.0"))
	FVector2D LookSensitivity = FVector2D(1.0, 1.0);

private:
	void Interact();
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
};
