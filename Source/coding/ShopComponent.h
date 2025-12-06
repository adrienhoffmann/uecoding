#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ItemData.h"
#include "ShopComponent.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CODING_API UShopComponent : public UActorComponent
{
    GENERATED_BODY()

public:    
    UShopComponent();

    // Items available in this shop
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
    TArray<UItemData*> AvailableItems;

    // Server RPC to request purchase
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_RequestPurchase(APlayerController* Buyer, UItemData* Item);

protected:
    virtual void BeginPlay() override;

    // Server-side purchase implementation
public:
    void HandlePurchase(APlayerController* Buyer, UItemData* Item);
};
