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

    // Bind le bouton d'achat
    if (BuyButton)
    {
        BuyButton->OnClicked.AddDynamic(this, &UShopItemWidget::OnBuyButtonClicked);
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
    }

    // Met à jour le nom
    if (ItemNameText)
    {
        FText Name = ItemData->DisplayName.IsEmpty() ? FText::FromString(TEXT("Unknown Item")) : ItemData->DisplayName;
        ItemNameText->SetText(Name);
    }

    // Met à jour le coût
    if (ItemCostText)
    {
        ItemCostText->SetText(FText::AsNumber(ItemData->Cost));
    }

    // If no icon provided, optionally add a neutral tint or hide the icon
    if (ItemIcon && !ItemData->Icon)
    {
        ItemIcon->SetVisibility(ESlateVisibility::Visible);
        ItemIcon->SetOpacity(0.5f);
    }
}

void UShopItemWidget::OnBuyButtonClicked()
{
    if (ItemData)
    {
        UE_LOG(LogTemp, Log, TEXT("ShopItemWidget: Buy button clicked for item %s (cost=%d)"), *ItemData->DisplayName.ToString(), ItemData->Cost);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("ShopItemWidget: Buy button clicked but ItemData is NULL"));
    }
    if (!ItemData || !ShopActor)
    {
        return;
    }

    // Récupère le PlayerController
    APlayerController* PC = GetOwningPlayer();
    if (!PC)
    {
        return;
    }

    // Cast vers le MOBAPlayerController pour appeler le RPC
    AMOBAPlayerController* MPC = Cast<AMOBAPlayerController>(PC);
    if (MPC)
    {
        // Appelle directement le Server RPC via le controller
        MPC->Server_RequestPurchase(ShopActor, ItemData);
    }
}
