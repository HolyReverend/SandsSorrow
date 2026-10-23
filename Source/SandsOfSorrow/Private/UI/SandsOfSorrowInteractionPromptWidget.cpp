#include "UI/SandsOfSorrowInteractionPromptWidget.h"

#include "Components/TextBlock.h"

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
