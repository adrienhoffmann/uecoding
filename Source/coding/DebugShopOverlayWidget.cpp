#include "DebugShopOverlayWidget.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"

void UDebugShopOverlayWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (DebugText)
    {
        DebugText->SetText(FText::FromString(TEXT("SHOP OPEN")));
    }
}

void UDebugShopOverlayWidget::SetItemCount(int32 Count)
{
    if (DebugText)
    {
        FString S = FString::Printf(TEXT("SHOP OPEN - Items: %d"), Count);
        DebugText->SetText(FText::FromString(S));
    }
}
