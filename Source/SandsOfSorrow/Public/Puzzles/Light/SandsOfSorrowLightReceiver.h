#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SandsOfSorrowLightReceiver.generated.h"

class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class USoundBase;
class UMaterialInterface;
class ASandsOfSorrowLightPuzzle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSandsLightIlluminationChanged, bool, bIlluminated);

// Приёмник распознаёт рабочее лицо и хранит обратимое текущее освещение.
UCLASS(Blueprintable)
class SANDSOFSORROW_API ASandsOfSorrowLightReceiver : public AActor
{
	GENERATED_BODY()

public:
	ASandsOfSorrowLightReceiver();

	// Используется координатором для проверки конкретного компонента и передней грани.
	bool IsWorkingFaceHit(const FHitResult& Hit, const FVector& IncomingDirection) const;
	UBoxComponent* GetWorkingSurface() const { return WorkingSurface; }

	UFUNCTION(BlueprintPure, Category = "Light")
	FVector GetFaceNormal() const;

	UFUNCTION(BlueprintPure, Category = "Light")
	bool HasSupportedLightTransform() const;

	// Результат последнего применения; один TraceBeam сам по себе его не меняет.
	UFUNCTION(BlueprintPure, Category = "Light")
	bool IsIlluminated() const { return bIsIlluminated; }

	UPROPERTY(BlueprintAssignable, Category = "Light")
	FSandsLightIlluminationChanged OnIlluminationChanged;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light|Visual")
	TObjectPtr<UMaterialInterface> ActiveMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light|Visual")
	TObjectPtr<UMaterialInterface> InactiveMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light|Audio")
	TObjectPtr<USoundBase> TransitionSound;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Light")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Light")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Light")
	TObjectPtr<UBoxComponent> WorkingSurface;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Light|Visual")
	TObjectPtr<UStaticMeshComponent> IndicatorMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Light|Visual")
	TObjectPtr<UPointLightComponent> IndicatorLight;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Light")
	bool bIsIlluminated = false;

private:
	friend class ASandsOfSorrowLightPuzzle;

	// Координатор сначала фиксирует все логические состояния, затем рассылает уведомления.
	bool SetIlluminatedSilently(bool bNewValue);
	void NotifyIlluminationChanged();
};
