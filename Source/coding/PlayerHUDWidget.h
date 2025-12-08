// Simple C++ UserWidget to display player stats and integrated minimap.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MapPing.h"
#include "FogOfWarManager.h"
#include "PlayerHUDWidget.generated.h"

class UTextBlock;
class UShopPanelWidget;
class AShopActor;
class UProgressBar;
class UImage;
class UButton;
class UHorizontalBox;
class UPanelWidget;
class UPlayerStatsComponent;
class AMapPing;
class AActor;
class AFogOfWarManager;

UCLASS()
class CODING_API UPlayerHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // Called to refresh the widget from a stats component
    UFUNCTION(BlueprintCallable, Category = "HUD")
    void UpdateFromStats(UPlayerStatsComponent* Stats);

    // Refresh the 6 item slots UI with current inventory items
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void RefreshInventory(const TArray<UItemData*>& Items);

    UFUNCTION(BlueprintCallable, Category = "HUD")
    void ShowStatsPopup();

    UFUNCTION(BlueprintCallable, Category = "HUD")
    void HideStatsPopup();

    UFUNCTION(BlueprintCallable, Category = "HUD")
    void ToggleStatsPopup();

    // ===== Minimap Functions =====
    // Convert a world location to normalized [0,1] minimap coords (assuming top-down X,Y)
    UFUNCTION(BlueprintCallable, Category = "Minimap")
    FVector2D WorldToMinimapNormalized(const FVector& WorldLocation) const;

    // Set the navigation path points to display on the minimap
    UFUNCTION(BlueprintCallable, Category = "Minimap")
    void SetMinimapPath(const TArray<FVector>& PathPoints);

protected:
    // These should be named exactly like the TextBlocks in your UMG designer
    // If names differ, the pointers will simply be null and UpdateFromStats will skip them.
    UPROPERTY(meta = (BindWidget))
    UTextBlock* Armor;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* AP;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* AD;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* AS;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* Crit;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* MagicResist;

    // Health / Mana
    UPROPERTY(meta = (BindWidget))
    UProgressBar* HealthBar;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* HealthText;

    UPROPERTY(meta = (BindWidget))
    UProgressBar* ManaBar;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* ManaText;

    // Gold / XP
    UPROPERTY(meta = (BindWidget))
    UTextBlock* GoldText;

    UPROPERTY(meta = (BindWidget))
    UProgressBar* XPBar;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* XPText;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* LevelText;

    // Character art
    UPROPERTY(meta = (BindWidget))
    UImage* CharacterArt;

    // ===== Minimap Widgets =====
    // The MapImage inside MinimapContainer - used for click detection and drawing
    UPROPERTY(meta = (BindWidgetOptional))
    UImage* MapImage;

    // Container for the minimap (used for click detection bounds)
    UPROPERTY(meta = (BindWidgetOptional))
    UPanelWidget* MinimapContainer;

    // World bounds represented on the minimap (X,Y half extents)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
    FVector2D WorldBoundsHalfSize = FVector2D(12000.f, 12000.f);

    // World center (X,Y) corresponding to minimap center. If zero, the origin is used.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
    FVector2D WorldCenter = FVector2D(0.f, 0.f);

    // Allow toggling/inverting axes in case the world/camera orientation differs from the minimap texture
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
    bool bInvertMinimapX = false;

    // Vertical inversion default: true because camera pitch/coordinate setup in this project makes +Y appear
    // opposite on the minimap texture in many scenes.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
    bool bInvertMinimapY = false;

    // If true, attempt to auto-detect whether the minimap's Y axis should be inverted based on camera projection
    // Default false to avoid overriding an explicit default inversion in editor builds.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
    bool bAutoDetectMinimapY = false;

    // Automatically detect world bounds from registered minimap components (if true)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
    bool bAutoDetectWorldBounds = true;

    // Extra margin (in world units) to add around detected extents when auto-detecting world bounds
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
    float AutoDetectWorldBoundsMargin = 500.0f;

public:
    // Runtime control: allow toggling inversion from Blueprints or console via the PlayerController
    UFUNCTION(BlueprintCallable, Category = "Minimap")
    void SetMinimapInversion(bool InvertX, bool InvertY, bool bSwapXY);

    UFUNCTION(BlueprintCallable, Category = "Minimap")
    void ToggleSwapMinimapXY();

    UFUNCTION(BlueprintCallable, Category = "Minimap")
    void ToggleInvertMinimapX();

    UFUNCTION(BlueprintCallable, Category = "Minimap")
    void ToggleInvertMinimapY();

    // If true, swap X<->Y mapping between world and minimap (useful when diagonals appear mirrored)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
    bool bSwapMinimapXY = true;

    // Ping wheel UI class (optional) - set this to a UMG widget blueprint that shows ping options
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
    TSubclassOf<UUserWidget> PingWheelClass;

    // Runtime instance of the ping wheel
    UPROPERTY(Transient)
    UUserWidget* PingWheelInstance;

    // Item slots - 6 images for inventory display
    UPROPERTY(meta = (BindWidgetOptional))
    UImage* Item_1;

    UPROPERTY(meta = (BindWidgetOptional))
    UImage* Item_2;

    UPROPERTY(meta = (BindWidgetOptional))
    UImage* Item_3;

    UPROPERTY(meta = (BindWidgetOptional))
    UImage* Item_4;

    UPROPERTY(meta = (BindWidgetOptional))
    UImage* Item_5;

    UPROPERTY(meta = (BindWidgetOptional))
    UImage* Item_6;

    // Ability / spell buttons
    UPROPERTY(meta = (BindWidgetOptional))
    UButton* SpellButton_Q;

    UPROPERTY(meta = (BindWidgetOptional))
    UButton* SpellButton_W;

    UPROPERTY(meta = (BindWidgetOptional))
    UButton* SummonerButton_1;

    UPROPERTY(meta = (BindWidgetOptional))
    UButton* SummonerButton_2;

    // Passive ability image
    UPROPERTY(meta = (BindWidgetOptional))
    UImage* PassiveImage;

    // Popup panel for detailed stats
    UPROPERTY(meta = (BindWidgetOptional))
    UWidget* StatsPopupPanel;

    // Optional debug text showing number of registered minimap icons
    UPROPERTY(meta = (BindWidgetOptional))
    class UTextBlock* Debug_IconCount;

    // Minimap logging controls (to avoid spamming the log every tick)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Debug")
    bool bEnableMinimapLogging = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Debug")
    bool bMinimapVerboseLogging = false;

    // Minimum seconds between successive minimap icon debug prints (when enabled)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|Debug")
    float MinimapLogInterval = 1.0f;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

public:
    // Enable / configure minimap logging at runtime
    UFUNCTION(BlueprintCallable, Category = "Minimap|Debug")
    void SetMinimapLogging(bool bEnable, bool bVerbose, float IntervalSeconds);

private:
    void BindButtonHandlers();
    UFUNCTION()
    void OnSpellQClicked();

    UFUNCTION()
    void OnSpellWClicked();

    UFUNCTION()
    void OnSummoner1Clicked();

    UFUNCTION()
    void OnSummoner2Clicked();

    // ===== Minimap Internal Data =====
    // Cached pings discovered each tick
    TArray<TWeakObjectPtr<AMapPing>> CachedPings;
    // Navigation path (world points) to draw on the minimap
    TArray<FVector> NavPathPoints;
    
    // Minimap icons collected each tick
    struct FMinimapIcon
    {
        FVector WorldLocation;
        FVector2D Normalized;
        FLinearColor Color;
        float Size;
        TWeakObjectPtr<AActor> OwnerActor;
        int32 TeamID;  // For fog of war visibility check
    };
    TArray<FMinimapIcon> CachedIcons;

    // Registered minimap components (avoid expensive world scan each tick)
    TArray<TWeakObjectPtr<class UMinimapComponent>> RegisteredMinimapComponents;

    // Fog of War manager reference
    UPROPERTY(Transient)
    AFogOfWarManager* FogManager = nullptr;

    // Local player's team ID for visibility checks
    int32 LocalPlayerTeamID = 0;

    // Timestamp of last per-icon logging (seconds, from world time)
    double LastIconLogTime = 0.0;

public:
    // Register / Unregister minimap components (called by UMinimapComponent in BeginPlay/EndPlay)
    UFUNCTION(BlueprintCallable, Category = "Minimap")
    bool RegisterMinimapComponent(UMinimapComponent* Comp);

    UFUNCTION(BlueprintCallable, Category = "Minimap")
    void UnregisterMinimapComponent(UMinimapComponent* Comp);

    // Destruction marks (added from Blueprint or code when environment is destroyed)
    UPROPERTY(Transient)
    TArray<FVector> DestructionMarks;

private:
    // Debug: store last minimap click for on-screen diagnostics
    FVector2D LastClickNormalized;
    FVector LastClickWorld;
    bool bHasLastClick = false;

    // Internal flag: have we auto-detected world bounds yet? (avoid repeated recomputation)
    bool bWorldBoundsAutoDetected = false;

    // Path drawing update controls
    // Seconds between recomputing the nav path for the minimap while moving
    UPROPERTY(EditAnywhere, Category = "Minimap|Path")
    float MinimapPathUpdateInterval = 0.15f;

    // Internal accumulator for time since last path update
    double PathUpdateAccumulator = 0.0;

    // Threshold (world units) the pawn must move before forcing a path recompute
    UPROPERTY(EditAnywhere, Category = "Minimap|Path")
    float PathRecomputeDistanceThreshold = 50.0f;

    // Last path target used to compute NavPathPoints
    FVector LastPathTarget = FVector::ZeroVector;

    // Last cached pawn location used to decide if path should be recomputed
    FVector LastPawnLocation = FVector::ZeroVector;

public:
    // For debugging: set the last click info so NativePaint can display it
    UFUNCTION(BlueprintCallable, Category = "Minimap|Debug")
    void SetLastClickDebug(const FVector2D& Normalized, const FVector& WorldLocation);

public:
    // Add a destruction mark at world location (call from Blueprint when an object is destroyed)
    UFUNCTION(BlueprintCallable, Category = "Minimap")
    void AddDestructionMark(const FVector& WorldLocation);

public:
    // Show the ping wheel centered at a given world location (projects to screen and positions widget)
    UFUNCTION(BlueprintCallable, Category = "PingWheel")
    void ShowPingWheelAtWorldLocation(const FVector& WorldLocation);
    
    // Show the ping wheel directly at a screen location (e.g. minimap click), centered at that pixel position
    UFUNCTION(BlueprintCallable, Category = "PingWheel")
    void ShowPingWheelAtScreenLocation(const FVector2D& ScreenLocation);

    // Request to purchase an item from a shop actor (calls server via playercontroller). Implemented in PlayerHUDWidget to expose to UMG
    UFUNCTION(BlueprintCallable, Category = "Shop")
    void RequestPurchase(class AShopActor* Shop, class UItemData* Item);

    // ===== Shop Panel Integration =====
    // Ouvre le panneau du shop avec les items disponibles
    UFUNCTION(BlueprintCallable, Category = "Shop")
    void OpenShop(AShopActor* Shop);

    // Ferme le panneau du shop
    UFUNCTION(BlueprintCallable, Category = "Shop")
    void CloseShop();

    // Toggle le panneau du shop
    UFUNCTION(BlueprintCallable, Category = "Shop")
    void ToggleShop(AShopActor* Shop);

    // Retourne true si le shop est ouvert
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Shop")
    bool IsShopOpen() const;

protected:
    // Panneau du shop (optionnel - bindez à un widget ShopPanelWidget dans votre HUD)
    UPROPERTY(meta = (BindWidgetOptional))
    UShopPanelWidget* ShopPanel;

    // Optional: class to create a runtime ShopPanel if none is present in the HUD
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
    TSubclassOf<UShopPanelWidget> ShopPanelClass;

    // Runtime instance of the shop panel created by code if fallback is needed
    UPROPERTY(Transient)
    UShopPanelWidget* RuntimeShopPanelInstance;

    // Optional debug overlay to force visual confirmation when shop opens
    UPROPERTY(Transient)
    class UDebugShopOverlayWidget* DebugShopOverlayInstance;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
    TSubclassOf<class UDebugShopOverlayWidget> DebugShopOverlayClass;
};
