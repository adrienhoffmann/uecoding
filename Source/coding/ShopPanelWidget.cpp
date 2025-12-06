// Implementation du panneau de shop
#include "ShopPanelWidget.h"
#include "ShopItemWidget.h"
#include "ShopActor.h"
#include "ShopComponent.h"
#include "ItemData.h"
#include "Components/ScrollBox.h"
#include "Components/WrapBox.h"
#include "Components/Button.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"

void UShopPanelWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Bind le bouton de fermeture
    if (CloseButton)
    {
        // Avoid double-binding if NativeConstruct is called multiple times
        CloseButton->OnClicked.RemoveDynamic(this, &UShopPanelWidget::OnCloseButtonClicked);
        CloseButton->OnClicked.AddUniqueDynamic(this, &UShopPanelWidget::OnCloseButtonClicked);
    }

    // Masque le widget au départ
    SetVisibility(ESlateVisibility::Collapsed);
    UE_LOG(LogTemp, Log, TEXT("ShopPanelWidget: NativeConstruct initialized"));
}

void UShopPanelWidget::OpenShop(AShopActor* Shop)
{
    if (!Shop)
    {
        return;
    }

    CurrentShop = Shop;
    bIsOpen = true;

    // Affiche le widget
    SetVisibility(ESlateVisibility::Visible);
    SetIsEnabled(true);
    // Try to bring to front if it's in a canvas slot
    if (UCanvasPanelSlot* CanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(this))
    {
        CanvasSlot->SetZOrder(999);
        UE_LOG(LogTemp, Log, TEXT("ShopPanelWidget: Brought canvas slot to front with ZOrder=999"));
    }
    else
    {
        // Not in a canvas slot - try to force visibility by removing and re-adding to viewport
        UE_LOG(LogTemp, Log, TEXT("ShopPanelWidget: Not in CanvasSlot; forcing AddToViewport"));
        RemoveFromParent();
        AddToViewport(9999);
    }
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, TEXT("ShopPanelWidget: OpenShop called"));
    }

    // Peuple la liste des items
    RefreshItems();
    UE_LOG(LogTemp, Log, TEXT("ShopPanelWidget: OpenShop called for %s"), Shop ? *Shop->GetName() : TEXT("NULL"));
}

void UShopPanelWidget::CloseShop()
{
    bIsOpen = false;
    CurrentShop = nullptr;

    // Masque le widget
    SetVisibility(ESlateVisibility::Collapsed);

    // Efface les items
    ClearItems();
    UE_LOG(LogTemp, Log, TEXT("ShopPanelWidget: CloseShop called"));
}

void UShopPanelWidget::RefreshItems()
{
    ClearItems();

    if (!CurrentShop || !CurrentShop->ShopComponent)
    {
        UE_LOG(LogTemp, Log, TEXT("ShopPanelWidget: RefreshItems aborted - no current shop or component"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("ShopPanelWidget: RefreshItems - %d available items. WrapBox=%s ScrollBox=%s"),
        CurrentShop->ShopComponent->AvailableItems.Num(),
        ItemsWrapBox ? TEXT("YES") : TEXT("NO"),
        ItemsScrollBox ? TEXT("YES") : TEXT("NO"));

    // Ajoute un widget pour chaque item disponible
    for (UItemData* Item : CurrentShop->ShopComponent->AvailableItems)
    {
        if (Item)
        {
            AddItemWidget(Item);
        }
    }

    // Force layout update
    ForceLayoutPrepass();
}

void UShopPanelWidget::ClearItems()
{
    if (ItemsWrapBox)
    {
        ItemsWrapBox->ClearChildren();
    }
    else if (ItemsScrollBox)
    {
        ItemsScrollBox->ClearChildren();
    }
}

void UShopPanelWidget::AddItemWidget(UItemData* ItemData)
{
    if (!ShopItemWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("ShopPanelWidget: ShopItemWidgetClass n'est pas configuré!"));
        return;
    }

    // Crée le widget d'item
    UShopItemWidget* ItemWidget = CreateWidget<UShopItemWidget>(GetOwningPlayer(), ShopItemWidgetClass);
    if (!ItemWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("ShopPanelWidget: Failed to create ShopItemWidget"));
        return;
    }

    // Initialise avec les données
    ItemWidget->InitializeItem(ItemData, CurrentShop);

    // Ajoute au container approprié
    if (ItemsWrapBox)
    {
        ItemsWrapBox->AddChild(ItemWidget);
        UE_LOG(LogTemp, Log, TEXT("ShopPanelWidget: Added item widget to WrapBox for %s (Ptr=%p)"), *ItemData->DisplayName.ToString(), ItemData);
    }
    else if (ItemsScrollBox)
    {
        ItemsScrollBox->AddChild(ItemWidget);
        UE_LOG(LogTemp, Log, TEXT("ShopPanelWidget: Added item widget to ScrollBox for %s (Ptr=%p)"), *ItemData->DisplayName.ToString(), ItemData);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("ShopPanelWidget: No container (WrapBox or ScrollBox) to add item widget!"));
    }
}

void UShopPanelWidget::OnCloseButtonClicked()
{
    CloseShop();
}
