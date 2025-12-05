// Simple C++ UserWidget to display player stats and integrated minimap.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MapPing.h"
#include "FogOfWarManager.h"
#include "PlayerHUDWidget.generated.h"

class UTextBlock;
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

    // Item slots (designer can place as many Image widgets as desired and name them Item_1 .. Item_6)
    UPROPERTY(meta = (BindWidgetOptional))
    UHorizontalBox* ItemsBox;

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

public:
    // For debugging: set the last click info so NativePaint can display it
    UFUNCTION(BlueprintCallable, Category = "Minimap|Debug")
    void SetLastClickDebug(const FVector2D& Normalized, const FVector& WorldLocation);

public:
    // Add a destruction mark at world location (call from Blueprint when an object is destroyed)
    UFUNCTION(BlueprintCallable, Category = "Minimap")
    void AddDestructionMark(const FVector& WorldLocation);
};
