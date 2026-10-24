#include "Interaction/SandsOfSorrowInteractable.h"

// По умолчанию устройство недоступно, пока реализация явно не разрешит взаимодействие.
bool ISandsOfSorrowInteractable::CanInteract_Implementation(AActor* Interactor) const
{
	return false;
}

// Базовый контракт не предлагает подпись действия.
FText ISandsOfSorrowInteractable::GetInteractionText_Implementation(AActor* Interactor) const
{
	return FText::GetEmpty();
}

// Без реализации запрос безопасно отклоняется и не меняет мир.
bool ISandsOfSorrowInteractable::Interact_Implementation(AActor* Interactor)
{
	return false;
}
