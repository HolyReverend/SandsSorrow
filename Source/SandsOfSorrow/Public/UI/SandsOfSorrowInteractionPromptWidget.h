#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SandsOfSorrowInteractionPromptWidget.generated.h"

class UTextBlock;

/** Отображает результат компонента взаимодействия; не выбирает цель и не обрабатывает ввод. */
UCLASS(Abstract, Blueprintable)
class SANDSOFSORROW_API USandsOfSorrowInteractionPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Обновляет текст, панель действия и маркер по готовому состоянию игрового управления. */
	void SetPromptState(bool bGameplayAvailable, bool bHasTarget, const FText& Text);

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsPromptShown() const { return bPromptShown; }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	FText GetDisplayedActionText() const { return DisplayedActionText; }

protected:
	virtual void NativeConstruct() override;

	// Имена этих элементов обязательны в WBP: ссылки заполняются через BindWidget.
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
