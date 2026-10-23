#include "Characters/SandsOfSorrowGameplayPlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "Characters/SandsOfSorrowPlayerCharacter.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "InputKeyEventArgs.h"
#include "Interaction/SandsOfSorrowInteractionComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UI/SandsOfSorrowInteractionPromptWidget.h"
#include "Widgets/SViewport.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "SandsOfSorrow.h"

void ASandsOfSorrowGameplayPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!IsLocalPlayerController())
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (!Subsystem)
	{
		UE_LOG(LogSandsOfSorrow, Error, TEXT("%s: Enhanced Input local player subsystem is unavailable."), *GetNameSafe(this));
		return;
	}

	if (OwnedMappingContext && OwnedMappingContext == PlayerMappingContext && MappingSubsystem.Get() == Subsystem
		&& Subsystem->HasMappingContext(OwnedMappingContext))
	{
		return;
	}
	RemoveOwnedMappingContext();

	if (!PlayerMappingContext)
	{
		UE_LOG(LogSandsOfSorrow, Error, TEXT("%s: assign PlayerMappingContext in the controller defaults."), *GetNameSafe(this));
		return;
	}

	// A context installed by another owner must not be removed by this controller.
	if (Subsystem->HasMappingContext(PlayerMappingContext))
	{
		UE_LOG(LogSandsOfSorrow, Warning, TEXT("%s: %s is already mapped by another owner; leaving it unchanged."),
			*GetNameSafe(this), *GetNameSafe(PlayerMappingContext));
		return;
	}

	Subsystem->AddMappingContext(PlayerMappingContext, 0);
	OwnedMappingContext = PlayerMappingContext;
	MappingSubsystem = Subsystem;
}

void ASandsOfSorrowGameplayPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DetachInteraction();
	if (FSlateApplication::IsInitialized() && ApplicationActivationHandle.IsValid())
	{
		FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(ApplicationActivationHandle);
	}
	ApplicationActivationHandle.Reset();
	if (InteractionPrompt)
	{
		InteractionPrompt->RemoveFromParent();
		InteractionPrompt = nullptr;
	}
	RemoveOwnedMappingContext();
	Super::EndPlay(EndPlayReason);
}

void ASandsOfSorrowGameplayPlayerController::RemoveOwnedMappingContext()
{
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = MappingSubsystem.Get())
	{
		if (OwnedMappingContext)
		{
			Subsystem->RemoveMappingContext(OwnedMappingContext);
		}
	}
	OwnedMappingContext = nullptr;
	MappingSubsystem.Reset();
}

ASandsOfSorrowGameplayPlayerController::ASandsOfSorrowGameplayPlayerController()
{
	PrimaryActorTick.bTickEvenWhenPaused = true;
	bShouldPerformFullTickWhenPaused = true;
}

void ASandsOfSorrowGameplayPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController() && FSlateApplication::IsInitialized())
	{
		bApplicationActive = FSlateApplication::Get().IsActive();
		ApplicationActivationHandle = FSlateApplication::Get().OnApplicationActivationStateChanged()
			.AddUObject(this, &ThisClass::HandleApplicationActivation);
	}
	EnsureInteractionPrompt();
	SynchronizeInteraction();
}

void ASandsOfSorrowGameplayPlayerController::EnsureInteractionPrompt()
{
	if (!IsLocalPlayerController() || !HasActorBegunPlay() || InteractionPrompt)
	{
		return;
	}
	if (!InteractionPromptClass)
	{
		if (!bReportedPromptError)
		{
			UE_LOG(LogSandsOfSorrow, Error, TEXT("%s: assign InteractionPromptClass in the controller defaults."), *GetNameSafe(this));
			bReportedPromptError = true;
		}
		return;
	}
	InteractionPrompt = CreateWidget<USandsOfSorrowInteractionPromptWidget>(this, InteractionPromptClass);
	if (InteractionPrompt)
	{
		InteractionPrompt->AddToPlayerScreen();
	}
}

bool ASandsOfSorrowGameplayPlayerController::IsGameplayInteractionAvailable() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UGameViewportClient* Viewport = LocalPlayer ? LocalPlayer->ViewportClient : nullptr;
	if (!IsLocalController() || !IsValid(GetPawn()) || !InteractionPrompt || !InteractionPrompt->IsInViewport()
		|| bInteractionUIActive || !bApplicationActive || IsMoveInputIgnored()
		|| !GetWorld() || UGameplayStatics::IsGamePaused(this) || !Viewport || Viewport->IgnoreInput()
		|| !FSlateApplication::IsInitialized())
	{
		return false;
	}
	const TSharedPtr<SViewport> Widget = Viewport->GetGameViewportWidget();
	return Widget.IsValid() && FSlateApplication::Get().IsActive()
		&& (Widget->HasAnyUserFocus().IsSet()
			|| FSlateApplication::Get().HasFocusedDescendants(Widget.ToSharedRef()));
}

void ASandsOfSorrowGameplayPlayerController::DetachInteraction()
{
	bFreshInteractionPress = false;
	if (USandsOfSorrowInteractionComponent* Old = ObservedInteraction.Get())
	{
		Old->OnFocusChanged.RemoveDynamic(this, &ThisClass::HandleFocusChanged);
		Old->SetInteractionInputEnabled(false);
	}
	ObservedInteraction.Reset();
	if (InteractionPrompt)
	{
		InteractionPrompt->SetPromptState(false, false, FText::GetEmpty());
	}
}

void ASandsOfSorrowGameplayPlayerController::SynchronizeInteraction()
{
	if (!IsLocalController())
	{
		return;
	}
	const ASandsOfSorrowPlayerCharacter* GameplayCharacter = Cast<ASandsOfSorrowPlayerCharacter>(GetPawn());
	USandsOfSorrowInteractionComponent* Current = IsValid(GameplayCharacter) ? GameplayCharacter->GetInteractionComponent() : nullptr;
	if (ObservedInteraction.Get() != Current)
	{
		DetachInteraction();
		if (IsValid(Current))
		{
			ObservedInteraction = Current;
			Current->OnFocusChanged.AddUniqueDynamic(this, &ThisClass::HandleFocusChanged);
		}
	}
	const bool bAvailable = IsGameplayInteractionAvailable();
	if (!bAvailable)
	{
		bFreshInteractionPress = false;
	}
	if (IsValid(Current))
	{
		Current->SetInteractionInputEnabled(bAvailable);
	}
	UpdatePrompt();
}

void ASandsOfSorrowGameplayPlayerController::UpdatePrompt()
{
	if (InteractionPrompt)
	{
		USandsOfSorrowInteractionComponent* Current = ObservedInteraction.Get();
		InteractionPrompt->SetPromptState(IsGameplayInteractionAvailable() && IsValid(Current),
			Current && IsValid(Current->GetFocusedActor()), Current ? Current->GetFocusedText() : FText::GetEmpty());
	}
}

void ASandsOfSorrowGameplayPlayerController::HandleFocusChanged(AActor* Target, FText InteractionText)
{
	UpdatePrompt();
}

void ASandsOfSorrowGameplayPlayerController::SetPawn(APawn* InPawn)
{
	if (GetPawn() != InPawn)
	{
		DetachInteraction();
	}
	Super::SetPawn(InPawn);
	SynchronizeInteraction();
}

void ASandsOfSorrowGameplayPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	SynchronizeInteraction();
}

void ASandsOfSorrowGameplayPlayerController::PreProcessInput(float DeltaTime, bool bGamePaused)
{
	SynchronizeInteraction();
	Super::PreProcessInput(DeltaTime, bGamePaused);
}

void ASandsOfSorrowGameplayPlayerController::PostProcessInput(float DeltaTime, bool bGamePaused)
{
	Super::PostProcessInput(DeltaTime, bGamePaused);
	// Never carry a physical press across frames, input-mode changes or an IMC rebuild.
	bFreshInteractionPress = false;
	SynchronizeInteraction();
}

bool ASandsOfSorrowGameplayPlayerController::IsInteractionKey(const FKey& Key) const
{
	const ASandsOfSorrowPlayerCharacter* GameplayCharacter = Cast<ASandsOfSorrowPlayerCharacter>(GetPawn());
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	return GameplayCharacter && GameplayCharacter->GetInteractAction() && Subsystem
		&& Subsystem->QueryKeysMappedToAction(GameplayCharacter->GetInteractAction()).Contains(Key);
}

bool ASandsOfSorrowGameplayPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	if (IsInteractionKey(Params.Key))
	{
		SynchronizeInteraction();
		// IE_Repeat after focus returns cannot arm the action. A new non-repeat press can,
		// even if its preceding release happened in another application.
		if (Params.Event == IE_Pressed && IsGameplayInteractionAvailable())
		{
			bFreshInteractionPress = true;
		}
	}
	return Super::InputKey(Params);
}

bool ASandsOfSorrowGameplayPlayerController::ConsumeInteractionPress()
{
	SynchronizeInteraction();
	const bool bAllowed = bFreshInteractionPress && IsGameplayInteractionAvailable();
	bFreshInteractionPress = false;
	return bAllowed;
}

void ASandsOfSorrowGameplayPlayerController::FlushPressedKeys()
{
	bFreshInteractionPress = false;
	Super::FlushPressedKeys();
}

void ASandsOfSorrowGameplayPlayerController::SetInteractionUIActive(bool bActive)
{
	if (bInteractionUIActive != bActive)
	{
		bInteractionUIActive = bActive;
		bFreshInteractionPress = false;
		SynchronizeInteraction();
	}
}

void ASandsOfSorrowGameplayPlayerController::HandleApplicationActivation(bool bActive)
{
	bApplicationActive = bActive;
	bFreshInteractionPress = false;
	SynchronizeInteraction();
}
