// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "MOBAPlayerController.generated.h"

class UHealthComponent;
class UCombatComponent;
class UInputMappingContext;
class UInputAction;
class UPlayerStatsComponent;
class UPlayerHUDWidget;
class UUserWidget;
class AMapPing;
class UNiagaraSystem;

UCLASS()
class CODING_API AMOBAPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// Refresh the HUD inventory display from controller (safe to call from other classes)
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RefreshHUDInventory();
	AMOBAPlayerController();

	// Move the camera to a world location (used by minimap clicks).
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void MoveCameraToWorldLocation(const FVector& WorldLocation);

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupInputComponent() override;

	// Enhanced Input
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* SetDestinationClickAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* SetDestinationTouchAction;

	// Right click action for attack/interact
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* RightClickAction;
    
	// Zoom (mouse wheel) action - Enhanced Input
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* ZoomAction;

	// Toggle camera lock (lock = camera follows pawn, unlock = free camera)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* ToggleCameraLockAction;

	// Camera lock properties
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	bool bCameraLocked;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float EdgeScrollMargin;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float EdgeScrollSpeed;

	// Allow panning when cursor is outside the game window by this many pixels
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float EdgeScrollOutsideMargin;

	// Player HUD widget class to create at runtime
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UUserWidget> PlayerHUDClass;

	// MapPing class to spawn (set to your BP_MapPing Blueprint with sounds configured)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ping")
	TSubclassOf<AMapPing> MapPingClass;

	UPROPERTY(Transient)
	ACameraActor* FreeCameraActor;

	// Current hovered actor under mouse
	UPROPERTY(BlueprintReadOnly, Category = "Targeting")
	AActor* HoveredActor;

	// Currently selected target
	UPROPERTY(BlueprintReadOnly, Category = "Targeting")
	AActor* SelectedTarget;

	// Maximum range for targeting
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	float TargetingRange;

	// Get hovered actor's health component
	UFUNCTION(BlueprintPure, Category = "Targeting")
	UHealthComponent* GetHoveredActorHealth() const;

	// Get selected target's health component
	UFUNCTION(BlueprintPure, Category = "Targeting")
	UHealthComponent* GetSelectedTargetHealth() const;

	// Convenience: get the PlayerStatsComponent from the controlled pawn
	UFUNCTION(BlueprintCallable, Category = "Stats")
	UPlayerStatsComponent* GetPlayerStatsComponent() const;

	// Convenience: get the InventoryComponent from the controlled pawn
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	class UInventoryComponent* GetPlayerInventoryComponent() const;

	// Server RPC to request a ping spawn at world location
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_RequestPing(const FVector& WorldLocation, EMapPingType PingType);

	// Server RPC to request movement to a world location (server authoritative)
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_RequestMoveTo(const FVector& WorldLocation);

	// Server RPC to request selecting/targeting an actor for attack (server authoritative)
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_RequestSetTarget(AActor* NewTarget);

	// Server RPC to request a purchase from a shop actor
	UFUNCTION(Server, Reliable, WithValidation, Category = "Shop")
	void Server_RequestPurchase(class AShopActor* Shop, class UItemData* Item);

	// Server RPC: spawn cursor effect at world location (called by client when right-clicking)
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SpawnCursorEffect(const FVector& WorldLocation);

	// Multicast RPC to spawn the cursor effect on all clients
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SpawnCursorEffect(const FVector& WorldLocation);

	// Client receives a navigation path (server->client) to display on minimap
	UFUNCTION(Client, Reliable)
	void Client_ReceiveNavPath(const TArray<FVector>& PathPoints);

	// Select a target for attack
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void SelectTarget(AActor* NewTarget);

	// Clear the selected target
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void ClearSelectedTarget();

	// Check if we're following a target to attack
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bIsFollowingTarget;

	// Maximum distance (in world units) to search for a shop when pressing the open shop hotkey
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	float ShopOpenRange = 2000.0f;

private:
	AActor* PreviousHoveredActor;
	FVector CachedDestination;
	bool bIsToDestination;
	float FollowTime;

	void UpdateMouseHover();
	void OnInputStarted();
	void OnSetDestinationTriggered();
	void OnSetDestinationReleased();
	void OnTouchTriggered();
	void OnTouchReleased();
	void OnRightClickTriggered();
	void OnLeftClickPressed();
	void HighlightActor(AActor* Actor, bool bHighlight);
	void OnZoom(const struct FInputActionValue& Value);
	// Runtime instance of the player HUD (C++ widget subclass)
	UPROPERTY(Transient)
	UPlayerHUDWidget* PlayerHUDWidget;

	// Handler called when stats change (bound to PlayerStatsComponent->OnStatsChanged)
	UFUNCTION()
	void OnStatsChanged_Handler();

	// Handler called when inventory changes
	UFUNCTION()
	void OnInventoryChanged_Handler();

	// Health / Mana event handlers (signatures match the component delegates)
	UFUNCTION()
	void OnHealthChanged_Handler(float Health, float MaxHealth, float DamageTaken);

	UFUNCTION()
	void OnManaChanged_Handler(float Mana, float MaxMana);

	void FollowAndAttackTarget();

	// Camera lock / free camera helpers (implemented in cpp)
	void OnToggleCameraLock();
	// Toggle minimap X/Y swap (debug)
	void OnToggleMinimapSwap();

	// Toggle minimap inversion flags (debug)
	void OnToggleMinimapInvertX();
	void OnToggleMinimapInvertY();
	void HandleEdgeScroll(float DeltaTime);
	void SpawnFreeCameraAt(const FVector& WorldLocation, const FRotator& WorldRotation);
	void DestroyFreeCamera();

	// Input handler: open nearest shop
	void OnOpenShopPressed();

	// Debug: increment clickable area padding and offset via keyboard
	void OnIncreaseClickablePaddingPressed();
	void OnDecreaseClickablePaddingPressed();
	void OnIncreaseClickableOffsetPressed();
	void OnDecreaseClickableScalePressed();
	// Toggle minimap debug overlay on-screen
	void OnToggleMinimapDebugOverlay();
	// Print minimap diagnostics
	void OnPrintMinimapDiagnostics();
	// Apply the last click reprojection delta to the HUD clickable offset (calibration)
	void OnApplyLastClickOffset();
	// Cycle force mapping mode for minimap (Auto/Geo/Cached)
	void OnCycleMinimapForceMapping();

	// Finds the closest shop to the controlled pawn, within ShopOpenRange, and opens it in the HUD
	void OpenClosestShop();

	// Niagara effect used for cursor clicks (set in BP or defaults)
	UPROPERTY(EditAnywhere, Category = "Effects")
	UNiagaraSystem* CursorClickEffect;
};
