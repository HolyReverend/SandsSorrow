#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/SandsOfSorrowInteractable.h"
#include "SandsOfSorrowReflector.generated.h"

class USceneComponent;
class UStaticMeshComponent;

/** Four local orientations. The marker is the working face normal (+X), not an outgoing beam. */
UCLASS(Blueprintable)
class SANDSOFSORROW_API ASandsOfSorrowReflector : public AActor, public ISandsOfSorrowInteractable
{
	GENERATED_BODY()

public:
	ASandsOfSorrowReflector();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractionText_Implementation(AActor* Interactor) const override;
	virtual bool Interact_Implementation(AActor* Interactor) override;

	UFUNCTION(BlueprintPure, Category = "Reflector")
	int32 GetRotationIndex() const { return RotationIndex; }

	UFUNCTION(BlueprintPure, Category = "Reflector")
	FVector GetFaceNormal() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FText InteractionText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bInteractionEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reflector")
	FRotator InitialHeadRotation = FRotator(0.0f, 45.0f, 0.0f);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reflector")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reflector")
	TObjectPtr<UStaticMeshComponent> BaseMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reflector")
	TObjectPtr<USceneComponent> HeadPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reflector")
	TObjectPtr<UStaticMeshComponent> HeadMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reflector")
	TObjectPtr<UStaticMeshComponent> NormalMarker;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reflector")
	TObjectPtr<UStaticMeshComponent> NormalMarkerTip;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Reflector")
	int32 RotationIndex = 0;

private:
	void ApplyHeadRotation();
};
