#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SandsOfSorrowPlayerCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class USpringArmComponent;
class USandsOfSorrowInteractionComponent;
class USandsOfSorrowCharacterMovementSettings;
class USandsOfSorrowCameraSettings;
struct FInputActionValue;

/** Игровой персонаж; действия ввода и временный меш назначаются в Blueprint. */
UCLASS(Blueprintable)
class SANDSOFSORROW_API ASandsOfSorrowPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ASandsOfSorrowPlayerCharacter();

	const UInputAction* GetInteractAction() const { return InteractAction; }

	UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	// Возвращает назначенный источник настроек; отсутствие ссылки означает нативную конфигурацию по умолчанию.
	UFUNCTION(BlueprintPure, Category = "Movement", meta = (ToolTip = "Назначенный ассет настроек движения. Если ссылка отсутствует, используются нативные значения по умолчанию."))
	const USandsOfSorrowCharacterMovementSettings* GetMovementSettings() const { return MovementSettings; }

	// Возвращает назначенный DA компонентов камеры; без ссылки используется нативная конфигурация.
	UFUNCTION(BlueprintPure, Category = "Camera", meta = (ToolTip = "Назначенный ассет настроек Camera и SpringArm. Отсутствующая ссылка использует нативные значения по умолчанию."))
	const USandsOfSorrowCameraSettings* GetCameraSettings() const { return CameraSettings; }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	USandsOfSorrowInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }

protected:
	// Обновляет предпросмотр после Construction Script без ручной симуляции движения.
	virtual void OnConstruction(const FTransform& Transform) override;

	// Исключает из светового луча также компоненты, добавленные в игровом Blueprint.
	virtual void PostInitializeComponents() override;

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (ToolTip = "Единый источник настроек ходьбы, разворота и прыжка. Применяется после сборки компонентов Blueprint, заменяя их старые значения. После изменения ассета создайте нового персонажа или перезапустите PIE."))
	TObjectPtr<USandsOfSorrowCharacterMovementSettings> MovementSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera", meta = (ToolTip = "Источник FOV, длины и смещений плеча, коллизии и сглаживания камеры. Применяется после сборки Blueprint и заменяет старые значения компонентов. После изменения DA создайте нового персонажа или перезапустите PIE. Чувствительность и пределы pitch настраиваются отдельно."))
	TObjectPtr<USandsOfSorrowCameraSettings> CameraSettings;

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

	/** Чувствительность мыши по осям; её смещение не умножается на время кадра. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "0.0"))
	FVector2D LookSensitivity = FVector2D(1.0, 1.0);

private:
	// Применяет валидный ассет; при ошибке сохраняет безопасные нативные значения и сообщает причину один раз.
	void ApplyMovementSettings();
	bool bMovementSettingsErrorReported = false;

	// Применяет конфигурацию собственных компонентов; ошибка диагностируется один раз и включает безопасный fallback.
	void ApplyCameraSettings();
	bool bCameraSettingsErrorReported = false;

	void Interact();
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
};
