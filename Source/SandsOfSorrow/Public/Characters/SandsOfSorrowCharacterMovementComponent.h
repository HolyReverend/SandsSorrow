#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SandsOfSorrowCharacterMovementComponent.generated.h"

/** Сохраняет стандартную физику; strafe-разворот следует за камерой только при движении. */
UCLASS()
class SANDSOFSORROW_API USandsOfSorrowCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

protected:
	virtual void PhysicsRotation(float DeltaTime) override;
};
