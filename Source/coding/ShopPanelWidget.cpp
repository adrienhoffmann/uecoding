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
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
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

    // Ne pas masquer ici - cela interfère avec OpenShop qui appelle AddToViewport
    // La visibilité sera gérée par OpenShop/CloseShop
    // Si bIsOpen est false, on cache le widget (pour le cas où il est créé sans être ouvert)
    if (!bIsOpen)
    {
        SetVisibility(ESlateVisibility::Collapsed);
    }
    UE_LOG(LogTemp, Log, TEXT("ShopPanelWidget: NativeConstruct initialized"));

    // Debug: report whether ShopItemWidgetClass is set on this instance
    if (ShopItemWidgetClass)
    {
        UE_LOG(LogTemp, Log, TEXT("ShopPanelWidget: ShopItemWidgetClass = %s"), *ShopItemWidgetClass->GetName());
        // Warn if ShopItemWidgetClass is the raw C++ class and not a Blueprint derived class
        if (ShopItemWidgetClass == UShopItemWidget::StaticClass())
        {
            UE_LOG(LogTemp, Error, TEXT("ShopPanelWidget: ShopItemWidgetClass is set to the raw C++ class UShopItemWidget. You must set it to a Blueprint Widget (e.g. WBP_Shop) that inherits from ShopItemWidget!"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("ShopPanelWidget: ShopItemWidgetClass is not set on widget instance"));
    }
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
        // Always attempt to center by using alignment and desired size
        if (GEngine && GEngine->GameViewport)
        {
            FVector2D ViewportSize;
            GEngine->GameViewport->GetViewportSize(ViewportSize);
            // Use a default size that hopefully fits across common viewports
            FVector2D DefaultSize(800.f, 500.f);
            SetDesiredSizeInViewport(DefaultSize);
            // Center the widget by setting alignment to middle and position to 0,0 in viewport
            SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
            SetPositionInViewport(FVector2D::ZeroVector, false);
            UE_LOG(LogTemp, Log, TEXT("ShopPanelWidget: Forced AddToViewport & centered with size %s (viewport %s)"), *DefaultSize.ToString(), *ViewportSize.ToString());
        }
        else
        {
            // Fallback: set a desired size in case we can't read viewport
            SetDesiredSizeInViewport(FVector2D(800.f, 500.f));
            SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
            SetPositionInViewport(FVector2D::ZeroVector, false);
            UE_LOG(LogTemp, Log, TEXT("ShopPanelWidget: Forced AddToViewport (no viewport access) & centered with default size"));
        }
    }
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, TEXT("ShopPanelWidget: OpenShop called"));
    }

    // Set focus and ensure UI is focusable so state changes and keyboard/mouse focus are correct
    SetIsEnabled(true);
    SetIsFocusable(true);
    if (APlayerController* PC = GetOwningPlayer())
    {
        FInputModeGameAndUI Mode;
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        Mode.SetHideCursorDuringCapture(false);
        PC->SetInputMode(Mode);
        PC->bShowMouseCursor = true;
        UE_LOG(LogTemp, Log, TEXT("ShopPanelWidget: Set input mode to GameAndUI and showed mouse cursor"));
    }

    // Show explicit on-screen debug message with item count to help visual verification
    if (GEngine && Shop && Shop->ShopComponent)
    {
        FString ItemMsg = FString::Printf(TEXT("SHOP OPEN: %d items"), Shop->ShopComponent->AvailableItems.Num());
        GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor::Green, ItemMsg);
    }

    // Peuple la liste des items
    RefreshItems();
    UE_LOG(LogTemp, Log, TEXT("ShopPanelWidget: OpenShop called for %s"), Shop ? *Shop->GetName() : TEXT("NULL"));

    // Debug: Log the actual rendered size of this widget
    FVector2D DesiredSize = GetDesiredSize();
    UE_LOG(LogTemp, Warning, TEXT("ShopPanelWidget: Widget DesiredSize = %s, Visibility = %d"), *DesiredSize.ToString(), (int32)GetVisibility());

    // Force on-screen debug to confirm the shop panel visibility state
    if (GEngine)
    {
        FString DebugMsg = FString::Printf(TEXT("SHOP PANEL: DesiredSize=(%.0f,%.0f) Visibility=%d IsInViewport=%d"),
            DesiredSize.X, DesiredSize.Y, (int32)GetVisibility(), IsInViewport() ? 1 : 0);
        GEngine->AddOnScreenDebugMessage(-1, 8.0f, FColor::Cyan, DebugMsg);
    }
    
    // Try to apply a temporary background tint at runtime if no visible background exists to aid debugging
    if (WidgetTree)
    {
        UWidget* Root = GetRootWidget();
        if (Root)
        {
            // If there's no border background, try to wrap content into a border at runtime
            UBorder* AsBorder = Cast<UBorder>(Root);
            if (!AsBorder)
            {
                UE_LOG(LogTemp, Log, TEXT("ShopPanelWidget: Root is not a Border; ensuring we add visual debug tint is left to designer"));
            }
            else
            {
                AsBorder->SetBrushColor(FLinearColor(0.0f, 0.2f, 0.6f, 0.6f));
                UE_LOG(LogTemp, Log, TEXT("ShopPanelWidget: Applied debug tint to root Border"));
            }
        }
    }

    // Also add a debug header text to the scroll/Wrap, this proves the widget is rendered even if no background exists.
    if (ItemsScrollBox && WidgetTree)
    {
        UTextBlock* Header = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        if (Header)
        {
            Header->SetText(FText::FromString(TEXT("DEBUG SHOP PANEL HEADER")));
            Header->SetColorAndOpacity(FSlateColor(FLinearColor::Yellow));
            ItemsScrollBox->InsertChildAt(0, Header);
            UE_LOG(LogTemp, Log, TEXT("ShopPanelWidget: Inserted debug header into ItemsScrollBox"));
        }
    }

    // CRITICAL: Force visibility to Visible at the very end of OpenShop
    // This ensures NativeConstruct (which may have been called during AddToViewport) doesn't leave us Collapsed
    SetVisibility(ESlateVisibility::Visible);
    UE_LOG(LogTemp, Log, TEXT("ShopPanelWidget: OpenShop FINAL - forced Visibility to Visible (current=%d)"), (int32)GetVisibility());
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

    // Log if the container is empty after adding
    int32 ChildCount = 0;
    if (ItemsWrapBox) ChildCount = ItemsWrapBox->GetChildrenCount();
    else if (ItemsScrollBox) ChildCount = ItemsScrollBox->GetChildrenCount();
    UE_LOG(LogTemp, Log, TEXT("ShopPanelWidget: Post RefreshItems - child count: %d"), ChildCount);
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

    // Log class info for debugging (ensure the blueprint actually derives from UShopItemWidget)
    if (ShopItemWidgetClass)
    {
        UE_LOG(LogTemp, Log, TEXT("ShopPanelWidget: ShopItemWidgetClass configured to '%s'"), *ShopItemWidgetClass->GetName());
        // Warn if using raw C++ class
        if (ShopItemWidgetClass == UShopItemWidget::StaticClass())
        {
            UE_LOG(LogTemp, Error, TEXT("ShopPanelWidget: ShopItemWidgetClass is the raw C++ class! Set it to your Blueprint (e.g. WBP_Shop) instead."));
        }
    }
    if (ItemWidget)
    {
        UE_LOG(LogTemp, Log, TEXT("ShopPanelWidget: Created ItemWidget instance class='%s' ptr=%p"), *ItemWidget->GetClass()->GetName(), ItemWidget);
        if (ItemWidget->GetClass()->GetSuperClass())
        {
            UE_LOG(LogTemp, Log, TEXT("ShopPanelWidget: ItemWidget parent class is '%s'"), *ItemWidget->GetClass()->GetSuperClass()->GetName());
        }
    }

    // Initialise avec les données
    ItemWidget->InitializeItem(ItemData, CurrentShop);

    // Ajoute au container approprié
    // If the item widget didn't bind its children (labels/images), create a fallback visual entry so we can still see the item in the UI.
    bool bHasNameText = false;
    if (ItemWidget && ItemWidget->HasBoundNameText())
    {
        bHasNameText = true;
    }

        if (!bHasNameText)
    {
        UE_LOG(LogTemp, Warning, TEXT("ShopPanelWidget: Item widget failed to bind ItemNameText - providing fallback text for %s"), *ItemData->GetName());
            // Also set a tooltip on the widget so users can find it when hovering
        if (ItemWidget)
        {
            FText Tooltip = FText::FromString(ItemData->DisplayName.IsEmpty() ? ItemData->GetName() : ItemData->DisplayName.ToString());
            ItemWidget->SetToolTipText(Tooltip);
        }
    }

    if (ItemsWrapBox)
    {
        ItemsWrapBox->AddChild(ItemWidget);
        UE_LOG(LogTemp, Log, TEXT("ShopPanelWidget: Added item widget to WrapBox for %s (Ptr=%p)"), *ItemData->DisplayName.ToString(), ItemData);
        // Append fallback text if binding failed
        if (!bHasNameText)
        {
            UTextBlock* FallbackText = nullptr;
            if (WidgetTree)
            {
                FallbackText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
            }
            else
            {
                FallbackText = NewObject<UTextBlock>(this);
            }
            if (FallbackText)
            {
                FallbackText->SetText(FText::FromString(ItemData->DisplayName.IsEmpty() ? ItemData->GetName() : ItemData->DisplayName.ToString()));
                ItemsWrapBox->AddChild(FallbackText);
            }
        }
        // Ensure item is definitely visible
        ItemWidget->SetVisibility(ESlateVisibility::Visible);
        ItemWidget->SetRenderOpacity(1.0f);
        UE_LOG(LogTemp, Log, TEXT("ShopPanelWidget: ItemWidget visible=%d visibility=%d opacity=%.2f"), ItemWidget->IsVisible(), (int32)ItemWidget->GetVisibility(), ItemWidget->GetRenderOpacity());
    }
    else if (ItemsScrollBox)
    {
        ItemsScrollBox->AddChild(ItemWidget);
        UE_LOG(LogTemp, Log, TEXT("ShopPanelWidget: Added item widget to ScrollBox for %s (Ptr=%p)"), *ItemData->DisplayName.ToString(), ItemData);
        // Append fallback text if binding failed
        if (!bHasNameText)
        {
            UTextBlock* FallbackText = nullptr;
            if (WidgetTree)
            {
                FallbackText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
            }
            else
            {
                FallbackText = NewObject<UTextBlock>(this);
            }
            if (FallbackText)
            {
                FallbackText->SetText(FText::FromString(ItemData->DisplayName.IsEmpty() ? ItemData->GetName() : ItemData->DisplayName.ToString()));
                ItemsScrollBox->AddChild(FallbackText);
            }
        }
        // Ensure item is definitely visible
        ItemWidget->SetVisibility(ESlateVisibility::Visible);
        ItemWidget->SetRenderOpacity(1.0f);
        UE_LOG(LogTemp, Log, TEXT("ShopPanelWidget: ItemWidget visible=%d visibility=%d opacity=%.2f"), ItemWidget->IsVisible(), (int32)ItemWidget->GetVisibility(), ItemWidget->GetRenderOpacity());
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
