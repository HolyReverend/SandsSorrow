#include "Characters/SandsOfSorrowCharacterMovementSettings.h"

#include "Characters/SandsOfSorrowCharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

// Диапазоны защищают расчёты CharacterMovement от неконечных и чрезмерных значений; это не игровой баланс.
bool USandsOfSorrowCharacterMovementSettings::IsValidConfiguration(FString& OutError) const
{
	OutError.Reset();
	struct FCheckedValue
	{
		const TCHAR* Name;
		float Value;
		float Maximum;
	};
	const FCheckedValue Values[] = {
		{TEXT("MaxWalkSpeed"), MaxWalkSpeed, 100000.0f},
		{TEXT("MinAnalogWalkSpeed"), MinAnalogWalkSpeed, 100000.0f},
		{TEXT("MaxAcceleration"), MaxAcceleration, 100000.0f},
		{TEXT("BrakingDecelerationWalking"), BrakingDecelerationWalking, 100000.0f},
		{TEXT("GroundFriction"), GroundFriction, 100.0f},
		{TEXT("CrouchedHalfHeight"), CrouchedHalfHeight, 100000.0f},
		{TEXT("CrouchMaxAcceleration"), CrouchMaxAcceleration, 100000.0f},
		{TEXT("CrouchBrakingDeceleration"), CrouchBrakingDeceleration, 100000.0f},
		{TEXT("CrouchGroundFriction"), CrouchGroundFriction, 100.0f},
		{TEXT("BrakingFriction"), BrakingFriction, 100.0f},
		{TEXT("BrakingFrictionFactor"), BrakingFrictionFactor, 100.0f},
		{TEXT("RotationRateYaw"), RotationRateYaw, 36000.0f},
		{TEXT("JumpZVelocity"), JumpZVelocity, 100000.0f},
		{TEXT("JumpMaxHoldTime"), JumpMaxHoldTime, 10.0f},
		{TEXT("GravityScale"), GravityScale, 100.0f},
		{TEXT("AirControl"), AirControl, 1.0f},
		{TEXT("AirControlBoostMultiplier"), AirControlBoostMultiplier, 100.0f},
		{TEXT("AirControlBoostVelocityThreshold"), AirControlBoostVelocityThreshold, 100000.0f},
		{TEXT("FallingLateralFriction"), FallingLateralFriction, 100.0f},
		{TEXT("BrakingDecelerationFalling"), BrakingDecelerationFalling, 100000.0f}
	};
	for (const FCheckedValue& Entry : Values)
	{
		if (!FMath::IsFinite(Entry.Value) || Entry.Value < 0.0f || Entry.Value > Entry.Maximum)
		{
			OutError = FString::Printf(TEXT("%s must be finite and within [0, %g]."), Entry.Name, Entry.Maximum);
			return false;
		}
	}
	if (MinAnalogWalkSpeed > MaxWalkSpeed * 0.5f)
	{
		OutError = TEXT("MinAnalogWalkSpeed must not exceed MaxWalkSpeed * 0.5.");
		return false;
	}
	if (JumpMaxCount < 0 || JumpMaxCount > 16)
	{
		OutError = TEXT("JumpMaxCount must be within [0, 16].");
		return false;
	}
	return true;
}

bool USandsOfSorrowCharacterMovementSettings::ApplyTo(ACharacter& Character, FString& OutError) const
{
	// Сначала проверяется весь набор, чтобы ошибка последнего поля не оставляла частичную конфигурацию.
	if (!IsValidConfiguration(OutError))
	{
		return false;
	}
	UCharacterMovementComponent* Movement = Character.GetCharacterMovement();
	if (!Movement)
	{
		OutError = TEXT("Character has no CharacterMovementComponent.");
		return false;
	}
	const UCapsuleComponent* Capsule = Character.GetCapsuleComponent();
	const ACharacter* DefaultCharacter = Character.bIsCrouched ? Character.GetClass()->GetDefaultObject<ACharacter>() : nullptr;
	const UCapsuleComponent* StandingCapsule = Character.bIsCrouched
		? (DefaultCharacter ? DefaultCharacter->GetCapsuleComponent() : nullptr) : Capsule;
	if (!Capsule || !StandingCapsule)
	{
		OutError = TEXT("Character has no capsule configuration for crouch.");
		return false;
	}
	// В crouch текущая высота уже уменьшена; стоящую конфигурацию берём из CDO класса, включая BP.
	const float CapsuleRadius = Capsule->GetUnscaledCapsuleRadius();
	const float StandingHalfHeight = StandingCapsule->GetUnscaledCapsuleHalfHeight();
	if (!FMath::IsFinite(CapsuleRadius) || !FMath::IsFinite(StandingHalfHeight)
		|| CapsuleRadius <= 0.0f || StandingHalfHeight < CapsuleRadius
		|| CrouchedHalfHeight < CapsuleRadius || CrouchedHalfHeight > StandingHalfHeight)
	{
		OutError = TEXT("CrouchedHalfHeight must fit between the actual capsule radius and standing half-height.");
		return false;
	}
	Movement->GetNavAgentPropertiesRef().bCanCrouch = true;
	// Сход с края остаётся возможен; малая воздушная капсула сохраняется до безопасной посадки.
	Movement->bCanWalkOffLedgesWhenCrouching = true;
	Movement->SetCrouchedHalfHeight(CrouchedHalfHeight);
	Movement->MaxWalkSpeedCrouched = MaxWalkSpeed * 0.5f;
	Movement->MaxWalkSpeed = MaxWalkSpeed;
	Movement->MinAnalogWalkSpeed = MinAnalogWalkSpeed;
	Movement->MaxAcceleration = MaxAcceleration;
	Movement->BrakingDecelerationWalking = BrakingDecelerationWalking;
	Movement->GroundFriction = GroundFriction;
	Movement->bUseSeparateBrakingFriction = bUseSeparateBrakingFriction;
	Movement->BrakingFriction = BrakingFriction;
	Movement->BrakingFrictionFactor = BrakingFrictionFactor;
	Movement->bOrientRotationToMovement = bOrientRotationToMovement;
	Movement->bUseControllerDesiredRotation = bUseControllerDesiredRotation;
	Movement->RotationRate = FRotator(0.0f, RotationRateYaw, 0.0f);
	Character.bUseControllerRotationYaw = bUseControllerRotationYaw;
	Movement->JumpZVelocity = JumpZVelocity;
	Character.JumpMaxCount = JumpMaxCount;
	Character.JumpMaxHoldTime = JumpMaxHoldTime;
	Movement->GravityScale = GravityScale;
	Movement->AirControl = AirControl;
	Movement->AirControlBoostMultiplier = AirControlBoostMultiplier;
	Movement->AirControlBoostVelocityThreshold = AirControlBoostVelocityThreshold;
	Movement->FallingLateralFriction = FallingLateralFriction;
	Movement->BrakingDecelerationFalling = BrakingDecelerationFalling;
	if (USandsOfSorrowCharacterMovementComponent* ProjectMovement = Cast<USandsOfSorrowCharacterMovementComponent>(Movement))
	{
		ProjectMovement->ConfigureCrouchGroundProfile(CrouchMaxAcceleration, CrouchBrakingDeceleration, CrouchGroundFriction);
	}
	return true;
}
