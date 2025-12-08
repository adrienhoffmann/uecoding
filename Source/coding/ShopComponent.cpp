#include "ShopComponent.h"
#include "MOBAPlayerController.h"
#include "PlayerStatsComponent.h"
#include "InventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "Logging.h"

UShopComponent::UShopComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UShopComponent::BeginPlay()
{
    Super::BeginPlay();
}

bool UShopComponent::Server_RequestPurchase_Validate(APlayerController* Buyer, UItemData* Item)
{
    // Basic validation — ensure non-null
    return Buyer != nullptr && Item != nullptr;
}

void UShopComponent::Server_RequestPurchase_Implementation(APlayerController* Buyer, UItemData* Item)
{
    HandlePurchase(Buyer, Item);
}

void UShopComponent::HandlePurchase(APlayerController* Buyer, UItemData* Item)
{
    UE_LOG(LogCoding, Warning, TEXT("SHOP HandlePurchase: Buyer=%s Item=%s"), 
        Buyer ? *Buyer->GetName() : TEXT("NULL"),
        Item ? *Item->GetName() : TEXT("NULL"));

    if (!Buyer || !Item) return;

    // Only process purchases on the server
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        UE_LOG(LogCoding, Warning, TEXT("SHOP HandlePurchase: Not on server, aborting"));
        return;
    }

    // Ensure the requested item belongs to this shop
    if (!AvailableItems.Contains(Item))
    {
        UE_LOG(LogCoding, Warning, TEXT("ShopComponent: Purchase denied — item not in this shop"));
        return;
    }

    // Get player's stats component
    AMOBAPlayerController* MPC = Cast<AMOBAPlayerController>(Buyer);
    if (!MPC)
    {
        UE_LOG(LogCoding, Warning, TEXT("ShopComponent: Purchase denied — buyer is not a MOBAPlayerController"));
        return;
    }

    UPlayerStatsComponent* Stats = MPC->GetPlayerStatsComponent();
    if (!Stats) return;

    // Check inventory capacity (do this before spending gold to avoid refund edge cases)
    UInventoryComponent* InvCandidate = nullptr;
    if (MPC)
    {
        InvCandidate = MPC->GetPlayerInventoryComponent();
    }
    if (InvCandidate && !InvCandidate->HasSpace())
    {
        UE_LOG(LogCoding, Display, TEXT("Shop: Buyer %s inventory full - cannot purchase %s"), *Buyer->GetName(), *Item->DisplayName.ToString());
        return;
    }

    // Check gold and spend
    if (Stats->Gold < Item->Cost)
    {
        UE_LOG(LogCoding, Display, TEXT("Shop: Buyer %s lacks gold for %s (cost %d, have %d)"), *Buyer->GetName(), *Item->DisplayName.ToString(), Item->Cost, Stats->Gold);
        return;
    }

    // Spend gold (server-side) and apply modifiers
    Stats->SpendGold(Item->Cost);
    Stats->ApplyItemModifiers(Item->Modifier);

    UE_LOG(LogCoding, Display, TEXT("Shop: Buyer %s purchased %s"), *Buyer->GetName(), *Item->DisplayName.ToString());

    // Add item to buyer's inventory component (if present)
    if (MPC)
    {
        UInventoryComponent* Inv = MPC->GetPlayerInventoryComponent();
        if (Inv)
        {
            if (Inv->AddItem(Item))
            {
                UE_LOG(LogCoding, Display, TEXT("Shop: Added %s to %s inventory"), *Item->GetName(), *Buyer->GetName());
            }
            else
            {
                UE_LOG(LogCoding, Warning, TEXT("Shop: Failed to add %s to %s inventory (full?)"), *Item->GetName(), *Buyer->GetName());
            }
        }
        else
        {
            UE_LOG(LogCoding, Warning, TEXT("Shop: Buyer %s has no InventoryComponent"), *Buyer->GetName());
        }
    }
}
