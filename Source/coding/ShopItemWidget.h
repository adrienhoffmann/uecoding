// Widget C++ pour afficher un item individuel dans le shop
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShopItemWidget.generated.h"

class UItemData;
class AShopActor;
class UImage;
class UTextBlock;
class UButton;

/**
 * Widget affichant un item dans le shop.
 * Créez un Widget Blueprint qui hérite de cette classe, avec les widgets suivants :
 * - ItemIcon (Image) : Affiche l'icône de l'item
 * - ItemNameText (TextBlock) : Affiche le nom de l'item
 * - ItemCostText (TextBlock) : Affiche le coût de l'item
 * - BuyButton (Button) : Bouton pour acheter l'item
 */
UCLASS(Blueprintable)
class CODING_API UShopItemWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // Initialise le widget avec les données de l'item et une référence au shop
    UFUNCTION(BlueprintCallable, Category = "Shop")
    void InitializeItem(UItemData* InItemData, AShopActor* InShopActor);

    // L'item affiché par ce widget
    UPROPERTY(BlueprintReadOnly, Category = "Shop")
    UItemData* ItemData;

    // Référence au shop pour l'achat
    UPROPERTY(BlueprintReadOnly, Category = "Shop")
    AShopActor* ShopActor;

    // Returns whether the name text is bound in this widget (BindWidget succeeded in blueprint)
    UFUNCTION(BlueprintCallable, Category = "Shop")
    bool HasBoundNameText() const;

    

    

    

protected:
    // Image de l'icône de l'item
    UPROPERTY(meta = (BindWidgetOptional))
    UImage* ItemIcon;

    // Texte du nom de l'item
    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* ItemNameText;

    // Texte du coût de l'item
    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* ItemCostText;

    // Bouton d'achat
    UPROPERTY(meta = (BindWidgetOptional))
    UButton* BuyButton;

    virtual void NativeConstruct() override;

private:
    UFUNCTION()
    void OnBuyButtonClicked();

    // Met à jour l'affichage visuel depuis ItemData
    void RefreshDisplay();

    
};
