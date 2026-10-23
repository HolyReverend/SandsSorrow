#include "Interaction/SandsOfSorrowInteractable.h"

bool ISandsOfSorrowInteractable::CanInteract_Implementation(AActor* Interactor) const
{
	return false;
}

FText ISandsOfSorrowInteractable::GetInteractionText_Implementation(AActor* Interactor) const
{
	return FText::GetEmpty();
}

bool ISandsOfSorrowInteractable::Interact_Implementation(AActor* Interactor)
{
	return false;
}
