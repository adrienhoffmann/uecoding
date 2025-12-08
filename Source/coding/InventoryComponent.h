#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Net/UnrealNetwork.h"
#include "ItemData.h"
#include "InventoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CODING_API UInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UInventoryComponent();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Add an item to the inventory on the server. Returns true if added.
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool AddItem(UItemData* Item);

    // Remove item at slot index (server)
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool RemoveItemAt(int32 SlotIndex);

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    const TArray<UItemData*>& GetItems() const { return Items; }

    UPROPERTY(BlueprintAssignable, Category = "Inventory")
    FOnInventoryChanged OnInventoryChanged;

protected:
    UPROPERTY(ReplicatedUsing=OnRep_Items)
    TArray<UItemData*> Items;

    UFUNCTION()
    void OnRep_Items();

    // Max number of inventory slots
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory")
    int32 MaxSlots = 6;
};