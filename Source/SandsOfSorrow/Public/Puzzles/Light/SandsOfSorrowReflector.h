#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/SandsOfSorrowInteractable.h"
#include "SandsOfSorrowReflector.generated.h"

class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;
class USoundBase;

/** Четыре положения головки; маркер показывает нормаль рабочего лица +X. */
UCLASS(Blueprintable)
class SANDSOFSORROW_API ASandsOfSorrowReflector : public AActor, public ISandsOfSorrowInteractable
{
	GENERATED_BODY()

public:
	// Создаёт корпус, головку и отдельную коллизию рабочего лица.
	ASandsOfSorrowReflector();
	// Применяет начальную ориентацию при создании и изменении экземпляра.
	virtual void OnConstruction(const FTransform& Transform) override;
	// Интерфейс сохраняет одиночный шаг и существующую доступность взаимодействия.
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractionText_Implementation(AActor* Interactor) const override;
	virtual bool Interact_Implementation(AActor* Interactor) override;

	// Индекс дискретного шага головки: 0–3 относительно InitialHeadRotation.
	UFUNCTION(BlueprintPure, Category = "Reflector")
	int32 GetRotationIndex() const { return RotationIndex; }

	// Мировая +X рабочей плоскости, независимо от нормалей декоративного меша.
	UFUNCTION(BlueprintPure, Category = "Reflector")
	FVector GetFaceNormal() const;

	// Возвращает компонент для распознавания попаданий, а не весь Actor.
	UBoxComponent* GetWorkingSurface() const { return WorkingSurface; }

	// Только положительный равномерный мировой масштаб поддерживает текущую модель зеркала.
	UFUNCTION(BlueprintPure, Category = "Reflector|Light")
	bool HasSupportedLightTransform() const;

	// Принимает лишь фронтальное попадание в плоскую грань рабочего компонента.
	bool IsWorkingFaceHit(const FHitResult& Hit, const FVector& IncomingDirection) const;

	// Короткий звук успешного шага назначается в BP.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reflector|Audio")
	TObjectPtr<USoundBase> RotateSound;

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

	// Тонкая рабочая плоскость вынесена перед корпусом и не перехватывает Visibility.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reflector|Light")
	TObjectPtr<UBoxComponent> WorkingSurface;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reflector")
	TObjectPtr<UStaticMeshComponent> NormalMarker;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reflector")
	TObjectPtr<UStaticMeshComponent> NormalMarkerTip;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Reflector")
	int32 RotationIndex = 0;

private:
	// Меняет только головку; основание остаётся неподвижным.
	void ApplyHeadRotation();
};
