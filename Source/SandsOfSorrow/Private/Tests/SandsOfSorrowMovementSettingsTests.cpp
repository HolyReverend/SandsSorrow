#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Characters/SandsOfSorrowCharacterMovementSettings.h"
#include "Characters/SandsOfSorrowPlayerCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include <limits>

namespace SandsMovementSettingsTests
{
// Изолированный игровой мир проверяет настоящие Actor/компоненты и освобождается после каждого сценария.
struct FTestWorld
{
	UWorld* World = nullptr;

	FTestWorld()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
	}

	~FTestWorld()
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSandsMovementSettingsApplicationTest,
	"SandsOfSorrow.MovementSettings.Application",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Проверяет реальные присваивания и отсутствие побочного сброса скорости при применении конфигурации.
bool FSandsMovementSettingsApplicationTest::RunTest(const FString& Parameters)
{
	SandsMovementSettingsTests::FTestWorld Scene;
	ACharacter* Character = Scene.World->SpawnActor<ACharacter>();
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	TStrongObjectPtr<USandsOfSorrowCharacterMovementSettings> Settings(NewObject<USandsOfSorrowCharacterMovementSettings>());
	Settings->MaxWalkSpeed = 321.0f;
	Settings->MinAnalogWalkSpeed = 12.0f;
	Settings->MaxAcceleration = 777.0f;
	Settings->BrakingDecelerationWalking = 456.0f;
	Settings->GroundFriction = 3.0f;
	Settings->bUseSeparateBrakingFriction = true;
	Settings->BrakingFriction = 4.0f;
	Settings->BrakingFrictionFactor = 0.5f;
	Settings->bOrientRotationToMovement = false;
	Settings->bUseControllerDesiredRotation = true;
	Settings->bUseControllerRotationYaw = true;
	Settings->RotationRateYaw = 234.0f;
	Settings->JumpZVelocity = 432.0f;
	Settings->JumpMaxCount = 2;
	Settings->JumpMaxHoldTime = 0.1f;
	Settings->GravityScale = 1.2f;
	Settings->AirControl = 0.12f;
	Settings->AirControlBoostMultiplier = 1.5f;
	Settings->AirControlBoostVelocityThreshold = 34.0f;
	Settings->FallingLateralFriction = 0.4f;
	Settings->BrakingDecelerationFalling = 567.0f;
	const FVector VelocityBefore(123.0, 4.0, 0.0);
	Movement->Velocity = VelocityBefore;
	const FVector PositionBefore = Character->GetActorLocation();
	FString Error;
	TestTrue(TEXT("Valid configuration applies"), Settings->ApplyTo(*Character, Error));
	TestTrue(TEXT("No stale error"), Error.IsEmpty());
	TestEqual(TEXT("Walk speed"), Movement->MaxWalkSpeed, 321.0f);
	TestEqual(TEXT("Analog floor"), Movement->MinAnalogWalkSpeed, 12.0f);
	TestEqual(TEXT("Acceleration"), Movement->MaxAcceleration, 777.0f);
	TestEqual(TEXT("Walking braking"), Movement->BrakingDecelerationWalking, 456.0f);
	TestEqual(TEXT("Ground friction"), Movement->GroundFriction, 3.0f);
	TestTrue(TEXT("Separate braking"), Movement->bUseSeparateBrakingFriction);
	TestEqual(TEXT("Braking friction"), Movement->BrakingFriction, 4.0f);
	TestEqual(TEXT("Braking factor"), Movement->BrakingFrictionFactor, 0.5f);
	TestFalse(TEXT("Orient to movement disabled"), Movement->bOrientRotationToMovement);
	TestTrue(TEXT("Controller desired rotation"), Movement->bUseControllerDesiredRotation);
	TestTrue(TEXT("Pawn controller yaw"), Character->bUseControllerRotationYaw);
	TestEqual(TEXT("Rotation yaw rate"), Movement->RotationRate.Yaw, 234.0);
	TestEqual(TEXT("Jump speed"), Movement->JumpZVelocity, 432.0f);
	TestEqual(TEXT("Jump count"), Character->JumpMaxCount, 2);
	TestEqual(TEXT("Jump hold"), Character->JumpMaxHoldTime, 0.1f);
	TestEqual(TEXT("Gravity"), Movement->GravityScale, 1.2f);
	TestEqual(TEXT("Air control"), Movement->AirControl, 0.12f);
	TestEqual(TEXT("Air boost"), Movement->AirControlBoostMultiplier, 1.5f);
	TestEqual(TEXT("Air boost threshold"), Movement->AirControlBoostVelocityThreshold, 34.0f);
	TestEqual(TEXT("Air friction"), Movement->FallingLateralFriction, 0.4f);
	TestEqual(TEXT("Air braking"), Movement->BrakingDecelerationFalling, 567.0f);
	TestEqual(TEXT("Velocity preserved"), Movement->Velocity, VelocityBefore);
	TestEqual(TEXT("Position preserved"), Character->GetActorLocation(), PositionBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSandsMovementSettingsValidationTest,
	"SandsOfSorrow.MovementSettings.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Некорректное поле в конце набора не должно менять даже первое корректное поле компонента.
bool FSandsMovementSettingsValidationTest::RunTest(const FString& Parameters)
{
	SandsMovementSettingsTests::FTestWorld Scene;
	ACharacter* Character = Scene.World->SpawnActor<ACharacter>();
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const float Before = Movement->MaxWalkSpeed;
	TStrongObjectPtr<USandsOfSorrowCharacterMovementSettings> Settings(NewObject<USandsOfSorrowCharacterMovementSettings>());
	Settings->MaxWalkSpeed = 333.0f;
	Settings->BrakingDecelerationFalling = std::numeric_limits<float>::quiet_NaN();
	FString Error;
	TestFalse(TEXT("NaN rejected before applying any field"), Settings->ApplyTo(*Character, Error));
	TestEqual(TEXT("No partial speed change"), Movement->MaxWalkSpeed, Before);
	TestTrue(TEXT("Error identifies field"), Error.Contains(TEXT("BrakingDecelerationFalling")));
	Settings->BrakingDecelerationFalling = 1500.0f;
	Settings->GroundFriction = std::numeric_limits<float>::infinity();
	TestFalse(TEXT("Infinity rejected"), Settings->IsValidConfiguration(Error));
	Settings->GroundFriction = -1.0f;
	TestFalse(TEXT("Negative friction rejected"), Settings->IsValidConfiguration(Error));
	Settings->GroundFriction = 8.0f;
	Settings->MaxAcceleration = std::numeric_limits<float>::max();
	TestFalse(TEXT("Unsafe finite magnitude rejected"), Settings->IsValidConfiguration(Error));
	Settings->MaxAcceleration = 1600.0f;
	Settings->MinAnalogWalkSpeed = 400.0f;
	TestFalse(TEXT("Analog floor above limit rejected"), Settings->IsValidConfiguration(Error));
	Settings->MinAnalogWalkSpeed = 20.0f;
	Settings->JumpMaxCount = 17;
	TestFalse(TEXT("Out-of-range integer rejected"), Settings->ApplyTo(*Character, Error));
	TestEqual(TEXT("Integer failure is also atomic"), Movement->MaxWalkSpeed, Before);
	Settings->JumpMaxCount = 1;
	TestTrue(TEXT("Repaired configuration applies"), Settings->ApplyTo(*Character, Error));
	TestTrue(TEXT("Successful validation clears prior error"), Error.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSandsMovementSettingsSourceTest,
	"SandsOfSorrow.MovementSettings.SourcePrecedence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Deferred spawn воспроизводит назначение DA и старые компонентные overrides до финальной сборки Actor.
bool FSandsMovementSettingsSourceTest::RunTest(const FString& Parameters)
{
	SandsMovementSettingsTests::FTestWorld Scene;
	TStrongObjectPtr<USandsOfSorrowCharacterMovementSettings> Settings(NewObject<USandsOfSorrowCharacterMovementSettings>());
	Settings->MaxWalkSpeed = 345.0f;
	Settings->BrakingDecelerationWalking = 678.0f;
	FObjectProperty* SettingsProperty = FindFProperty<FObjectProperty>(ASandsOfSorrowPlayerCharacter::StaticClass(), TEXT("MovementSettings"));
	if (!TestNotNull(TEXT("Serialized settings reference exists"), SettingsProperty))
	{
		return false;
	}
	ASandsOfSorrowPlayerCharacter* Character = Scene.World->SpawnActorDeferred<ASandsOfSorrowPlayerCharacter>(
		ASandsOfSorrowPlayerCharacter::StaticClass(), FTransform::Identity, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	Character->GetCharacterMovement()->MaxWalkSpeed = 999.0f;
	Character->GetCharacterMovement()->BrakingDecelerationWalking = 999.0f;
	SettingsProperty->SetObjectPropertyValue_InContainer(Character, Settings.Get());
	UGameplayStatics::FinishSpawningActor(Character, FTransform::Identity);
	TestEqual(TEXT("Asset overrides old component speed"), Character->GetCharacterMovement()->MaxWalkSpeed, 345.0f);
	TestEqual(TEXT("Asset overrides old component braking"), Character->GetCharacterMovement()->BrakingDecelerationWalking, 678.0f);
	TestTrue(TEXT("Actor retains assigned asset"), Character->GetMovementSettings() == Settings.Get());
	Settings->MaxWalkSpeed = 456.0f;
	TestEqual(TEXT("Existing simulation is not silently retuned"), Character->GetCharacterMovement()->MaxWalkSpeed, 345.0f);
	ASandsOfSorrowPlayerCharacter* Next = Scene.World->SpawnActorDeferred<ASandsOfSorrowPlayerCharacter>(
		ASandsOfSorrowPlayerCharacter::StaticClass(), FTransform(FVector(500, 0, 0)), nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	SettingsProperty->SetObjectPropertyValue_InContainer(Next, Settings.Get());
	UGameplayStatics::FinishSpawningActor(Next, FTransform(FVector(500, 0, 0)));
	TestEqual(TEXT("Next spawn reads edited asset"), Next->GetCharacterMovement()->MaxWalkSpeed, 456.0f);
	return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSandsMovementSettingsTooltipsTest,
	"SandsOfSorrow.MovementSettings.RussianTooltips",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Проверяется метадата после UHT: наличие текста в исходном файле само по себе не доказывает подсказку редактора.
bool FSandsMovementSettingsTooltipsTest::RunTest(const FString& Parameters)
{
	int32 Checked = 0;
	for (TFieldIterator<FProperty> It(USandsOfSorrowCharacterMovementSettings::StaticClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		const FProperty* Property = *It;
		const FString Tooltip = Property->GetMetaData(TEXT("ToolTip"));
		bool bHasRussianText = false;
		for (const TCHAR Character : Tooltip)
		{
			bHasRussianText |= Character >= 0x0400 && Character <= 0x04FF;
		}
		TestTrue(*FString::Printf(TEXT("Russian tooltip: %s"), *Property->GetName()), bHasRussianText);
		++Checked;
	}
	TestTrue(TEXT("Editable settings were inspected"), Checked > 0);
	return true;
}
#endif

#endif
