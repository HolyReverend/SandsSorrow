#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SandsOfSorrowInteractable.generated.h"

UINTERFACE(BlueprintType, Blueprintable)
class SANDSOFSORROW_API USandsOfSorrowInteractable : public UInterface
{
	GENERATED_BODY()
};

/** Контракт устройства: запросы не меняют мир; вызывающий код учитывает возможное уничтожение из Blueprint. */
class SANDSOFSORROW_API ISandsOfSorrowInteractable
{
	GENERATED_BODY()

public:
	/** Проверяет доступность действия для инициатора без изменения состояния. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool CanInteract(AActor* Interactor) const;
	virtual bool CanInteract_Implementation(AActor* Interactor) const;

	/** Возвращает подпись доступного действия для подсказки игроку. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FText GetInteractionText(AActor* Interactor) const;
	virtual FText GetInteractionText_Implementation(AActor* Interactor) const;

	/** Выполняет одно действие и возвращает, было ли оно принято устройством. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool Interact(AActor* Interactor);
	virtual bool Interact_Implementation(AActor* Interactor);
};
