#include "Interaction/SandsOfSorrowInteractionComponent.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/SandsOfSorrowInteractable.h"
#include "Kismet/GameplayStatics.h"
#include "SandsOfSorrow.h"

USandsOfSorrowInteractionComponent::USandsOfSorrowInteractionComponent()
{
	bAutoActivate = true;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bTickEvenWhenPaused = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

bool USandsOfSorrowInteractionComponent::GetInteractionContext(APawn*& Pawn, APlayerController*& Controller) const
{
	Pawn = Cast<APawn>(GetOwner());
	Controller = IsValid(Pawn) ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	return bInteractionInputEnabled && IsActive() && IsValid(Pawn) && IsValid(Controller) && Pawn->IsLocallyControlled()
		&& Controller->GetPawn() == Pawn && !Controller->IsMoveInputIgnored()
		&& GetWorld() && !UGameplayStatics::IsGamePaused(this);
}

AActor* USandsOfSorrowInteractionComponent::FindTarget(FText& OutText) const
{
	OutText = FText::GetEmpty();
	APawn* Pawn = nullptr;
	APlayerController* Controller = nullptr;
	if (!GetInteractionContext(Pawn, Controller))
	{
		return nullptr;
	}

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
		|| FVector::DistSquared(Pawn->GetActorLocation(), CameraHit.ImpactPoint) > FMath::Square(InteractionDistance))
	{
		return nullptr;
	}

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

	if (!ISandsOfSorrowInteractable::Execute_CanInteract(Target, Pawn) || !IsValid(Target) || !IsValid(Pawn))
	{
		return nullptr;
	}
	OutText = ISandsOfSorrowInteractable::Execute_GetInteractionText(Target, Pawn);
	return IsValid(Target) && IsValid(Pawn) ? Target : nullptr;
}

void USandsOfSorrowInteractionComponent::SetFocus(AActor* Target, const FText& Text)
{
	if (!IsValid(Target))
	{
		Target = nullptr;
	}
	const FText NewText = Target ? Text : FText::GetEmpty();
	// Compare the weak handle too: a destroyed target still needs a clearing notification.
	if (FocusedActor == TWeakObjectPtr<AActor>(Target) && FocusedText.EqualTo(NewText))
	{
		return;
	}
	FocusedActor = Target;
	FocusedText = NewText;
	UE_LOG(LogSandsOfSorrow, Verbose, TEXT("Interaction focus: %s (%s)"), *GetNameSafe(Target), *NewText.ToString());
	OnFocusChanged.Broadcast(Target, NewText);
}

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

void USandsOfSorrowInteractionComponent::Deactivate()
{
	Super::Deactivate();
	ClearFocus();
}

void USandsOfSorrowInteractionComponent::RefreshFocus()
{
	FText Text;
	AActor* Target = FindTarget(Text);
	SetFocus(Target, Text);
}

void USandsOfSorrowInteractionComponent::ClearFocus()
{
	SetFocus(nullptr, FText::GetEmpty());
}

bool USandsOfSorrowInteractionComponent::RequestInteraction()
{
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

	// Focus notifications can run Blueprint code; recheck geometry and availability after broadcasting.
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

void USandsOfSorrowInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RefreshFocus();
}

void USandsOfSorrowInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearFocus();
	OnFocusChanged.Clear();
	Super::EndPlay(EndPlayReason);
}
