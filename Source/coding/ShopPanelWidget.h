// Widget C++ pour afficher le panneau du shop avec la liste des items
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShopPanelWidget.generated.h"

class AShopActor;
class UItemData;
class UScrollBox;
class UWrapBox;
class UButton;
class UShopItemWidget;

/**
 * Widget affichant le panneau complet du shop.
 * Créez un Widget Blueprint qui hérite de cette classe, avec les widgets suivants :
 * - ItemsScrollBox (ScrollBox) : Contient les items du shop (utilise un WrapBox à l'intérieur)
 * - ItemsWrapBox (WrapBox) : Grille flexible pour les items (optionnel, si vous préférez une grille)
 * - CloseButton (Button) : Bouton pour fermer le shop
 * 
 * Dans l'éditeur, configurez ShopItemWidgetClass avec votre WBP_ShopItem Blueprint.
 */
UCLASS(Blueprintable)
class CODING_API UShopPanelWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // Ouvre le shop et peuple la liste des items
    UFUNCTION(BlueprintCallable, Category = "Shop")
    void OpenShop(AShopActor* Shop);

    // Ferme le shop
    UFUNCTION(BlueprintCallable, Category = "Shop")
    void CloseShop();

    // Rafraîchit la liste des items (utile après un achat)
    UFUNCTION(BlueprintCallable, Category = "Shop")
    void RefreshItems();

    // Retourne true si le shop est actuellement ouvert
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Shop")
    bool IsShopOpen() const { return bIsOpen; }

    // Référence au shop actuellement ouvert
    UPROPERTY(BlueprintReadOnly, Category = "Shop")
    AShopActor* CurrentShop;

    // Classe du widget d'item à instancier (configurez dans l'éditeur avec votre WBP_ShopItem)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
    TSubclassOf<UShopItemWidget> ShopItemWidgetClass;

protected:
    // ScrollBox contenant les items
    UPROPERTY(meta = (BindWidgetOptional))
    UScrollBox* ItemsScrollBox;

    // WrapBox pour une grille flexible (alternative au ScrollBox direct)
    UPROPERTY(meta = (BindWidgetOptional))
    UWrapBox* ItemsWrapBox;

    // Bouton pour fermer le shop
    UPROPERTY(meta = (BindWidgetOptional))
    UButton* CloseButton;

    virtual void NativeConstruct() override;

private:
    bool bIsOpen = false;

    UFUNCTION()
    void OnCloseButtonClicked();

    // Crée et ajoute un widget d'item au container approprié
    void AddItemWidget(UItemData* ItemData);

    // Efface tous les widgets d'items
    void ClearItems();
};
