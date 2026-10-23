#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SandsOfSorrowInteractable.generated.h"

UINTERFACE(BlueprintType, Blueprintable)
class SANDSOFSORROW_API USandsOfSorrowInteractable : public UInterface
{
	GENERATED_BODY()
};

/** Device contract. Queries must not mutate the world; callers still tolerate Blueprint destruction. */
class SANDSOFSORROW_API ISandsOfSorrowInteractable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool CanInteract(AActor* Interactor) const;
	virtual bool CanInteract_Implementation(AActor* Interactor) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FText GetInteractionText(AActor* Interactor) const;
	virtual FText GetInteractionText_Implementation(AActor* Interactor) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool Interact(AActor* Interactor);
	virtual bool Interact_Implementation(AActor* Interactor);
};
