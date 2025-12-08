// Implementation du widget d'item de shop
#include "ShopItemWidget.h"
#include "ItemData.h"
#include "ShopActor.h"
#include "PlayerHUDWidget.h"
#include "MOBAPlayerController.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"

void UShopItemWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Bind le bouton d'achat (use AddUniqueDynamic to avoid double-binding)
    if (BuyButton)
    {
        BuyButton->OnClicked.AddUniqueDynamic(this, &UShopItemWidget::OnBuyButtonClicked);
    }

    // Refresh l'affichage si déjà initialisé
    if (ItemData)
    {
        RefreshDisplay();
    }
}

void UShopItemWidget::InitializeItem(UItemData* InItemData, AShopActor* InShopActor)
{
    ItemData = InItemData;
    ShopActor = InShopActor;

    RefreshDisplay();
}

void UShopItemWidget::RefreshDisplay()
{
    if (!ItemData)
    {
        return;
    }

    // Met à jour l'icône
        if (ItemIcon && ItemData->Icon)
    {
        FSlateBrush Brush;
        Brush.SetResourceObject(ItemData->Icon);
        Brush.ImageSize = FVector2D(64.0f, 64.0f);
            ItemIcon->SetBrush(Brush);
            ItemIcon->SetVisibility(ESlateVisibility::Visible);
    }

    // Met à jour le nom
        if (ItemNameText)
    {
        FText Name = ItemData->DisplayName.IsEmpty() ? FText::FromName(*ItemData->GetFName().ToString()) : ItemData->DisplayName;
        if (ItemData->DisplayName.IsEmpty())
        {
            // fallback to asset name
            Name = FText::FromString(ItemData->GetName());
        }
            ItemNameText->SetText(Name);
            ItemNameText->SetVisibility(ESlateVisibility::Visible);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("ShopItemWidget: ItemNameText is null for item %s (WidgetClass=%s)"), *ItemData->GetName(), *GetClass()->GetName());
    }

    // Met à jour le coût
        if (ItemCostText)
    {
        ItemCostText->SetText(FText::AsNumber(ItemData->Cost));
            ItemCostText->SetVisibility(ESlateVisibility::Visible);
    }

    // If no icon provided, optionally add a neutral tint or hide the icon
        if (ItemIcon && !ItemData->Icon)
    {
        ItemIcon->SetVisibility(ESlateVisibility::Visible);
        ItemIcon->SetOpacity(0.5f);
    }
    else if (!ItemIcon)
    {
        UE_LOG(LogTemp, Warning, TEXT("ShopItemWidget: ItemIcon text is null for item %s (WidgetClass=%s)"), *ItemData->GetName(), *GetClass()->GetName());
    }
        // Ensure buy button is visible (designer may hide it in blueprint)
        if (BuyButton)
        {
            BuyButton->SetVisibility(ESlateVisibility::Visible);
        }
}

bool UShopItemWidget::HasBoundNameText() const
{
    return ItemNameText != nullptr;
}

void UShopItemWidget::OnBuyButtonClicked()
{
    UE_LOG(LogTemp, Warning, TEXT("SHOP: BuyButton clicked! ItemData=%s ShopActor=%s"), 
        ItemData ? *ItemData->GetName() : TEXT("NULL"),
        ShopActor ? *ShopActor->GetName() : TEXT("NULL"));

    if (!ItemData || !ShopActor)
    {
        UE_LOG(LogTemp, Warning, TEXT("SHOP: Purchase aborted - ItemData or ShopActor is NULL"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("SHOP: Buying item '%s' (cost=%d)"), *ItemData->DisplayName.ToString(), ItemData->Cost);

    // Récupère le PlayerController
    APlayerController* PC = GetOwningPlayer();
    if (!PC)
    {
        UE_LOG(LogTemp, Warning, TEXT("SHOP: No owning player controller!"));
        return;
    }

    // Cast vers le MOBAPlayerController pour appeler le RPC
    AMOBAPlayerController* MPC = Cast<AMOBAPlayerController>(PC);
    if (MPC)
    {
        UE_LOG(LogTemp, Warning, TEXT("SHOP: Calling Server_RequestPurchase on controller %s"), *MPC->GetName());
        // Appelle directement le Server RPC via le controller
        MPC->Server_RequestPurchase(ShopActor, ItemData);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("SHOP: PlayerController is not a MOBAPlayerController!"));
    }
}
