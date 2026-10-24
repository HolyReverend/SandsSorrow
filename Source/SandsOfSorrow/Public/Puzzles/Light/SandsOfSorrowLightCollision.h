#pragma once

#include "Engine/EngineTypes.h"

class USceneComponent;
class UBoxComponent;
struct FHitResult;

namespace SandsOfSorrowLightCollision
{
	// Свободный слот проверен на шаге 2.2; имя PuzzleLight задано в DefaultEngine.ini.
	// Automation проверяет это соответствие, чтобы смена настроек не трассировала другой канал.
	inline constexpr ECollisionChannel Channel = ECC_GameTraceChannel1;
	// Общий контракт масштаба для реальных рабочих плоскостей зеркала и приёмника.
	SANDSOFSORROW_API bool HasSupportedTransform(const USceneComponent* Component);

	// Отсекает корпус, тыл, боковину, проникновение и почти касательный вход.
	SANDSOFSORROW_API bool IsFrontFaceHit(const UBoxComponent* Surface, const FHitResult& Hit, const FVector& IncomingDirection);
}
