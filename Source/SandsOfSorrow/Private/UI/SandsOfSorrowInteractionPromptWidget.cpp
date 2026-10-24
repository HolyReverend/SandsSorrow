#include "UI/SandsOfSorrowInteractionPromptWidget.h"

#include "Components/TextBlock.h"

// Инициализирует пустую скрытую подсказку, включая повторное добавление виджета на экран.
void USandsOfSorrowInteractionPromptWidget::NativeConstruct()
{
	Super::NativeConstruct();
	DisplayedActionText = FText::GetEmpty();
	if (ActionText)
	{
		ActionText->SetText(DisplayedActionText);
	}
	SetPromptState(false, false, FText::GetEmpty());
}

// Показывает текст только для доступной цели; маркер зависит от доступности управления.
void USandsOfSorrowInteractionPromptWidget::SetPromptState(bool bGameplayAvailable, bool bHasTarget, const FText& Text)
{
	bPromptShown = bGameplayAvailable && bHasTarget && !Text.IsEmpty();
	const FText NewText = bPromptShown ? Text : FText::GetEmpty();
	if (!DisplayedActionText.EqualTo(NewText))
	{
		DisplayedActionText = NewText;
		if (ActionText)
		{
			ActionText->SetText(NewText);
		}
	}
	// Вся подсказка пропускает мышь: UI не должен отбирать фокус у игрового ввода.
	SetVisibility(bGameplayAvailable ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (AimMarker)
	{
		AimMarker->SetVisibility(bGameplayAvailable ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (PromptPanel)
	{
		PromptPanel->SetVisibility(bPromptShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
