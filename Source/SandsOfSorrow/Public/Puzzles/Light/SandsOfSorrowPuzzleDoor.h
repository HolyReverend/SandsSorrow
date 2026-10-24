#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SandsOfSorrowPuzzleDoor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSandsPuzzleDoorOpened);

// Створка движется с физической коллизией и сохраняет запрос открытия при препятствии.
UCLASS(Blueprintable)
class SANDSOFSORROW_API ASandsOfSorrowPuzzleDoor : public AActor
{
	GENERATED_BODY()

public:
	ASandsOfSorrowPuzzleDoor();
	virtual void Tick(float DeltaSeconds) override;

	// Повторный запрос не сбрасывает движение; игровая дверь не закрывается обратно.
	UFUNCTION(BlueprintCallable, Category = "Door")
	void SetOpen(bool bOpen);

	UFUNCTION(BlueprintPure, Category = "Door")
	bool IsFullyOpen() const { return bFullyOpen; }

	UPROPERTY(BlueprintAssignable, Category = "Door")
	FSandsPuzzleDoorOpened OnOpened;

	// Локальное смещение створки и время полного пути в секундах.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
	FVector OpenOffset = FVector(0.0, 0.0, 280.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door", meta = (ClampMin = "0.0"))
	float OpenDuration = 1.5f;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	TObjectPtr<UBoxComponent> CollisionRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

private:
	// Начальная точка принадлежит экземпляру карты и не меняется при повторных вызовах.
	FVector ClosedWorldLocation = FVector::ZeroVector;
	bool bOpenRequested = false;
	bool bFullyOpen = false;

	void AdvanceOpening(float DeltaSeconds);
};
