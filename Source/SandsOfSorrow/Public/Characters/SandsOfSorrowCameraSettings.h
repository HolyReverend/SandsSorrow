#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SandsOfSorrowCameraSettings.generated.h"

class UCameraComponent;
class USpringArmComponent;

/** Конфигурация камеры и плеча; текущим положением и проверкой препятствий владеют штатные компоненты. */
UCLASS(BlueprintType)
class SANDSOFSORROW_API USandsOfSorrowCameraSettings : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "5.0", ClampMax = "170.0", ToolTip = "Угол обзора камеры по горизонтали, градусы. Меньшее значение делает персонажа и окружение крупнее в кадре, но сужает обзор. Итоговая вертикальная область зависит от соотношения сторон окна."))
	float FieldOfView = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spring Arm", meta = (ClampMin = "0.0", ClampMax = "10000.0", ToolTip = "Желаемая длина плеча камеры, см. Меньшая длина приближает камеру к персонажу. При включённой проверке препятствий фактическая дистанция может временно сокращаться."))
	float TargetArmLength = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spring Arm", meta = (ClampMin = "-10000.0", ClampMax = "10000.0", ToolTip = "Смещение начала плеча в мировых осях, см, относительно компонента-родителя. Z определяет высоту точки обзора; настройка согласуется с высотой капсулы и желаемым положением персонажа в кадре."))
	FVector TargetOffset = FVector(0.0, 0.0, 50.83839551347791);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spring Arm", meta = (ClampMin = "-10000.0", ClampMax = "10000.0", ToolTip = "Смещение камеры на конце плеча в локальных осях плеча, см. Y смещает камеру вбок от персонажа; штатная проверка препятствий учитывает это смещение."))
	FVector SocketOffset = FVector(0.0, 28.168040555333242, 0.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision", meta = (ToolTip = "Проверяет препятствия между точкой обзора и камерой. При столкновении плечо сокращается, чтобы камера не проходила через стены. Канал проверки остаётся штатным Camera."))
	bool bDoCollisionTest = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision", meta = (ClampMin = "0.0", ClampMax = "1000.0", EditCondition = "bDoCollisionTest", ToolTip = "Радиус сферы проверки камеры, см. Больший радиус оставляет больший зазор до стен и чаще сокращает плечо. Действует при включённом DoCollisionTest."))
	float ProbeSize = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Position Lag", meta = (ToolTip = "Включает плавное следование точки камеры за перемещением персонажа. Не меняет скорость персонажа; может создавать ощущение запаздывания камеры."))
	bool bEnableCameraLag = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Position Lag", meta = (ClampMin = "0.0", ClampMax = "1000.0", EditCondition = "bEnableCameraLag", ToolTip = "Скорость позиционного сглаживания. Большее значение быстрее приближает камеру к цели; 0 убирает интерполяционную задержку. Действует при EnableCameraLag."))
	float CameraLagSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Position Lag", meta = (ClampMin = "0.0", ClampMax = "10000.0", EditCondition = "bEnableCameraLag", ToolTip = "Максимальное отставание сглаженной точки камеры, см. 0 не ограничивает расстояние отставания. Это ограничение сглаживания, а не длина плеча."))
	float CameraLagMaxDistance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rotation Lag", meta = (ToolTip = "Включает плавное следование поворота плеча за направлением взгляда. Не меняет чувствительность мыши, но добавляет задержку поворота камеры."))
	bool bEnableCameraRotationLag = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rotation Lag", meta = (ClampMin = "0.0", ClampMax = "1000.0", EditCondition = "bEnableCameraRotationLag", ToolTip = "Скорость сглаживания поворота. Большее значение быстрее догоняет направление взгляда; 0 убирает интерполяционную задержку. Действует при EnableCameraRotationLag."))
	float CameraRotationLagSpeed = 10.0f;

	// Проверяет весь набор до записи; редакторские ограничения не заменяют проверку runtime-данных.
	bool IsValidConfiguration(FString& OutError) const;

	// Применяет настройки компонентов, сохраняя их привязки, политику управления и состояние игрового Pawn.
	bool ApplyTo(USpringArmComponent& SpringArm, UCameraComponent& Camera, FString& OutError) const;
};
