#include "Characters/SandsOfSorrowCameraSettings.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

namespace SandsCameraSettingsLimits
{
constexpr float MinimumFieldOfView = 5.0f;
constexpr float MaximumFieldOfView = 170.0f;
constexpr float MaximumArmLength = 10000.0f;
constexpr double MaximumOffset = 10000.0;
constexpr float MaximumProbeSize = 1000.0f;
constexpr float MaximumLagSpeed = 1000.0f;
}

// Отклоняет опасные значения без частичного обновления камеры; пределы служат защитой, а не балансом.
bool USandsOfSorrowCameraSettings::IsValidConfiguration(FString& OutError) const
{
	OutError.Reset();
	if (!FMath::IsFinite(FieldOfView) || FieldOfView < SandsCameraSettingsLimits::MinimumFieldOfView
		|| FieldOfView > SandsCameraSettingsLimits::MaximumFieldOfView)
	{
		OutError = TEXT("FieldOfView must be finite and within [5, 170] degrees.");
		return false;
	}
	struct FCheckedValue
	{
		const TCHAR* Name;
		float Value;
		float Maximum;
	};
	const FCheckedValue Values[] = {
		{TEXT("TargetArmLength"), TargetArmLength, SandsCameraSettingsLimits::MaximumArmLength},
		{TEXT("ProbeSize"), ProbeSize, SandsCameraSettingsLimits::MaximumProbeSize},
		{TEXT("CameraLagSpeed"), CameraLagSpeed, SandsCameraSettingsLimits::MaximumLagSpeed},
		{TEXT("CameraLagMaxDistance"), CameraLagMaxDistance, SandsCameraSettingsLimits::MaximumArmLength},
		{TEXT("CameraRotationLagSpeed"), CameraRotationLagSpeed, SandsCameraSettingsLimits::MaximumLagSpeed}
	};
	for (const FCheckedValue& Value : Values)
	{
		if (!FMath::IsFinite(Value.Value) || Value.Value < 0.0f || Value.Value > Value.Maximum)
		{
			OutError = FString::Printf(TEXT("%s must be finite and within [0, %g]."), Value.Name, Value.Maximum);
			return false;
		}
	}
	if (TargetOffset.ContainsNaN() || SocketOffset.ContainsNaN()
		|| TargetOffset.GetAbsMax() > SandsCameraSettingsLimits::MaximumOffset
		|| SocketOffset.GetAbsMax() > SandsCameraSettingsLimits::MaximumOffset)
	{
		OutError = TEXT("TargetOffset and SocketOffset must be finite and within +/-10000 cm per axis.");
		return false;
	}
	return true;
}

// Сначала проверяется весь набор, чтобы неверный offset не оставлял уже изменённые FOV и длину плеча.
bool USandsOfSorrowCameraSettings::ApplyTo(USpringArmComponent& SpringArm, UCameraComponent& Camera, FString& OutError) const
{
	if (!IsValidConfiguration(OutError))
	{
		return false;
	}
	Camera.SetFieldOfView(FieldOfView);
	SpringArm.TargetArmLength = TargetArmLength;
	SpringArm.TargetOffset = TargetOffset;
	SpringArm.SocketOffset = SocketOffset;
	SpringArm.bDoCollisionTest = bDoCollisionTest;
	SpringArm.ProbeSize = ProbeSize;
	SpringArm.bEnableCameraLag = bEnableCameraLag;
	SpringArm.CameraLagSpeed = CameraLagSpeed;
	SpringArm.CameraLagMaxDistance = CameraLagMaxDistance;
	SpringArm.bEnableCameraRotationLag = bEnableCameraRotationLag;
	SpringArm.CameraRotationLagSpeed = CameraRotationLagSpeed;
	return true;
}
