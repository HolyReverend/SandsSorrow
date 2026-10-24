#include "Interaction/SandsOfSorrowInteractionComponent.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/SandsOfSorrowInteractable.h"
#include "Kismet/GameplayStatics.h"
#include "SandsOfSorrow.h"

// Включает автоматическую активацию и обновление фокуса после основного обновления мира.
USandsOfSorrowInteractionComponent::USandsOfSorrowInteractionComponent()
{
	bAutoActivate = true;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bTickEvenWhenPaused = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

// Проверяет активность, локальное владение Pawn и доступность управления.
bool USandsOfSorrowInteractionComponent::GetInteractionContext(APawn*& Pawn, APlayerController*& Controller) const
{
	Pawn = Cast<APawn>(GetOwner());
	Controller = IsValid(Pawn) ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	return bInteractionInputEnabled && IsActive() && IsValid(Pawn) && IsValid(Controller) && Pawn->IsLocallyControlled()
		&& Controller->GetPawn() == Pawn && !Controller->IsMoveInputIgnored()
		&& GetWorld() && !UGameplayStatics::IsGamePaused(this);
}

// Выбирает одну цель первым попаданием камеры, затем проверяет досягаемость персонажа.
AActor* USandsOfSorrowInteractionComponent::FindTarget(FText& OutText) const
{
	OutText = FText::GetEmpty();
	APawn* Pawn = nullptr;
	APlayerController* Controller = nullptr;
	// Метаданные редактора не ограничивают значения, присвоенные из C++.
	if (!FMath::IsFinite(CameraTraceDistance) || CameraTraceDistance < 1.0f
		|| !FMath::IsFinite(InteractionDistance) || InteractionDistance < 1.0f
		|| !GetInteractionContext(Pawn, Controller))
	{
		return nullptr;
	}

	// Берём первое блокирующее попадание: объекты за стеной не становятся кандидатами.
	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SandsInteraction), false, Pawn);
	FHitResult CameraHit;
	if (!GetWorld()->LineTraceSingleByChannel(CameraHit, ViewLocation,
		ViewLocation + ViewRotation.Vector() * CameraTraceDistance, ECC_Visibility, Params))
	{
		return nullptr;
	}

	AActor* Target = CameraHit.GetActor();
	if (!IsValid(Target) || !Target->Implements<USandsOfSorrowInteractable>()
		|| FVector::DistSquared(Pawn->GetActorLocation(), CameraHit.ImpactPoint) > FMath::Square(static_cast<double>(InteractionDistance)))
	{
		return nullptr;
	}

	// Видимость от тела запрещает взаимодействие из-за угла, даже если камера видит цель.
	FVector BodyOrigin = Pawn->GetPawnViewLocation();
	if (const ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
		BodyOrigin = Capsule->GetComponentLocation() + Capsule->GetUpVector()
			* (Capsule->GetScaledCapsuleHalfHeight() * 0.75f);
	}
	FHitResult BodyHit;
	if (GetWorld()->LineTraceSingleByChannel(BodyHit, BodyOrigin, CameraHit.ImpactPoint, ECC_Visibility, Params)
		&& BodyHit.GetActor() != Target)
	{
		return nullptr;
	}

	// Blueprint-запросы не должны менять мир, но после них всё равно проверяем жизнь объектов.
	if (!ISandsOfSorrowInteractable::Execute_CanInteract(Target, Pawn) || !IsValid(Target) || !IsValid(Pawn))
	{
		return nullptr;
	}
	OutText = ISandsOfSorrowInteractable::Execute_GetInteractionText(Target, Pawn);
	return IsValid(Target) && IsValid(Pawn) ? Target : nullptr;
}

// Обновляет цель и текст вместе; уведомляет подписчиков только при изменении.
void USandsOfSorrowInteractionComponent::SetFocus(AActor* Target, const FText& Text)
{
	if (!IsValid(Target))
	{
		Target = nullptr;
	}
	const FText NewText = Target ? Text : FText::GetEmpty();
	// Сравниваем саму слабую ссылку: уничтоженная цель тоже требует уведомления об очистке.
	if (FocusedActor == TWeakObjectPtr<AActor>(Target) && FocusedText.EqualTo(NewText))
	{
		return;
	}
	FocusedActor = Target;
	FocusedText = NewText;
	UE_LOG(LogSandsOfSorrow, Verbose, TEXT("Interaction focus: %s (%s)"), *GetNameSafe(Target), *NewText.ToString());
	OnFocusChanged.Broadcast(Target, NewText);
}

// При блокировке сразу очищает фокус, при возобновлении заново проверяет мир.
void USandsOfSorrowInteractionComponent::SetInteractionInputEnabled(bool bEnabled)
{
	const bool bChanged = bInteractionInputEnabled != bEnabled;
	bInteractionInputEnabled = bEnabled;
	if (!bEnabled)
	{
		ClearFocus();
	}
	else if (bChanged)
	{
		RefreshFocus();
	}
}

// Отключение компонента немедленно убирает ранее выбранную цель.
void USandsOfSorrowInteractionComponent::Deactivate()
{
	Super::Deactivate();
	ClearFocus();
}

// Пересчитывает цель и подпись по текущему состоянию мира.
void USandsOfSorrowInteractionComponent::RefreshFocus()
{
	FText Text;
	AActor* Target = FindTarget(Text);
	SetFocus(Target, Text);
}

// Сбрасывает цель и текст через общий механизм уведомления.
void USandsOfSorrowInteractionComponent::ClearFocus()
{
	SetFocus(nullptr, FText::GetEmpty());
}

// Повторно проверяет показанную цель перед действием; новое попадание требует следующего нажатия.
bool USandsOfSorrowInteractionComponent::RequestInteraction()
{
	// Уведомление фокуса может повторно вызвать запрос; вложенное действие отклоняем.
	if (bRequestInProgress)
	{
		return false;
	}
	TGuardValue<bool> Guard(bRequestInProgress, true);
	const TWeakObjectPtr<AActor> PreviousTarget = FocusedActor;
	FText Text;
	AActor* Target = FindTarget(Text);
	SetFocus(Target, Text);
	APawn* Pawn = nullptr;
	APlayerController* Controller = nullptr;
	if (!IsValid(Target) || !PreviousTarget.IsValid() || PreviousTarget.Get() != Target
		|| !GetInteractionContext(Pawn, Controller) || FocusedActor.Get() != Target)
	{
		UE_LOG(LogSandsOfSorrow, Verbose, TEXT("Interaction rejected: unavailable or changed target."));
		return false;
	}

	// Подписчики фокуса могут выполнить Blueprint-код: после уведомления проверяем мир ещё раз.
	FText RecheckedText;
	if (FindTarget(RecheckedText) != Target || !IsValid(Target) || !IsValid(Pawn))
	{
		ClearFocus();
		return false;
	}
	const bool bSuccess = ISandsOfSorrowInteractable::Execute_Interact(Target, Pawn);
	UE_LOG(LogSandsOfSorrow, Display, TEXT("Interaction %s: %s"), bSuccess ? TEXT("succeeded") : TEXT("rejected"),
		*GetNameSafe(IsValid(Target) ? Target : nullptr));
	RefreshFocus();
	return bSuccess;
}

// Регулярно обновляет фокус при движении игрока, камеры и устройств.
void USandsOfSorrowInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RefreshFocus();
}

// Очищает фокус с уведомлением и освобождает подписчиков при завершении жизни компонента.
void USandsOfSorrowInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearFocus();
	OnFocusChanged.Clear();
	Super::EndPlay(EndPlayReason);
}
