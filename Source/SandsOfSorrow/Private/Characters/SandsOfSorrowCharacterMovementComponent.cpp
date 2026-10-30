#include "Characters/SandsOfSorrowCharacterMovementComponent.h"
#include "Characters/SandsOfSorrowPlayerCharacter.h"

// Скорость уже рассчитана физикой: осмотр и ввод в стену не вызывают разворот на месте.
void USandsOfSorrowCharacterMovementComponent::PhysicsRotation(float DeltaTime)
{
	constexpr double StationarySpeedToleranceCmPerSecond = 1.0;
	if (!bOrientRotationToMovement && bUseControllerDesiredRotation
		&& Velocity.SizeSquared2D() <= FMath::Square(StationarySpeedToleranceCmPerSecond))
	{
		return;
	}
	// Штатный путь сохраняет RotationRate, strafe и резервную ориентацию по движению.
	Super::PhysicsRotation(DeltaTime);
}

// Профиль поступает после проверки всего DA; физические поля базового профиля не подменяются.
void USandsOfSorrowCharacterMovementComponent::ConfigureCrouchGroundProfile(float InAcceleration, float Braking, float Friction)
{
	CrouchAcceleration = InAcceleration;
	CrouchBraking = Braking;
	CrouchFriction = Friction;
}

// Запрос удержания хранит персонаж; физическое состояние меняется штатными проверками капсулы.
void USandsOfSorrowCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	const ASandsOfSorrowPlayerCharacter* Player = Cast<ASandsOfSorrowPlayerCharacter>(CharacterOwner);
	if (Player)
	{
		// В воздухе не меняем объём: сход с края сохраняет маленькую капсулу до посадки.
		bWantsToCrouch = IsFalling() ? IsCrouching() : Player->IsCrouchHeld();
	}
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}

// Посадка согласует позу до оставшейся наземной физики этого кадра.
void USandsOfSorrowCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
	if (HasValidData() && IsMovingOnGround())
	{
		UpdateCharacterStateBeforeMovement(0.0f);
	}
}

// Воздушное ускорение всегда берётся из исходного профиля, даже при сходе с края в crouch.
float USandsOfSorrowCharacterMovementComponent::GetMaxAcceleration() const
{
	return IsMovingOnGround() && IsCrouching() ? CrouchAcceleration : Super::GetMaxAcceleration();
}

// Производная скорость пересчитывается и при диагностическом изменении базовой скорости.
float USandsOfSorrowCharacterMovementComponent::GetMaxSpeed() const
{
	return IsMovingOnGround() && IsCrouching() ? MaxWalkSpeed * 0.5f : Super::GetMaxSpeed();
}

// Торможение crouch действует только на земле.
float USandsOfSorrowCharacterMovementComponent::GetMaxBrakingDeceleration() const
{
	return IsMovingOnGround() && IsCrouching() ? CrouchBraking : Super::GetMaxBrakingDeceleration();
}

// Нативный расчёт сохраняет инерцию и ограничение скорости; меняются только параметры текущей позы.
void USandsOfSorrowCharacterMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	const bool bGroundCrouch = IsMovingOnGround() && IsCrouching();
	Super::CalcVelocity(DeltaTime, bGroundCrouch ? CrouchFriction : Friction, bFluid,
		bGroundCrouch ? CrouchBraking : BrakingDeceleration);
}

// Отдельное тормозное трение стояния не должно отменять точность остановки crouch.
void USandsOfSorrowCharacterMovementComponent::ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration)
{
	const bool bGroundCrouch = IsMovingOnGround() && IsCrouching();
	Super::ApplyVelocityBraking(DeltaTime, bGroundCrouch ? CrouchFriction : Friction,
		bGroundCrouch ? CrouchBraking : BrakingDeceleration);
}
