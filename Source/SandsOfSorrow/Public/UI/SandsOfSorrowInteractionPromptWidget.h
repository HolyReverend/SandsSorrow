#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SandsOfSorrowInteractionPromptWidget.generated.h"

class UTextBlock;

/** Displays the interaction component's result. Contains no target queries or input handlers. */
UCLASS(Abstract, Blueprintable)
class SANDSOFSORROW_API USandsOfSorrowInteractionPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetPromptState(bool bGameplayAvailable, bool bHasTarget, const FText& Text);

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsPromptShown() const { return bPromptShown; }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	FText GetDisplayedActionText() const { return DisplayedActionText; }

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> PromptPanel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ActionText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> AimMarker;

private:
	FText DisplayedActionText;
	bool bPromptShown = false;
};
