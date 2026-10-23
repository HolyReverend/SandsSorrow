#include "Characters/SandsOfSorrowPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Characters/SandsOfSorrowGameplayPlayerController.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Interaction/SandsOfSorrowInteractionComponent.h"
#include "SandsOfSorrow.h"

ASandsOfSorrowPlayerCharacter::ASandsOfSorrowPlayerCharacter()
{
	InteractionComponent = CreateDefaultSubobject<USandsOfSorrowInteractionComponent>(TEXT("InteractionComponent"));
	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	JumpMaxCount = 1;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	Movement->JumpZVelocity = 500.0f;
	Movement->AirControl = 0.35f;
	Movement->MaxWalkSpeed = 500.0f;
	Movement->MinAnalogWalkSpeed = 20.0f;
	Movement->BrakingDecelerationWalking = 2000.0f;
	Movement->BrakingDecelerationFalling = 1500.0f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetRootComponent());
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

void ASandsOfSorrowPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogSandsOfSorrow, Error, TEXT("%s requires an EnhancedInputComponent."), *GetNameSafe(this));
		return;
	}

	const auto IsActionValid = [this](const UInputAction* Action, EInputActionValueType ExpectedType, const TCHAR* PropertyName)
	{
		if (!Action || Action->ValueType != ExpectedType)
		{
			UE_LOG(LogSandsOfSorrow, Error,
				TEXT("%s: assign %s with value type %s in the character defaults."),
				*GetNameSafe(this), PropertyName, *UEnum::GetValueAsString(ExpectedType));
			return false;
		}
		return true;
	};

	if (IsActionValid(MoveAction, EInputActionValueType::Axis2D, TEXT("MoveAction")))
	{
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASandsOfSorrowPlayerCharacter::Move);
	}
	if (IsActionValid(LookAction, EInputActionValueType::Axis2D, TEXT("LookAction")))
	{
		EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &ASandsOfSorrowPlayerCharacter::Look);
	}
	if (IsActionValid(JumpAction, EInputActionValueType::Boolean, TEXT("JumpAction")))
	{
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Canceled, this, &ACharacter::StopJumping);
	}
	if (IsActionValid(InteractAction, EInputActionValueType::Boolean, TEXT("InteractAction")))
	{
		EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &ASandsOfSorrowPlayerCharacter::Interact);
	}
}

void ASandsOfSorrowPlayerCharacter::Interact()
{
	ASandsOfSorrowGameplayPlayerController* PlayerController = Cast<ASandsOfSorrowGameplayPlayerController>(GetController());
	if (PlayerController && PlayerController->ConsumeInteractionPress())
	{
		InteractionComponent->RequestInteraction();
	}
}

void ASandsOfSorrowPlayerCharacter::Move(const FInputActionValue& Value)
{
	if (!Controller)
	{
		return;
	}

	const FVector2D Input = Value.Get<FVector2D>().GetClampedToMaxSize(1.0);
	const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
	const FRotationMatrix YawMatrix(YawRotation);
	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::X), Input.Y);
	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::Y), Input.X);
}

void ASandsOfSorrowPlayerCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();
	AddControllerYawInput(Input.X * LookSensitivity.X);
	AddControllerPitchInput(Input.Y * LookSensitivity.Y);
}
