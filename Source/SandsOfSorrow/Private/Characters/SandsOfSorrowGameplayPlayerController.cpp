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

// Подключает контекст локального игрока; повторная настройка не создаёт дубль.
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

	// Чужой контекст не присваиваем себе, чтобы позднее не удалить его при очистке.
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

// Освобождает подписки, подсказку и контекст, которыми владеет контроллер.
void ASandsOfSorrowGameplayPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelCrouchHold();
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

// Удаляет только собственный контекст из той подсистемы, в которую он был добавлен.
void ASandsOfSorrowGameplayPlayerController::RemoveOwnedMappingContext()
{
	CancelCrouchHold();
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

// Продолжает обновлять доступность взаимодействия и UI во время паузы.
ASandsOfSorrowGameplayPlayerController::ASandsOfSorrowGameplayPlayerController()
{
	PrimaryActorTick.bTickEvenWhenPaused = true;
	bShouldPerformFullTickWhenPaused = true;
}

// Подписывается на смену активности приложения и связывает подсказку с текущим Pawn.
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

// Создаёт единственный виджет локального игрока после начала игры.
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

// Разрешает взаимодействие только при доступном игровом вводе и фокусе viewport.
bool ASandsOfSorrowGameplayPlayerController::IsGameplayControlAvailable() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UGameViewportClient* Viewport = LocalPlayer ? LocalPlayer->ViewportClient : nullptr;
	if (!IsLocalController() || !IsValid(GetPawn())
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

// Подсказка ограничивает только взаимодействие, но не обычное управление персонажем.
bool ASandsOfSorrowGameplayPlayerController::IsGameplayInteractionAvailable() const
{
	return IsGameplayControlAvailable() && InteractionPrompt && InteractionPrompt->IsInViewport();
}

// Отписывается от прежнего Pawn и сразу сбрасывает его фокус, подсказку и разрешение нажатия.
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

// Согласует подписку на текущий Pawn, доступность компонента и состояние UI.
void ASandsOfSorrowGameplayPlayerController::SynchronizeInteraction()
{
	if (!IsLocalController())
	{
		return;
	}
	if (!IsGameplayControlAvailable())
	{
		CancelCrouchHold();
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
	// Блокировка отменяет старое нажатие; её снятие само по себе новое не создаёт.
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

// Передаёт виджету готовый фокус компонента; повторного выбора цели в UI нет.
void ASandsOfSorrowGameplayPlayerController::UpdatePrompt()
{
	if (InteractionPrompt)
	{
		USandsOfSorrowInteractionComponent* Current = ObservedInteraction.Get();
		InteractionPrompt->SetPromptState(IsGameplayInteractionAvailable() && IsValid(Current),
			Current && IsValid(Current->GetFocusedActor()), Current ? Current->GetFocusedText() : FText::GetEmpty());
	}
}

// Обновляет подсказку из актуального состояния компонента после его уведомления.
void ASandsOfSorrowGameplayPlayerController::HandleFocusChanged(AActor* Target, FText InteractionText)
{
	UpdatePrompt();
}

// Разрывает связь со старым компонентом до передачи управления новому Pawn.
void ASandsOfSorrowGameplayPlayerController::SetPawn(APawn* InPawn)
{
	if (GetPawn() != InPawn)
	{
		CancelCrouchHold();
		DetachInteraction();
	}
	Super::SetPawn(InPawn);
	SynchronizeInteraction();
}

// Отслеживает изменения доступности, даже если событий ввода в кадре нет.
void ASandsOfSorrowGameplayPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	SynchronizeInteraction();
}

// Применяет ограничения до обработки действий Enhanced Input.
void ASandsOfSorrowGameplayPlayerController::PreProcessInput(float DeltaTime, bool bGamePaused)
{
	SynchronizeInteraction();
	Super::PreProcessInput(DeltaTime, bGamePaused);
}

// Завершает обработку нажатия и синхронизирует состояние после действий.
void ASandsOfSorrowGameplayPlayerController::PostProcessInput(float DeltaTime, bool bGamePaused)
{
	Super::PostProcessInput(DeltaTime, bGamePaused);
	// Разрешение не переносится в другой кадр, режим ввода или перестроенный IMC.
	bFreshInteractionPress = false;
	bFreshCrouchPress = false;
	SynchronizeInteraction();
}

// Определяет клавишу по действующим привязкам действия текущего персонажа.
bool ASandsOfSorrowGameplayPlayerController::IsInteractionKey(const FKey& Key) const
{
	const ASandsOfSorrowPlayerCharacter* GameplayCharacter = Cast<ASandsOfSorrowPlayerCharacter>(GetPawn());
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	return GameplayCharacter && GameplayCharacter->GetInteractAction() && Subsystem
		&& Subsystem->QueryKeysMappedToAction(GameplayCharacter->GetInteractAction()).Contains(Key);
}

// Даёт одноразовое разрешение на действие только по свежему физическому нажатию.
bool ASandsOfSorrowGameplayPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	if (IsCrouchKey(Params.Key) && Params.Event == IE_Pressed && IsGameplayControlAvailable())
	{
		bFreshCrouchPress = true;
	}
	if (IsInteractionKey(Params.Key))
	{
		SynchronizeInteraction();
		// Автоповтор после возврата фокуса не разрешает действие. Новое нажатие допустимо,
		// даже если предыдущее отпускание произошло в другом приложении.
		if (Params.Event == IE_Pressed && IsGameplayInteractionAvailable())
		{
			bFreshInteractionPress = true;
		}
	}
	return Super::InputKey(Params);
}

// Потребляет разрешение ровно один раз из обработчика Started персонажа.
bool ASandsOfSorrowGameplayPlayerController::ConsumeInteractionPress()
{
	SynchronizeInteraction();
	const bool bAllowed = bFreshInteractionPress && IsGameplayInteractionAvailable();
	bFreshInteractionPress = false;
	return bAllowed;
}

// Сброс клавиш отменяет также ещё не использованное разрешение взаимодействия.
void ASandsOfSorrowGameplayPlayerController::FlushPressedKeys()
{
	bFreshInteractionPress = false;
	CancelCrouchHold();
	Super::FlushPressedKeys();
}

// Владелец меню сообщает о блокировке; режим ввода он переключает самостоятельно.
void ASandsOfSorrowGameplayPlayerController::SetInteractionUIActive(bool bActive)
{
	if (bInteractionUIActive != bActive)
	{
		bInteractionUIActive = bActive;
		bFreshInteractionPress = false;
		SynchronizeInteraction();
	}
}

// При смене активности отменяет нажатие и немедленно пересчитывает доступность.
void ASandsOfSorrowGameplayPlayerController::HandleApplicationActivation(bool bActive)
{
	bApplicationActive = bActive;
	bFreshInteractionPress = false;
	SynchronizeInteraction();
}

// Привязка читается из действующего IMC, а не дублируется жёсткой клавишей в C++.
bool ASandsOfSorrowGameplayPlayerController::IsCrouchKey(const FKey& Key) const
{
	const ASandsOfSorrowPlayerCharacter* GameplayCharacter = Cast<ASandsOfSorrowPlayerCharacter>(GetPawn());
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	return GameplayCharacter && GameplayCharacter->GetCrouchAction() && Subsystem
		&& Subsystem->QueryKeysMappedToAction(GameplayCharacter->GetCrouchAction()).Contains(Key);
}

// Возврат фокуса с удержанной клавишей не создаёт новое разрешение Started.
bool ASandsOfSorrowGameplayPlayerController::ConsumeCrouchPress()
{
	const bool bAllowed = bFreshCrouchPress && IsGameplayControlAvailable();
	bFreshCrouchPress = false;
	return bAllowed;
}

// Сброс намерения оставляет безопасное вставание штатной проверке коллизии.
void ASandsOfSorrowGameplayPlayerController::CancelCrouchHold()
{
	bFreshCrouchPress = false;
	if (ASandsOfSorrowPlayerCharacter* GameplayCharacter = Cast<ASandsOfSorrowPlayerCharacter>(GetPawn()))
	{
		GameplayCharacter->SetCrouchHeld(false);
	}
}
