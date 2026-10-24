#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SandsOfSorrowLightEmitter.generated.h"

class USceneComponent;
class UStaticMeshComponent;

// Неподвижный источник: точка выхода находится снаружи собственного корпуса.
UCLASS(Blueprintable)
class SANDSOFSORROW_API ASandsOfSorrowLightEmitter : public AActor
{
	GENERATED_BODY()

public:
	ASandsOfSorrowLightEmitter();

	// Направление задаётся ориентацией BeamOrigin, а не ссылкой на следующую цель.
	UFUNCTION(BlueprintPure, Category = "Light")
	FVector GetBeamOrigin() const;

	UFUNCTION(BlueprintPure, Category = "Light")
	FVector GetBeamDirection() const;

	// Проверка масштаба и конечности исходных данных перед запросами физики.
	bool HasValidBeamTransform() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light")
	bool bEnabled = true;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Light")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Light")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Light")
	TObjectPtr<USceneComponent> BeamOrigin;
};
