#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SandsOfSorrowCharacterMovementComponent.generated.h"

/** Сохраняет стандартную физику; strafe-разворот следует за камерой только при движении. */
UCLASS()
class SANDSOFSORROW_API USandsOfSorrowCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	// Сохраняет профиль отдельно от стояния и воздуха: смена позы не накапливает множители.
	void ConfigureCrouchGroundProfile(float InAcceleration, float Braking, float Friction);
	virtual float GetMaxAcceleration() const override;
	virtual float GetMaxBrakingDeceleration() const override;
	virtual float GetMaxSpeed() const override;

protected:
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
	virtual void ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration) override;

private:
	float CrouchAcceleration = 2400.0f;
	float CrouchBraking = 2000.0f;
	float CrouchFriction = 12.0f;

protected:
	virtual void PhysicsRotation(float DeltaTime) override;
};
