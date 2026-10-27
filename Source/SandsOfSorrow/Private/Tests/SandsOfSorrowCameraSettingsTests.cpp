#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Camera/CameraComponent.h"
#include "Characters/SandsOfSorrowCameraSettings.h"
#include "Characters/SandsOfSorrowPlayerCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include <limits>

namespace SandsCameraSettingsTests
{
// Изолированный World позволяет проверить настоящий порядок сборки Pawn и освобождается после теста.
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSandsCameraSettingsApplicationTest,
	"SandsOfSorrow.CameraSettings.Application", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Проверяет все параметры и сохранение привязки/политики управления существующих компонентов.
bool FSandsCameraSettingsApplicationTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<USpringArmComponent> Arm(NewObject<USpringArmComponent>());
	TStrongObjectPtr<UCameraComponent> Camera(NewObject<UCameraComponent>());
	TStrongObjectPtr<USandsOfSorrowCameraSettings> Settings(NewObject<USandsOfSorrowCameraSettings>());
	Camera->SetupAttachment(Arm.Get(), USpringArmComponent::SocketName);
	Arm->bUsePawnControlRotation = true;
	Camera->bUsePawnControlRotation = false;
	Settings->FieldOfView = 67.0f;
	Settings->TargetArmLength = 234.0f;
	Settings->TargetOffset = FVector(1, 2, 33);
	Settings->SocketOffset = FVector(4, 25, 6);
	Settings->bDoCollisionTest = false;
	Settings->ProbeSize = 9.0f;
	Settings->bEnableCameraLag = true;
	Settings->CameraLagSpeed = 7.0f;
	Settings->CameraLagMaxDistance = 55.0f;
	Settings->bEnableCameraRotationLag = true;
	Settings->CameraRotationLagSpeed = 8.0f;
	FString Error;
	TestTrue(TEXT("Valid settings apply"), Settings->ApplyTo(*Arm, *Camera, Error));
	TestTrue(TEXT("No stale error"), Error.IsEmpty());
	TestEqual(TEXT("FOV"), Camera->FieldOfView, 67.0f);
	TestEqual(TEXT("Arm"), Arm->TargetArmLength, 234.0f);
	TestEqual(TEXT("Target offset"), Arm->TargetOffset, FVector(1, 2, 33));
	TestEqual(TEXT("Socket offset"), Arm->SocketOffset, FVector(4, 25, 6));
	TestFalse(TEXT("Collision setting"), Arm->bDoCollisionTest);
	TestEqual(TEXT("Probe"), Arm->ProbeSize, 9.0f);
	TestTrue(TEXT("Position lag"), Arm->bEnableCameraLag);
	TestEqual(TEXT("Position lag speed"), Arm->CameraLagSpeed, 7.0f);
	TestEqual(TEXT("Lag distance"), Arm->CameraLagMaxDistance, 55.0f);
	TestTrue(TEXT("Rotation lag"), Arm->bEnableCameraRotationLag);
	TestEqual(TEXT("Rotation lag speed"), Arm->CameraRotationLagSpeed, 8.0f);
	TestTrue(TEXT("Attachment retained"), Camera->GetAttachParent() == Arm.Get());
	TestTrue(TEXT("Arm follows control rotation"), Arm->bUsePawnControlRotation);
	TestFalse(TEXT("Camera control policy retained"), Camera->bUsePawnControlRotation);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSandsCameraSettingsValidationTest,
	"SandsOfSorrow.CameraSettings.Validation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Некорректные данные отвергаются до изменения FOV или плеча, включая ошибку последнего проверяемого offset.
bool FSandsCameraSettingsValidationTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<USpringArmComponent> Arm(NewObject<USpringArmComponent>());
	TStrongObjectPtr<UCameraComponent> Camera(NewObject<UCameraComponent>());
	TStrongObjectPtr<USandsOfSorrowCameraSettings> Settings(NewObject<USandsOfSorrowCameraSettings>());
	Camera->SetFieldOfView(93.0f);
	Arm->TargetArmLength = 321.0f;
	Settings->SocketOffset.X = std::numeric_limits<double>::quiet_NaN();
	FString Error;
	TestFalse(TEXT("NaN offset rejected"), Settings->ApplyTo(*Arm, *Camera, Error));
	TestEqual(TEXT("FOV unchanged on failure"), Camera->FieldOfView, 93.0f);
	TestEqual(TEXT("Arm unchanged on failure"), Arm->TargetArmLength, 321.0f);
	TestTrue(TEXT("Error identifies offset"), Error.Contains(TEXT("Offset")));
	Settings->SocketOffset = FVector::ZeroVector;
	Settings->FieldOfView = 180.0f;
	TestFalse(TEXT("Invalid projection rejected"), Settings->IsValidConfiguration(Error));
	Settings->FieldOfView = 80.0f;
	Settings->ProbeSize = -1.0f;
	TestFalse(TEXT("Negative probe rejected"), Settings->IsValidConfiguration(Error));
	Settings->ProbeSize = 12.0f;
	Settings->TargetArmLength = std::numeric_limits<float>::infinity();
	TestFalse(TEXT("Infinity rejected"), Settings->IsValidConfiguration(Error));
	Settings->TargetArmLength = 260.0f;
	Settings->TargetOffset.Z = std::numeric_limits<double>::max();
	TestFalse(TEXT("Unsafe finite offset rejected"), Settings->IsValidConfiguration(Error));
	Settings->TargetOffset = FVector::ZeroVector;
	TestTrue(TEXT("Repaired settings apply"), Settings->ApplyTo(*Arm, *Camera, Error));
	TestTrue(TEXT("Prior error cleared"), Error.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSandsCameraSettingsSourceTest,
	"SandsOfSorrow.CameraSettings.SourcePrecedence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Сериализованный DA должен заменить старые компонентные overrides и применяться к следующему Pawn после правки.
bool FSandsCameraSettingsSourceTest::RunTest(const FString& Parameters)
{
	SandsCameraSettingsTests::FTestWorld Scene;
	TStrongObjectPtr<USandsOfSorrowCameraSettings> Settings(NewObject<USandsOfSorrowCameraSettings>());
	Settings->FieldOfView = 63.0f;
	Settings->TargetArmLength = 222.0f;
	FObjectProperty* Property = FindFProperty<FObjectProperty>(ASandsOfSorrowPlayerCharacter::StaticClass(), TEXT("CameraSettings"));
	if (!TestNotNull(TEXT("Camera source property"), Property))
	{
		return false;
	}
	ASandsOfSorrowPlayerCharacter* Character = Scene.World->SpawnActorDeferred<ASandsOfSorrowPlayerCharacter>(
		ASandsOfSorrowPlayerCharacter::StaticClass(), FTransform::Identity, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	Character->GetFollowCamera()->SetFieldOfView(119.0f);
	Character->GetCameraBoom()->TargetArmLength = 999.0f;
	Property->SetObjectPropertyValue_InContainer(Character, Settings.Get());
	UGameplayStatics::FinishSpawningActor(Character, FTransform::Identity);
	TestTrue(TEXT("Assigned asset retained"), Character->GetCameraSettings() == Settings.Get());
	TestEqual(TEXT("Asset FOV wins"), Character->GetFollowCamera()->FieldOfView, 63.0f);
	TestEqual(TEXT("Asset arm wins"), Character->GetCameraBoom()->TargetArmLength, 222.0f);
	Settings->FieldOfView = 68.0f;
	TestEqual(TEXT("Existing camera is not silently retuned"), Character->GetFollowCamera()->FieldOfView, 63.0f);
	ASandsOfSorrowPlayerCharacter* Next = Scene.World->SpawnActorDeferred<ASandsOfSorrowPlayerCharacter>(
		ASandsOfSorrowPlayerCharacter::StaticClass(), FTransform(FVector(500, 0, 0)), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	Property->SetObjectPropertyValue_InContainer(Next, Settings.Get());
	UGameplayStatics::FinishSpawningActor(Next, FTransform(FVector(500, 0, 0)));
	TestEqual(TEXT("Next Pawn reads edited asset"), Next->GetFollowCamera()->FieldOfView, 68.0f);
	return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSandsCameraSettingsTooltipsTest,
	"SandsOfSorrow.CameraSettings.RussianTooltips", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Проверяется отражённая метаинформация после UHT для каждого параметра DA.
bool FSandsCameraSettingsTooltipsTest::RunTest(const FString& Parameters)
{
	int32 Checked = 0;
	for (TFieldIterator<FProperty> It(USandsOfSorrowCameraSettings::StaticClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		const FProperty* Property = *It;
		bool bHasRussianText = false;
		for (const TCHAR Character : Property->GetMetaData(TEXT("ToolTip")))
		{
			bHasRussianText |= Character >= 0x0400 && Character <= 0x04FF;
		}
		TestTrue(*FString::Printf(TEXT("Russian tooltip: %s"), *Property->GetName()), bHasRussianText);
		++Checked;
	}
	TestTrue(TEXT("Settings inspected"), Checked > 0);
	return true;
}
#endif
#endif
