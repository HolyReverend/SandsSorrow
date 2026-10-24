#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SandsOfSorrowLightStateTestListener.generated.h"

class ASandsOfSorrowLightEmitter;
class ASandsOfSorrowLightPuzzle;
class ASandsOfSorrowLightReceiver;

// Слушатель проверяет порядок публичных уведомлений и попытку повторного входа.
UCLASS()
class USandsOfSorrowLightStateTestListener : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<ASandsOfSorrowLightPuzzle> Puzzle;

	UPROPERTY()
	TObjectPtr<ASandsOfSorrowLightReceiver> Receiver;

	UPROPERTY()
	TObjectPtr<ASandsOfSorrowLightEmitter> Emitter;

	int32 IlluminationEvents = 0;
	int32 SolvedEvents = 0;
	bool bCoherent = true;
	bool bReentryRejected = true;
	bool bDisableEmitterOnFirstLight = false;

	UFUNCTION()
	void HandleIllumination(bool bIlluminated);

	UFUNCTION()
	void HandleSolved();

	// Моделирует Blueprint, удаляющий комнату сразу после открытия двери.
	UFUNCTION()
	void HandleDoorOpened();
};
