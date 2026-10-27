#include "Characters/SandsOfSorrowPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Characters/SandsOfSorrowGameplayPlayerController.h"
#include "Characters/SandsOfSorrowCharacterMovementSettings.h"
#include "Characters/SandsOfSorrowCameraSettings.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Puzzles/Light/SandsOfSorrowLightCollision.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Interaction/SandsOfSorrowInteractionComponent.h"
#include "SandsOfSorrow.h"
#include "UObject/UObjectGlobals.h"

// Создаёт компоненты; Blueprint назначает игровой ассет настроек, ввод и представление.
ASandsOfSorrowPlayerCharacter::ASandsOfSorrowPlayerCharacter()
{
	InteractionComponent = CreateDefaultSubobject<USandsOfSorrowInteractionComponent>(TEXT("InteractionComponent"));
	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);
	// В прототипе игрок не перекрывает луч своим телом.
	GetCapsuleComponent()->SetCollisionResponseToChannel(SandsOfSorrowLightCollision::Channel, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(SandsOfSorrowLightCollision::Channel, ECR_Ignore);
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	// Нативный CDO конфигурации задаёт тот же профиль, что и игровой DA; ассеты здесь не загружаются.
	ApplyMovementSettings();

	// Плечо следует за взглядом и сокращается у препятствий; камера наследует его положение.
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetRootComponent());
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	ApplyCameraSettings();
}

void ASandsOfSorrowPlayerCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyMovementSettings();
	ApplyCameraSettings();
}

void ASandsOfSorrowPlayerCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// Сериализованные overrides и Construction Script уже применены: финальные параметры берутся из DA.
	ApplyMovementSettings();
	ApplyCameraSettings();
	// Выполняется после сборки BP-компонентов, один раз вместо поиска каждый кадр.
	TInlineComponentArray<UPrimitiveComponent*> Primitives;
	GetComponents(Primitives);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		Primitive->SetCollisionResponseToChannel(SandsOfSorrowLightCollision::Channel, ECR_Ignore);
	}
}

void ASandsOfSorrowPlayerCharacter::ApplyMovementSettings()
{
	const USandsOfSorrowCharacterMovementSettings* Defaults = GetDefault<USandsOfSorrowCharacterMovementSettings>();
	const USandsOfSorrowCharacterMovementSettings* Settings = MovementSettings ? MovementSettings.Get() : Defaults;
	FString Error;
	if (!Settings->ApplyTo(*this, Error))
	{
		if (!bMovementSettingsErrorReported)
		{
			UE_LOG(LogSandsOfSorrow, Error, TEXT("%s: invalid movement settings %s: %s Using native defaults."),
				*GetNameSafe(this), *GetPathNameSafe(Settings), *Error);
			bMovementSettingsErrorReported = true;
		}
		FString DefaultError;
		if (!Defaults->ApplyTo(*this, DefaultError))
		{
			UE_LOG(LogSandsOfSorrow, Error, TEXT("%s: native movement defaults failed: %s"), *GetNameSafe(this), *DefaultError);
		}
		return;
	}
	UE_LOG(LogSandsOfSorrow, Verbose, TEXT("%s: applied movement settings %s (speed=%g, walking braking=%g, friction factor=%g)."),
		*GetNameSafe(this), *GetPathNameSafe(Settings), Settings->MaxWalkSpeed,
		Settings->BrakingDecelerationWalking, Settings->BrakingFrictionFactor);
}

// Применяет DA после сериализованных overrides; привязки компонентов и управление взглядом остаются прежними.
void ASandsOfSorrowPlayerCharacter::ApplyCameraSettings()
{
	const USandsOfSorrowCameraSettings* Defaults = GetDefault<USandsOfSorrowCameraSettings>();
	const USandsOfSorrowCameraSettings* Settings = CameraSettings ? CameraSettings.Get() : Defaults;
	FString Error;
	if (!Settings->ApplyTo(*CameraBoom, *FollowCamera, Error))
	{
		if (!bCameraSettingsErrorReported)
		{
			UE_LOG(LogSandsOfSorrow, Error, TEXT("%s: invalid camera settings %s: %s Using native defaults."),
				*GetNameSafe(this), *GetPathNameSafe(Settings), *Error);
			bCameraSettingsErrorReported = true;
		}
		FString DefaultError;
		if (!Defaults->ApplyTo(*CameraBoom, *FollowCamera, DefaultError))
		{
			UE_LOG(LogSandsOfSorrow, Error, TEXT("%s: native camera defaults failed: %s"), *GetNameSafe(this), *DefaultError);
		}
		return;
	}
	UE_LOG(LogSandsOfSorrow, Verbose, TEXT("%s: applied camera settings %s (arm=%g, FOV=%g, target Z=%g)."),
		*GetNameSafe(this), *GetPathNameSafe(Settings), Settings->TargetArmLength, Settings->FieldOfView, Settings->TargetOffset.Z);
}

// Проверяет типы назначенных действий и связывает их с обработчиками персонажа.
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

// Started обращается к компоненту только с разрешением свежего нажатия от контроллера.
void ASandsOfSorrowPlayerCharacter::Interact()
{
	ASandsOfSorrowGameplayPlayerController* PlayerController = Cast<ASandsOfSorrowGameplayPlayerController>(GetController());
	if (PlayerController && PlayerController->ConsumeInteractionPress())
	{
		InteractionComponent->RequestInteraction();
	}
}

// Переводит оси движения в горизонтальные направления относительно взгляда.
void ASandsOfSorrowPlayerCharacter::Move(const FInputActionValue& Value)
{
	if (!Controller)
	{
		return;
	}

	// Ограничение длины вектора не даёт диагонали увеличивать скорость.
	const FVector2D Input = Value.Get<FVector2D>().GetClampedToMaxSize(1.0);
	const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
	const FRotationMatrix YawMatrix(YawRotation);
	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::X), Input.Y);
	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::Y), Input.X);
}

// Применяет чувствительность к смещению мыши без умножения на время кадра.
void ASandsOfSorrowPlayerCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();
	AddControllerYawInput(Input.X * LookSensitivity.X);
	AddControllerPitchInput(Input.Y * LookSensitivity.Y);
}
