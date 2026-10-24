#include "Puzzles/Light/SandsOfSorrowLightCollision.h"

#include "Components/BoxComponent.h"
#include "Engine/HitResult.h"

bool SandsOfSorrowLightCollision::HasSupportedTransform(const USceneComponent* Component)
{
	if (!IsValid(Component))
	{
		return false;
	}
	const FVector Scale = Component->GetComponentScale();
	return !Scale.ContainsNaN() && Scale.GetMin() > UE_KINDA_SMALL_NUMBER
		&& FMath::IsNearlyEqual(Scale.X, Scale.Y, UE_KINDA_SMALL_NUMBER)
		&& FMath::IsNearlyEqual(Scale.X, Scale.Z, UE_KINDA_SMALL_NUMBER);
}

bool SandsOfSorrowLightCollision::IsFrontFaceHit(const UBoxComponent* Surface, const FHitResult& Hit, const FVector& IncomingDirection)
{
	if (!Hit.bBlockingHit || Hit.bStartPenetrating || Hit.GetComponent() != Surface
		|| !HasSupportedTransform(Surface) || IncomingDirection.ContainsNaN())
	{
		return false;
	}
	const FVector Direction = IncomingDirection.GetSafeNormal();
	const FVector Normal = Surface->GetForwardVector();
	return !Direction.IsNearlyZero() && FVector::DotProduct(Hit.ImpactNormal, Normal) > 0.999
		&& FVector::DotProduct(Direction, Normal) < -0.001;
}
