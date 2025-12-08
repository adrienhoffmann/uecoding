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
#include "Components/SizeBox.h"
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

    // Debug: report whether ShopItemWidgetClass is set
    if (ShopItemWidgetClass)
    {
        if (ShopItemWidgetClass == UShopItemWidget::StaticClass())
        {
            UE_LOG(LogTemp, Error, TEXT("SHOP ERROR: ShopItemWidgetClass must be set to a Blueprint (e.g. WBP_Shop), not the raw C++ class!"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("SHOP: ShopItemWidgetClass is not set!"));
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
    }
    else
    {
        // Not in a canvas slot - force AddToViewport
        RemoveFromParent();
        AddToViewport(9999);
        if (GEngine && GEngine->GameViewport)
        {
            FVector2D ViewportSize;
            GEngine->GameViewport->GetViewportSize(ViewportSize);
            FVector2D DefaultSize(800.f, 500.f);
            SetDesiredSizeInViewport(DefaultSize);
            SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
            SetPositionInViewport(FVector2D::ZeroVector, false);
        }
        else
        {
            SetDesiredSizeInViewport(FVector2D(800.f, 500.f));
            SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
            SetPositionInViewport(FVector2D::ZeroVector, false);
        }
    }
    // Set focus and input mode for UI
    SetIsEnabled(true);
    SetIsFocusable(true);
    if (APlayerController* PC = GetOwningPlayer())
    {
        FInputModeGameAndUI Mode;
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        Mode.SetHideCursorDuringCapture(false);
        PC->SetInputMode(Mode);
        PC->bShowMouseCursor = true;
    }

    // Show explicit on-screen debug message with item count to help visual verification
    if (GEngine && Shop && Shop->ShopComponent)
    {
        FString ItemMsg = FString::Printf(TEXT("SHOP OPEN: %d items"), Shop->ShopComponent->AvailableItems.Num());
        GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor::Green, ItemMsg);
    }

    // Peuple la liste des items
    RefreshItems();
    // OpenShop called; items refreshed and UI set up

    // End OpenShop UI setup
    
    // Try to apply a temporary background tint at runtime if no visible background exists to aid debugging
    if (WidgetTree)
    {
        UWidget* Root = GetRootWidget();
        if (Root)
        {
            // If there's no border background, try to wrap content into a border at runtime
            UBorder* AsBorder = Cast<UBorder>(Root);
            if (AsBorder)
            {
                AsBorder->SetBrushColor(FLinearColor(0.0f, 0.2f, 0.6f, 0.6f));
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
        }
    }

    // Ensure visibility
    SetVisibility(ESlateVisibility::Visible);
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

    UE_LOG(LogTemp, Warning, TEXT("=== SHOP: RefreshItems - %d items available ==="), CurrentShop->ShopComponent->AvailableItems.Num());
    UE_LOG(LogTemp, Warning, TEXT("SHOP: ScrollBox=%s WrapBox=%s ShopItemWidgetClass=%s"),
        ItemsScrollBox ? TEXT("YES") : TEXT("NO"),
        ItemsWrapBox ? TEXT("YES") : TEXT("NO"),
        ShopItemWidgetClass ? *ShopItemWidgetClass->GetName() : TEXT("NULL"));

    // Log each available item
    for (int32 i = 0; i < CurrentShop->ShopComponent->AvailableItems.Num(); ++i)
    {
        UItemData* ItmDbg = CurrentShop->ShopComponent->AvailableItems[i];
        if (ItmDbg)
        {
            UE_LOG(LogTemp, Warning, TEXT("SHOP: Item[%d] = %s (DisplayName='%s', Cost=%d, Icon=%s)"),
                i, *ItmDbg->GetName(), *ItmDbg->DisplayName.ToString(), ItmDbg->Cost,
                ItmDbg->Icon ? *ItmDbg->Icon->GetName() : TEXT("NULL"));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("SHOP: Item[%d] = NULL"), i);
        }
    }

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

    // Force layout on the widget so it can compute desired size
    // Ensure the widget has had a chance to compute layout before we inspect size
    ItemWidget->ForceLayoutPrepass();

    // Log detailed binding status
    UE_LOG(LogTemp, Warning, TEXT("SHOP: ItemWidget for '%s' - HasNameText=%d, WidgetClass=%s"),
        *ItemData->GetName(),
        ItemWidget->HasBoundNameText() ? 1 : 0,
        *ItemWidget->GetClass()->GetName());

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
        // Wrap item widget in a SizeBox to enforce minimum visible size
        UWidget* WidgetToAdd = ItemWidget;
        if (WidgetTree)
        {
            USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
            if (SizeBox)
            {
                SizeBox->SetWidthOverride(200.f);
                SizeBox->SetHeightOverride(64.f);
                SizeBox->SetContent(ItemWidget);
                WidgetToAdd = SizeBox;
            }
        }
        ItemsWrapBox->AddChild(WidgetToAdd);
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
        // If the widget reports an extremely small desired size, provide a visible fallback
        FVector2D WidgetDesired = ItemWidget->GetDesiredSize();
        if (WidgetDesired.IsNearlyZero())
        {
            UE_LOG(LogTemp, Warning, TEXT("ShopPanelWidget: ItemWidget desired size is zero for %s; adding visual fallback border"), *ItemData->GetName());
            if (WidgetTree)
            {
                UBorder* FallbackBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
                UTextBlock* FallbackText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
                        if (FallbackBorder && FallbackText)
                {
                    FallbackText->SetText(FText::FromString(ItemData->DisplayName.IsEmpty() ? ItemData->GetName() : ItemData->DisplayName.ToString()));
                    FallbackText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
                    FallbackBorder->SetPadding(FMargin(4.0f));
                            FallbackBorder->SetContent(FallbackText);
                    ItemsWrapBox->AddChildToWrapBox(FallbackBorder);
                }
            }
        }
        // Force a layout update on the container
        ItemsWrapBox->ForceLayoutPrepass();
    }
    else if (ItemsScrollBox)
    {
        // Wrap item widget in a SizeBox to enforce minimum visible size
        UWidget* WidgetToAdd2 = ItemWidget;
        if (WidgetTree)
        {
            USizeBox* SizeBox2 = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
            if (SizeBox2)
            {
                SizeBox2->SetWidthOverride(200.f);
                SizeBox2->SetHeightOverride(64.f);
                SizeBox2->SetContent(ItemWidget);
                WidgetToAdd2 = SizeBox2;
            }
        }
        ItemsScrollBox->AddChild(WidgetToAdd2);
        UE_LOG(LogTemp, Warning, TEXT("SHOP: >>> Added ItemWidget to ScrollBox for '%s'"), *ItemData->DisplayName.ToString());
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
        // If the widget reports an extremely small desired size, provide a visible fallback
        FVector2D WidgetDesired2 = ItemWidget->GetDesiredSize();
        if (WidgetDesired2.IsNearlyZero())
        {
            UE_LOG(LogTemp, Warning, TEXT("ShopPanelWidget: ItemWidget desired size is zero for %s; adding visual fallback border"), *ItemData->GetName());
            if (WidgetTree)
            {
                UBorder* FallbackBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
                UTextBlock* FallbackText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
                if (FallbackBorder && FallbackText)
                {
                    FallbackText->SetText(FText::FromString(ItemData->DisplayName.IsEmpty() ? ItemData->GetName() : ItemData->DisplayName.ToString()));
                    FallbackText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
                    FallbackBorder->SetPadding(FMargin(4.0f));
                    FallbackBorder->SetContent(FallbackText);
                    ItemsScrollBox->AddChild(FallbackBorder);
                }
            }
        }
        // Force a layout update on the container
        ItemsScrollBox->ForceLayoutPrepass();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("SHOP: No container to add item widget!"));
    }
}

void UShopPanelWidget::OnCloseButtonClicked()
{
    CloseShop();
}
