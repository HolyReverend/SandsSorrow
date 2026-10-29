#include "Characters/SandsOfSorrowCharacterMovementComponent.h"

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
