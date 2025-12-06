// Small debug widget to ensure Shop UI is visible
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DebugShopOverlayWidget.generated.h"

class UTextBlock;

UCLASS()
class CODING_API UDebugShopOverlayWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void SetItemCount(int32 Count);

protected:
    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* DebugText;

    virtual void NativeConstruct() override;
};
