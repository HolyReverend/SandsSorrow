#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SandsOfSorrowCharacterMovementSettings.generated.h"

class ACharacter;

/** Общая конфигурация движения; симуляцией и текущей скоростью владеет CharacterMovement. */
UCLASS(BlueprintType)
class SANDSOFSORROW_API USandsOfSorrowCharacterMovementSettings : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (ClampMin = "0.0", ClampMax = "100000.0", ToolTip = "Максимальная скорость обычного движения, см/с. 500 см/с соответствует 5 м/с. Также участвует в ограничении горизонтальной скорости в воздухе."))
	float MaxWalkSpeed = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (ClampMin = "0.0", ClampMax = "100000.0", ToolTip = "Нижняя целевая скорость при очень слабом ненулевом аналоговом вводе, см/с. Не должна превышать половину MaxWalkSpeed; нулевой ввод по-прежнему останавливает персонажа."))
	float MinAnalogWalkSpeed = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (ClampMin = "0.0", ClampMax = "100000.0", ToolTip = "Максимальное ускорение от ввода, см/с². Меньшее значение удлиняет разгон и смену направления. В воздухе его влияние дополнительно ограничивается AirControl."))
	float MaxAcceleration = 1600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (ClampMin = "0.0", ClampMax = "100000.0", ToolTip = "Постоянная часть торможения при ходьбе без ускоряющего ввода, см/с². Работает вместе с трением; меньшее значение увеличивает время и путь остановки."))
	float BrakingDecelerationWalking = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ground", meta = (ClampMin = "0.0", ClampMax = "100.0", ToolTip = "Трение на земле: определяет быстроту изменения направления. Если отдельное тормозное трение отключено, используется и при остановке с множителем BrakingFrictionFactor."))
	float GroundFriction = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crouch", meta = (ClampMin = "0.0", ClampMax = "100000.0", ToolTip = "Половина высоты капсулы в crouch, см без масштаба; не меньше фактического радиуса и не больше стоящей капсулы. Начальное значение требует проверки ощущения и геометрии."))
	float CrouchedHalfHeight = 52.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crouch", meta = (ClampMin = "0.0", ClampMax = "100000.0", ToolTip = "Максимальное наземное ускорение в crouch, см/с². Не меняет воздушный профиль."))
	float CrouchMaxAcceleration = 2400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crouch", meta = (ClampMin = "0.0", ClampMax = "100000.0", ToolTip = "Постоянное наземное торможение в crouch, см/с². Начальное значение для уменьшения инерции."))
	float CrouchBrakingDeceleration = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crouch", meta = (ClampMin = "0.0", ClampMax = "100.0", ToolTip = "Наземное трение в crouch; применяется только к фактическому наземному состоянию и не переносится в падение."))
	float CrouchGroundFriction = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Braking", meta = (ToolTip = "Включает отдельное BrakingFriction при торможении. Если выключено, используется трение текущего режима: GroundFriction на земле и FallingLateralFriction в воздухе."))
	bool bUseSeparateBrakingFriction = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Braking", meta = (ClampMin = "0.0", ClampMax = "100.0", EditCondition = "bUseSeparateBrakingFriction", ToolTip = "Отдельное трение при отсутствии ускоряющего ввода или превышении скорости. Учитывается только при включённом UseSeparateBrakingFriction и может действовать также в воздухе."))
	float BrakingFriction = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Braking", meta = (ClampMin = "0.0", ClampMax = "100.0", ToolTip = "Множитель трения именно при торможении. Например, GroundFriction 8 и множитель 1 дают тормозное трение 8 при отключённом отдельном режиме. Может влиять на воздух, если его трение ненулевое."))
	float BrakingFrictionFactor = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Orientation", meta = (ToolTip = "Поворачивает тело по направлению ускорения движения. Имеет приоритет над UseControllerDesiredRotation; для движения боком с сохранением направления тела эту настройку отключают."))
	bool bOrientRotationToMovement = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Orientation", meta = (ToolTip = "Плавно поворачивает тело к желаемой ориентации Controller со скоростью RotationRateYaw. Не используется, пока включён OrientRotationToMovement."))
	bool bUseControllerDesiredRotation = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Orientation", meta = (ToolTip = "Передаёт поворот Controller по горизонтали непосредственно телу Pawn. Это отдельный способ ориентации; обычно применяется с отключённым OrientRotationToMovement."))
	bool bUseControllerRotationYaw = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Orientation", meta = (ClampMin = "0.0", ClampMax = "36000.0", ToolTip = "Скорость плавного горизонтального разворота тела, градусов/с. Используется CharacterMovement при OrientRotationToMovement или UseControllerDesiredRotation; 0 отключает этот плавный разворот."))
	float RotationRateYaw = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jump", meta = (ClampMin = "0.0", ClampMax = "100000.0", ToolTip = "Начальная вертикальная скорость прыжка, см/с. Высота и время в воздухе также зависят от GravityScale и удержания кнопки."))
	float JumpZVelocity = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jump", meta = (ClampMin = "0", ClampMax = "16", ToolTip = "Максимальное число прыжков до приземления. 0 запрещает прыжок, 1 разрешает один обычный прыжок, большее значение добавляет повторные прыжки в воздухе."))
	int32 JumpMaxCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jump", meta = (ClampMin = "0.0", ClampMax = "10.0", ToolTip = "Максимальное время дополнительной силы прыжка при удержании кнопки, секунды. 0 оставляет обычный прыжок без продления удержанием; обработчик отпускания вызывает StopJumping."))
	float JumpMaxHoldTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air", meta = (ClampMin = "0.0", ClampMax = "100.0", ToolTip = "Множитель мировой гравитации для персонажа. 1 использует обычную гравитацию; увеличение ускоряет падение и уменьшает высоту при той же скорости прыжка."))
	float GravityScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Доля управления ускорением в воздухе от наземного. 0 убирает воздушную коррекцию, 1 разрешает полное управление; итог также зависит от воздушного ускорителя и трения."))
	float AirControl = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air", meta = (ClampMin = "0.0", ClampMax = "100.0", ToolTip = "Множитель AirControl при малой горизонтальной скорости в воздухе. Используется ниже порога AirControlBoostVelocityThreshold; итоговый AirControl ограничивается единицей."))
	float AirControlBoostMultiplier = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air", meta = (ClampMin = "0.0", ClampMax = "100000.0", ToolTip = "Порог горизонтальной скорости для усиления AirControl, см/с. Ниже этого порога применяется AirControlBoostMultiplier."))
	float AirControlBoostVelocityThreshold = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air", meta = (ClampMin = "0.0", ClampMax = "100.0", ToolTip = "Горизонтальное трение в режиме падения. При отключённом отдельном тормозном трении участвует также в воздушной остановке с множителем BrakingFrictionFactor."))
	float FallingLateralFriction = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air", meta = (ClampMin = "0.0", ClampMax = "100000.0", ToolTip = "Постоянная часть горизонтального торможения в воздухе без ускоряющего ввода, см/с². Даже при нулевом FallingLateralFriction ненулевое значение может гасить горизонтальную инерцию прыжка."))
	float BrakingDecelerationFalling = 1500.0f;

	// Проверяет весь набор до записи в физический компонент, включая значения вне редакторских ограничений.
	bool IsValidConfiguration(FString& OutError) const;

	// Применяет только конфигурацию: не меняет текущую скорость, позицию или накопленный ввод.
	bool ApplyTo(ACharacter& Character, FString& OutError) const;
};
