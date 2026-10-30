#include "Characters/SandsOfSorrowPlayerCharacter.h"

#include "Characters/SandsOfSorrowCharacterMovementComponent.h"

#include "Camera/CameraComponent.h"
#include "Characters/SandsOfSorrowGameplayPlayerController.h"
#include "Characters/SandsOfSorrowCharacterMovementSettings.h"
#include "Characters/SandsOfSorrowCameraSettings.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Puzzles/Light/SandsOfSorrowLightCollision.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Interaction/SandsOfSorrowInteractionComponent.h"
#include "SandsOfSorrow.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Class.h"

// Создаёт компоненты; Blueprint назначает игровой ассет настроек, ввод и представление.
ASandsOfSorrowPlayerCharacter::ASandsOfSorrowPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<USandsOfSorrowCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	InteractionComponent = CreateDefaultSubobject<USandsOfSorrowInteractionComponent>(TEXT("InteractionComponent"));
	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);
	// В прототипе игрок не перекрывает луч своим телом.
	GetCapsuleComponent()->SetCollisionResponseToChannel(SandsOfSorrowLightCollision::Channel, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(SandsOfSorrowLightCollision::Channel, ECR_Ignore);
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	// Нативный CDO задаёт резервный профиль; игровой DA может отличаться, в том числе ориентацией.
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
		StandingCameraTargetOffset = CameraBoom->TargetOffset;
		return;
	}
	StandingCameraTargetOffset = CameraBoom->TargetOffset;
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
	if (IsActionValid(CrouchAction, EInputActionValueType::Boolean, TEXT("CrouchAction")))
	{
		EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Started, this, &ThisClass::BeginCrouchHold);
		EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Completed, this, &ThisClass::EndCrouchHold);
		EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Canceled, this, &ThisClass::EndCrouchHold);
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

// Проверка выполняется при обработке запроса, после возможного изменения препятствий со времени Started.
void ASandsOfSorrowPlayerCharacter::CheckJumpInput(float DeltaTime)
{
	if (!bPressedJump)
	{
		Super::CheckJumpInput(DeltaTime);
		return;
	}
	// Правила прыжка принадлежат C++; BP-переопределение могло бы изменить мир между проверкой и импульсом.
	const UFunction* JumpEligibility = FindFunction(TEXT("CanJumpInternal"));
	if (!JumpEligibility || !JumpEligibility->Script.IsEmpty())
	{
		if (!bJumpOverrideErrorReported)
		{
			UE_LOG(LogSandsOfSorrow, Error, TEXT("%s: Blueprint CanJump overrides are unsupported; keep jump eligibility in C++."), *GetNameSafe(this));
			bJumpOverrideErrorReported = true;
		}
		StopJumping();
		return;
	}
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement || !Movement->IsMovingOnGround() || !Movement->IsJumpAllowed()
		|| JumpMaxCount <= 0 || JumpCurrentCount >= JumpMaxCount
		|| (Movement->bConstrainToPlane && FMath::IsNearlyEqual(FMath::Abs(Movement->GetPlaneConstraintNormal().Z), 1.0))
		|| !HasJumpClearance(true))
	{
		StopJumping();
		return;
	}
	const bool bStartedCrouched = bIsCrouched;
	const bool bPreviousRequest = Movement->bWantsToCrouch;
	Movement->bWantsToCrouch = false;
	if (bStartedCrouched)
	{
		Movement->UnCrouch(false);
	}
	// Штатная смена капсулы может вызвать внешние callbacks; перед импульсом проверяем мир повторно.
	if (!IsValid(this) || !IsValid(Movement))
	{
		return;
	}
	if (bIsCrouched || !Movement->IsMovingOnGround() || !HasJumpClearance() || !CanJump())
	{
		Movement->bWantsToCrouch = bPreviousRequest;
		if (bStartedCrouched && !bIsCrouched)
		{
			Movement->Crouch(false);
		}
		if (IsValid(this))
		{
			StopJumping();
		}
		return;
	}
	Super::CheckJumpInput(DeltaTime);
	if (IsValid(this) && IsValid(Movement) && !Movement->IsFalling())
	{
		Movement->bWantsToCrouch = bPreviousRequest;
		if (bStartedCrouched && !bIsCrouched)
		{
			Movement->Crouch(false);
		}
	}
	// Любой результат потребляет одну попытку. Удержание Space не создаёт новый Started.
	if (IsValid(this))
	{
		StopJumping();
	}
}

// Высота рассчитывается из действующей физики; геометрия использует полный масштаб и ответы капсулы.
bool ASandsOfSorrowPlayerCharacter::HasJumpClearance(bool bStandingTakeoff) const
{
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const UWorld* World = GetWorld();
	if (!Movement || !Capsule || !World || !Capsule->IsQueryCollisionEnabled())
	{
		return false;
	}
	const float GravityZ = Movement->GetGravityZ();
	const float JumpSpeed = Movement->JumpZVelocity;
	if (!FMath::IsFinite(GravityZ) || GravityZ >= -UE_KINDA_SMALL_NUMBER
		|| !FMath::IsFinite(JumpSpeed) || JumpSpeed <= 0.0f || JumpSpeed > 100000.0f)
	{
		return false;
	}
	// DoJump сохраняет большую входящую вертикальную скорость; объём должен покрывать и этот случай.
	const double BaseVelocityZ = Movement->GetImpartedMovementBaseVelocity().Z;
	const double TakeoffSpeed = FMath::Max(static_cast<double>(JumpSpeed), Movement->Velocity.Z)
		+ FMath::Max(0.0, BaseVelocityZ);
	const double Height = FMath::Square(TakeoffSpeed) / (-2.0 * GravityZ);
	constexpr float ClearanceMarginCm = 2.0f;
	// Ограничение защищает запрос физики от неподдерживаемой гравитации/чрезмерного объёма.
	constexpr float MaximumClearanceHeightCm = 100000.0f;
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float CurrentHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const ASandsOfSorrowPlayerCharacter* Defaults = GetClass()->GetDefaultObject<ASandsOfSorrowPlayerCharacter>();
	const float HalfHeight = bStandingTakeoff && bIsCrouched
		? Defaults->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() * Capsule->GetShapeScale()
		: CurrentHalfHeight;
	if (!FMath::IsFinite(BaseVelocityZ) || !FMath::IsFinite(TakeoffSpeed) || !FMath::IsFinite(Height) || Height < 0.0f || Height > MaximumClearanceHeightCm
		|| !FMath::IsFinite(Radius) || !FMath::IsFinite(HalfHeight) || Radius <= 0.0f || HalfHeight < Radius)
	{
		return false;
	}
	const FVector Start = Capsule->GetComponentLocation() + FVector::UpVector * (HalfHeight - CurrentHalfHeight);
	const FVector End = Start + FVector::UpVector * (Height + ClearanceMarginCm);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SandsJumpClearance), false, this);
	Params.bFindInitialOverlaps = true;
	const FCollisionResponseParams Responses(Capsule->GetCollisionResponseToChannels());
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
	if (World->OverlapBlockingTestByChannel(Start, Capsule->GetComponentQuat(),
		Capsule->GetCollisionObjectType(), Shape, Params, Responses))
	{
		return false;
	}
	FHitResult Hit;
	return !World->SweepSingleByChannel(Hit, Start, End, Capsule->GetComponentQuat(),
		Capsule->GetCollisionObjectType(), Shape, Params, Responses);
}

// Сход с края и весь прыжок используют один фактический режим без отдельного флага полёта.
bool ASandsOfSorrowPlayerCharacter::IsGameplayInteractionAllowed() const
{
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	return Movement && !Movement->IsFalling();
}

// Начало удержания требует отдельного физического нажатия, независимо от разрешения E.
void ASandsOfSorrowPlayerCharacter::BeginCrouchHold()
{
	ASandsOfSorrowGameplayPlayerController* Player = Cast<ASandsOfSorrowGameplayPlayerController>(Controller);
	if (Player && Player->ConsumeCrouchPress())
	{
		SetCrouchHeld(true);
	}
}

// Отпускание и отмена одинаково снимают запрос; потолок проверяет CharacterMovement.
void ASandsOfSorrowPlayerCharacter::EndCrouchHold()
{
	SetCrouchHeld(false);
}

// Публичный запрос не подменяет фактический bIsCrouched.
void ASandsOfSorrowPlayerCharacter::SetCrouchHeld(bool bHeld)
{
	bCrouchHeld = bHeld;
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (Movement && Movement->IsMovingOnGround())
	{
		Movement->bWantsToCrouch = bHeld;
	}
}

// Компенсирует скачок центра капсулы; затем единственное смещение плавно возвращается к DA.
void ASandsOfSorrowPlayerCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	if (IsValid(this) && CameraBoom)
	{
		CameraBoom->TargetOffset.Z += ScaledHalfHeightAdjust;
	}
}

// Обратная компенсация сохраняет непрерывность камеры и при прерванной смене позы.
void ASandsOfSorrowPlayerCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	if (IsValid(this) && CameraBoom)
	{
		CameraBoom->TargetOffset.Z -= ScaledHalfHeightAdjust;
	}
}

// Плечо сохраняет штатные lag и collision; добавочная интерполяция нужна только при смене высоты.
void ASandsOfSorrowPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	constexpr float CrouchCameraInterpolationSpeed = 12.0f;
	if (CameraBoom && !CameraBoom->TargetOffset.Equals(StandingCameraTargetOffset, 0.001))
	{
		CameraBoom->TargetOffset = FMath::VInterpTo(CameraBoom->TargetOffset, StandingCameraTargetOffset,
			DeltaSeconds, CrouchCameraInterpolationSpeed);
		if (CameraBoom->TargetOffset.Equals(StandingCameraTargetOffset, 0.001))
		{
			CameraBoom->TargetOffset = StandingCameraTargetOffset;
		}
	}
}

// Старый Pawn не переносит удержание следующему владельцу.
void ASandsOfSorrowPlayerCharacter::UnPossessed()
{
	SetCrouchHeld(false);
	Super::UnPossessed();
}

// Завершение мира не сохраняет запросы управления.
void ASandsOfSorrowPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetCrouchHeld(false);
	Super::EndPlay(EndPlayReason);
}
