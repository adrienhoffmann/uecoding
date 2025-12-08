#include "InventoryComponent.h"
#include "GameFramework/Pawn.h"
#include "MOBAPlayerController.h"
#include "PlayerHUDWidget.h"

UInventoryComponent::UInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UInventoryComponent, Items);
}

bool UInventoryComponent::AddItem(UItemData* Item)
{
    if (!Item) return false;

    // Only allow on server
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("InventoryComponent::AddItem called on non-server owner"));
        return false;
    }

    if (Items.Num() >= MaxSlots)
    {
        UE_LOG(LogTemp, Warning, TEXT("InventoryComponent::AddItem failed - inventory full"));
        return false;
    }

    Items.Add(Item);
    UE_LOG(LogTemp, Log, TEXT("InventoryComponent: Added item %s. New count=%d"), *Item->GetName(), Items.Num());

    // Since we modified replicated property on server, OnRep will be called on clients.
    // We also call it locally to update server-side HUD/UI if needed.
    OnRep_Items();
    return true;
}

bool UInventoryComponent::RemoveItemAt(int32 SlotIndex)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return false;
    if (SlotIndex < 0 || SlotIndex >= Items.Num()) return false;
    Items.RemoveAt(SlotIndex);
    OnRep_Items();
    return true;
}

void UInventoryComponent::OnRep_Items()
{
    UE_LOG(LogTemp, Verbose, TEXT("InventoryComponent::OnRep_Items - items=%d"), Items.Num());

    // Notify any listeners bound to this component
    OnInventoryChanged.Broadcast();

    // Try to refresh the HUD if this is the owning player's pawn
    APawn* PawnOwner = Cast<APawn>(GetOwner());
    if (PawnOwner)
    {
        AController* C = PawnOwner->GetController();
        if (C)
        {
            AMOBAPlayerController* PC = Cast<AMOBAPlayerController>(C);
            if (PC)
            {
                PC->RefreshHUDInventory();
            }
        }
    }
}
