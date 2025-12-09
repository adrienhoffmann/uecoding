// Implementation for the PlayerHUDWidget with integrated minimap
#include "PlayerHUDWidget.h"
#include "Components/TextBlock.h"
#include "PlayerStatsComponent.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "HealthComponent.h"
#include "AbilityComponent.h"
#include "MOBACharacter.h"
#include "Kismet/KismetTextLibrary.h"
#include "Logging.h"
#include "Components/PanelWidget.h"
#include "MapPing.h"
#include "MOBAPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Rendering/DrawElements.h"
#include "Framework/Application/SlateApplication.h"
#include "PingWheelWidget.h"
#include "MinimapComponent.h"
#include "Styling/CoreStyle.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "FogOfWarManager.h"
#include "VisionSourceComponent.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "AI/Navigation/NavigationTypes.h"
#include "ShopActor.h"
#include "DebugShopOverlayWidget.h"
#include "ShopComponent.h"
#include "ItemData.h"
#include "ShopPanelWidget.h"
#include "InventoryComponent.h"
#include "Blueprint/WidgetTree.h"

void UPlayerHUDWidget::UpdateFromStats(UPlayerStatsComponent* Stats)
{
    if (!Stats) return;

    // Gold
    if (GoldText)
    {
        GoldText->SetText(FText::AsNumber(Stats->Gold));
    }

    // Detailed stats
    if (Armor)
    {
        Armor->SetText(FText::AsNumber(FMath::RoundToInt(Stats->GetArmor())));
    }

    if (AD)
    {
        AD->SetText(FText::AsNumber(FMath::RoundToInt(Stats->AttackDamage)));
    }

    if (AP)
    {
        AP->SetText(FText::AsNumber(FMath::RoundToInt(Stats->AbilityPower)));
    }

    if (AS)
    {
        // Attack Speed is usually shown as a float (e.g., 0.70)
        AS->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), Stats->AttackSpeed)));
    }

    if (Crit)
    {
        // Crit chance as percentage
        Crit->SetText(FText::FromString(FString::Printf(TEXT("%.0f%%"), Stats->CritChance * 100.f)));
    }

    if (MagicResist)
    {
        MagicResist->SetText(FText::AsNumber(FMath::RoundToInt(Stats->MagicResist)));
    }


    if (XPBar)
    {
        XPBar->SetPercent(Stats->GetXPPercent());
    }

    if (XPText)
    {
        int32 CurXP = Stats->CurrentXP;
        int32 ToNext = Stats->XPToNextLevel;
        XPText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), CurXP, ToNext)));
    }

    if (LevelText)
    {
        LevelText->SetText(FText::AsNumber(Stats->CurrentLevel));
    }

    // Health and Mana come from components on the pawn (if available)
    APlayerController* PC = GetOwningPlayer();
    if (PC)
    {
        APawn* P = PC->GetPawn();
        if (P)
        {
            if (AMOBACharacter* C = Cast<AMOBACharacter>(P))
            {
                if (C->GetHealthComponent())
                {
                    float HPPct = C->GetHealthComponent()->GetHealthPercent();
                    if (HealthBar) HealthBar->SetPercent(HPPct);
                    if (HealthText)
                    {
                        float H = C->GetHealthComponent()->Health;
                        float M = C->GetHealthComponent()->MaxHealth;
                        HealthText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), H, M)));
                    }
                }

                if (C->GetComponentByClass(UAbilityComponent::StaticClass()))
                {
                    UAbilityComponent* Ab = Cast<UAbilityComponent>(C->GetComponentByClass(UAbilityComponent::StaticClass()));
                    if (Ab)
                    {
                        float ManaPct = Ab->GetManaPercent();
                        if (ManaBar) ManaBar->SetPercent(ManaPct);
                        if (ManaText)
                        {
                            ManaText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), Ab->Mana, Ab->MaxMana)));
                        }
                    }
                }
            }
        }
    }
    // End UpdateFromStats
}

void UPlayerHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    BindButtonHandlers();

    // Make the widget focusable to receive mouse events
    SetIsFocusable(true);

    UE_LOG(LogCoding, Log, TEXT("PlayerHUDWidget::NativeConstruct - HUD constructed and ready. WorldBoundsHalfSize=(%.1f,%.1f) WorldCenter=(%.1f,%.1f) AutoDetectWorldBounds=%d"), WorldBoundsHalfSize.X, WorldBoundsHalfSize.Y, WorldCenter.X, WorldCenter.Y, bAutoDetectWorldBounds ? 1 : 0);

    // Load the minimap texture if not already set
    if (!MinimapTexture && !MinimapTexturePath.IsEmpty())
    {
        MinimapTexture = LoadObject<UTexture2D>(nullptr, *MinimapTexturePath);
        if (MinimapTexture)
        {
            UE_LOG(LogCoding, Log, TEXT("PlayerHUDWidget: Loaded minimap texture from path: %s"), *MinimapTexturePath);
        }
    }

    // Get Fog of War manager reference
    FogManager = AFogOfWarManager::GetInstance(GetWorld());
    if (FogManager)
    {
        UE_LOG(LogCoding, Log, TEXT("PlayerHUDWidget: FogOfWarManager found"));
    }

    // Get local player's team ID for fog visibility checks
    if (APlayerController* PC = GetOwningPlayer())
    {
        if (APawn* Pawn = PC->GetPawn())
        {
            // Try to get team from VisionSourceComponent
            if (UVisionSourceComponent* VSC = Pawn->FindComponentByClass<UVisionSourceComponent>())
            {
                LocalPlayerTeamID = VSC->TeamID;
                UE_LOG(LogCoding, Log, TEXT("PlayerHUDWidget: LocalPlayerTeamID=%d"), LocalPlayerTeamID);
            }
        }
    }

    // Auto-detect minimap Y inversion if requested: project two world points (+Y offset) and compare screen Y
    if (bAutoDetectMinimapY)
    {
        if (APlayerController* PC = GetOwningPlayer())
        {
            FVector P0 = FVector::ZeroVector;
            FVector P1 = FVector(0.0f, 1000.0f, 0.0f); // +Y direction test
            FVector2D S0, S1;
            bool b0 = PC->ProjectWorldLocationToScreen(P0, S0);
            bool b1 = PC->ProjectWorldLocationToScreen(P1, S1);
            if (b0 && b1)
            {
                // Screen Y increases downwards. If +Y world projects to larger screen Y, then world+Y is down on screen -> invert mapping
                bInvertMinimapY = (S1.Y > S0.Y);
                UE_LOG(LogCoding, Log, TEXT("PlayerHUDWidget: Auto-detected minimap Y inversion = %d (S0=%.1f S1=%.1f)"), bInvertMinimapY ? 1 : 0, S0.Y, S1.Y);
            }
            else
            {
                UE_LOG(LogCoding, VeryVerbose, TEXT("PlayerHUDWidget: Could not auto-detect minimap Y inversion (project failed)"));
            }
        }
    }
    // Log current inversion state so it's obvious in the output
    UE_LOG(LogCoding, Log, TEXT("PlayerHUDWidget: bInvertMinimapX=%d bInvertMinimapY=%d bAutoDetectMinimapY=%d"), bInvertMinimapX?1:0, bInvertMinimapY?1:0, bAutoDetectMinimapY?1:0);

    // Debug: Report whether a ShopPanel reference or template classes are set
    UE_LOG(LogTemp, Log, TEXT("PlayerHUDWidget: ShopPanel pointer=%s ShopPanelClass=%s DebugShopOverlayClass=%s"),
        ShopPanel ? *ShopPanel->GetName() : TEXT("NULL"),
        ShopPanelClass ? *ShopPanelClass->GetName() : TEXT("NULL"),
        DebugShopOverlayClass ? *DebugShopOverlayClass->GetName() : TEXT("NULL"));
}

// ===== Minimap Functions =====

FVector2D UPlayerHUDWidget::WorldToMinimapNormalized(const FVector& WorldLocation) const
{
    FVector2D Norm;
    if (WorldBoundsHalfSize.X > 0 && WorldBoundsHalfSize.Y > 0)
    {
        float WX = (WorldLocation.X - WorldCenter.X) / (2.0f * WorldBoundsHalfSize.X);
        float WY = (WorldLocation.Y - WorldCenter.Y) / (2.0f * WorldBoundsHalfSize.Y);

        // Apply inversion flags first
        if (bInvertMinimapX) WX = -WX;
        if (bInvertMinimapY) WY = -WY;

        // Optionally swap axes if minimap uses X/Y transposed
        if (bSwapMinimapXY)
        {
            float Tmp = WX;
            WX = WY;
            WY = Tmp;
        }

        Norm.X = WX + 0.5f;
        Norm.Y = 0.5f - WY;
    }
    return Norm;
}

FVector2D UPlayerHUDWidget::GetMinimapTopLeftLocal(const FGeometry& Geometry) const
{
    FVector2D WidgetSize = Geometry.GetLocalSize();
    FVector2D Size = MinimapSize;
    FVector2D TopLeft;
    switch (MinimapAnchorCorner)
    {
        case 0: // TopLeft
            TopLeft = FVector2D(MinimapOffset.X, MinimapOffset.Y);
            break;
        case 1: // TopRight
            TopLeft = FVector2D(WidgetSize.X - Size.X - MinimapOffset.X, MinimapOffset.Y);
            break;
        case 2: // BottomLeft
            TopLeft = FVector2D(MinimapOffset.X, WidgetSize.Y - Size.Y - MinimapOffset.Y);
            break;
        case 3: // BottomRight
        default:
            TopLeft = FVector2D(WidgetSize.X - Size.X - MinimapOffset.X, WidgetSize.Y - Size.Y - MinimapOffset.Y);
            break;
    }
    return TopLeft;
}

bool UPlayerHUDWidget::ScreenPositionToWorld(const FGeometry& Geometry, const FVector2D& ScreenPosition, FVector& OutWorldLocation, FVector2D* OutMinimapLocal /*= nullptr*/) const
{
    FVector2D LocalPos = Geometry.AbsoluteToLocal(ScreenPosition);
    FVector2D TopLeft = GetMinimapTopLeftLocal(Geometry);
    FVector2D MinimapLocalPos = LocalPos - TopLeft;
    if (OutMinimapLocal) *OutMinimapLocal = MinimapLocalPos;

    if (MinimapLocalPos.X < 0 || MinimapLocalPos.Y < 0 || MinimapLocalPos.X > MinimapSize.X || MinimapLocalPos.Y > MinimapSize.Y)
    {
        return false;
    }

    FVector2D Normalized(MinimapLocalPos.X / MinimapSize.X, MinimapLocalPos.Y / MinimapSize.Y);
    float NX = (Normalized.X - 0.5f);
    float NY = (0.5f - Normalized.Y);

    if (bSwapMinimapXY)
    {
        float Tmp = NX;
        NX = NY;
        NY = Tmp;
    }
    if (bInvertMinimapX) NX = -NX;
    if (bInvertMinimapY) NY = -NY;

    OutWorldLocation.X = WorldCenter.X + NX * 2.0f * WorldBoundsHalfSize.X;
    OutWorldLocation.Y = WorldCenter.Y + NY * 2.0f * WorldBoundsHalfSize.Y;
    OutWorldLocation.Z = 0.0f;
    return true;
}

bool UPlayerHUDWidget::IsScreenPositionOverMinimap(const FVector2D& ScreenPosition) const
{
    // Get the cached geometry from the widget; fallback to viewport-based computation if invalid
    const FGeometry& Geo = GetCachedGeometry();
    if (Geo.GetLocalSize().IsNearlyZero())
    {
        // Fallback to viewport computation
        FVector2D TopLeft = GetMinimapTopLeftFromViewport();
        FVector2D MinimapLocalPos = ScreenPosition - TopLeft;
        UE_LOG(LogCoding, Verbose, TEXT("IsScreenPositionOverMinimap: Using viewport fallback TopLeft=(%.1f,%.1f) Screen=(%.1f,%.1f) MinimapLocal=(%.1f,%.1f) Size=(%.1f,%.1f)"), TopLeft.X, TopLeft.Y, ScreenPosition.X, ScreenPosition.Y, MinimapLocalPos.X, MinimapLocalPos.Y, MinimapSize.X, MinimapSize.Y);
        return (MinimapLocalPos.X >= 0 && MinimapLocalPos.Y >= 0 && MinimapLocalPos.X <= MinimapSize.X && MinimapLocalPos.Y <= MinimapSize.Y);
    }
    
    FVector2D LocalPos = Geo.AbsoluteToLocal(ScreenPosition);
    FVector2D TopLeft = GetMinimapTopLeftLocal(Geo);
    FVector2D MinimapLocalPos = LocalPos - TopLeft;
    UE_LOG(LogCoding, Verbose, TEXT("IsScreenPositionOverMinimap: GeoSize=(%.1f,%.1f) TopLeft=(%.1f,%.1f) Screen=(%.1f,%.1f) MinimapLocal=(%.1f,%.1f) Size=(%.1f,%.1f)"), Geo.GetLocalSize().X, Geo.GetLocalSize().Y, TopLeft.X, TopLeft.Y, ScreenPosition.X, ScreenPosition.Y, MinimapLocalPos.X, MinimapLocalPos.Y, MinimapSize.X, MinimapSize.Y);
    
    return (MinimapLocalPos.X >= 0 && MinimapLocalPos.Y >= 0 && 
            MinimapLocalPos.X <= MinimapSize.X && MinimapLocalPos.Y <= MinimapSize.Y);
}

FVector2D UPlayerHUDWidget::GetMinimapTopLeftFromViewport() const
{
    FVector2D ViewportSize(1920, 1080);
    if (GEngine && GEngine->GameViewport)
    {
        FVector2D VPSize;
        GEngine->GameViewport->GetViewportSize(VPSize);
        ViewportSize = VPSize;
    }

    FVector2D Size = MinimapSize;
    FVector2D TopLeft;
    switch (MinimapAnchorCorner)
    {
        case 0: // TopLeft
            TopLeft = FVector2D(MinimapOffset.X, MinimapOffset.Y);
            break;
        case 1: // TopRight
            TopLeft = FVector2D(ViewportSize.X - Size.X - MinimapOffset.X, MinimapOffset.Y);
            break;
        case 2: // BottomLeft
            TopLeft = FVector2D(MinimapOffset.X, ViewportSize.Y - Size.Y - MinimapOffset.Y);
            break;
        case 3: // BottomRight
        default:
            TopLeft = FVector2D(ViewportSize.X - Size.X - MinimapOffset.X, ViewportSize.Y - Size.Y - MinimapOffset.Y);
            break;
    }
    return TopLeft;
}

FVector2D UPlayerHUDWidget::GetMinimapTopLeftAbsolute() const
{
    const FGeometry& Geo = GetCachedGeometry();
    if (!Geo.GetLocalSize().IsNearlyZero())
    {
        FVector2D LocalTopLeft = GetMinimapTopLeftLocal(Geo);
        return Geo.LocalToAbsolute(LocalTopLeft);
    }
    return GetMinimapTopLeftFromViewport();
}

bool UPlayerHUDWidget::HandleMinimapClick(const FVector2D& ScreenPosition, bool bIsRightClick, bool bCtrlHeld, bool bAltHeld)
{
    const FGeometry& Geo = GetCachedGeometry();
    if (Geo.GetLocalSize().IsNearlyZero())
    {
        return false;
    }
    
    // Convert screen position to world location
    FVector WorldHit;
    FVector2D MinimapLocalPos;
    bool bInsideMap = ScreenPositionToWorld(Geo, ScreenPosition, WorldHit, &MinimapLocalPos);
    UE_LOG(LogCoding, Display, TEXT("HandleMinimapClick: GeoSize=(%.1f,%.1f) TopLeft=(%.1f,%.1f) Screen=(%.1f,%.1f) MinimapLocal=(%.1f,%.1f) Inside=%d"),
        Geo.GetLocalSize().X, Geo.GetLocalSize().Y, GetMinimapTopLeftLocal(Geo).X, GetMinimapTopLeftLocal(Geo).Y, ScreenPosition.X, ScreenPosition.Y, MinimapLocalPos.X, MinimapLocalPos.Y, bInsideMap ? 1 : 0);
    
    if (!bInsideMap)
    {
        return false;
    }
    
    UE_LOG(LogCoding, Display, TEXT("PlayerHUDWidget::HandleMinimapClick - Screen=(%.0f,%.0f) World=%s RightClick=%d Ctrl=%d Alt=%d"),
        ScreenPosition.X, ScreenPosition.Y, *WorldHit.ToString(), bIsRightClick ? 1 : 0, bCtrlHeld ? 1 : 0, bAltHeld ? 1 : 0);
    
    AMOBAPlayerController* MPC = Cast<AMOBAPlayerController>(GetOwningPlayer());
    
    if (bIsRightClick)
    {
        // Right-click on minimap: move pawn to that location
        if (MPC)
        {
            MPC->Server_RequestMoveTo(WorldHit);
            MPC->Server_SpawnCursorEffect(WorldHit);
        }
        return true;
    }
    
    // Left-click handling
    bool bCameraUnlocked = false;
    if (MPC)
    {
        bCameraUnlocked = !MPC->bCameraLocked;
    }
    
    // Ctrl+Click or Alt+Click: open ping wheel
    if (bCtrlHeld || bAltHeld)
    {
        UE_LOG(LogCoding, Display, TEXT("PlayerHUDWidget: Minimap Ctrl/Alt+left-click - opening ping wheel at world %s"), *WorldHit.ToString());
        if (PingWheelClass)
        {
            ShowPingWheelAtScreenLocation(ScreenPosition, WorldHit);
        }
        else if (MPC)
        {
            MPC->Server_RequestPing(WorldHit, EMapPingType::Ping_OnMyWay);
        }
        return true;
    }
    
    // Normal left-click: move camera if unlocked
    if (bCameraUnlocked && MPC)
    {
        UE_LOG(LogCoding, Display, TEXT("PlayerHUDWidget: Minimap left-click with camera unlocked - moving camera to %s"), *WorldHit.ToString());
        MPC->MoveCameraToWorldLocation(WorldHit);
        return true;
    }
    
    // Camera is locked and no modifier - open ping wheel as convenience
    UE_LOG(LogCoding, Display, TEXT("PlayerHUDWidget: Minimap left-click with camera locked - opening ping wheel at world %s"), *WorldHit.ToString());
    if (PingWheelClass)
    {
        ShowPingWheelAtScreenLocation(ScreenPosition, WorldHit);
    }
    else if (MPC)
    {
        MPC->Server_RequestPing(WorldHit, EMapPingType::Ping_OnMyWay);
    }
    return true;
}

void UPlayerHUDWidget::SetMinimapPath(const TArray<FVector>& PathPoints)
{
    NavPathPoints = PathPoints;
}

void UPlayerHUDWidget::AddDestructionMark(const FVector& WorldLocation)
{
    DestructionMarks.Add(WorldLocation);
}

void UPlayerHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    // Update cached ping actors (cheap: find all pings each tick)
    CachedPings.Empty();
    if (UWorld* W = GetWorld())
    {
        TArray<AActor*> Found;
        UGameplayStatics::GetAllActorsOfClass(W, AMapPing::StaticClass(), Found);
        for (AActor* A : Found)
        {
            if (AMapPing* MP = Cast<AMapPing>(A))
            {
                CachedPings.Add(MP);
            }
        }
    }

    // Collect minimap icons from registered components (faster than scanning all actors)
    // Apply FOW visibility: visible enemies show normally, explored-only show as ghosts
    CachedIcons.Empty();

    // Clean up expired last-known positions
    if (LastKnownPositionExpirySeconds > 0.f)
    {
        const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
        TArray<TWeakObjectPtr<AActor>> ToRemove;
        for (auto& Pair : LastSeenTimes)
        {
            if (!Pair.Key.IsValid() || (Now - Pair.Value) > LastKnownPositionExpirySeconds)
            {
                ToRemove.Add(Pair.Key);
            }
        }
        for (auto& Key : ToRemove)
        {
            LastSeenTimes.Remove(Key);
            LastKnownPositions.Remove(Key);
        }
    }

    // Track which actors we've added icons for (to add ghost icons for non-visible ones at end)
    TSet<TWeakObjectPtr<AActor>> AddedActors;

    for (TWeakObjectPtr<UMinimapComponent>& WeakComp : RegisteredMinimapComponents)
    {
        if (!WeakComp.IsValid()) continue;
        UMinimapComponent* MC = WeakComp.Get();
        if (!MC->bShowOnMinimap) continue;
        AActor* Owner = MC->GetOwner();
        if (!Owner) continue;

        const FVector ActorLoc = Owner->GetActorLocation();
        const int32 OwnerTeamID = MC->TeamID;
        const bool bIsEnemy = (OwnerTeamID != LocalPlayerTeamID);
        
        // Towers (and structures using Icon_Tower or Icon_Object) are always visible on minimap
        // Only hide enemy minions and heroes in fog
        const bool bShouldApplyFogHiding = (MC->IconType == EMinimapIconType::Icon_Minion || MC->IconType == EMinimapIconType::Icon_Hero);

        // Default: not ghost, visible
        bool bShouldShow = true;
        bool bAsGhost = false;

        // Check FOW visibility for enemy minions and heroes only (towers/objects always visible)
        if (bIsEnemy && FogManager && bShouldApplyFogHiding)
        {
            const bool bVisible = FogManager->IsLocationVisibleToTeam(ActorLoc, LocalPlayerTeamID);
            const bool bExplored = FogManager->IsLocationExploredByTeam(ActorLoc, LocalPlayerTeamID);

            if (bVisible)
            {
                // Enemy is visible -> show normally and update last known position
                bShouldShow = true;
                bAsGhost = false;
                LastKnownPositions.Add(Owner, ActorLoc);
                LastSeenTimes.Add(Owner, GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0);
                if (bEnableMinimapLogging && bMinimapVerboseLogging)
                {
                    UE_LOG(LogCoding, Log, TEXT("PlayerHUDWidget: Visible enemy %s Team=%d Position=(%.1f,%.1f,%.1f) -> updating LastKnown"), *Owner->GetName(), OwnerTeamID, ActorLoc.X, ActorLoc.Y, ActorLoc.Z);
                }
            }
            else if (bShowExploredAsGhost && bExplored)
            {
                // Enemy is in explored area but not visible -> show as ghost at last known position
                if (FVector* LastPos = LastKnownPositions.Find(Owner))
                {
                    bShouldShow = true;
                    bAsGhost = true;
                    // Use last known position, not current
                    FMinimapIcon I;
                    I.WorldLocation = *LastPos;
                    I.OwnerActor = Owner;
                    I.Normalized = WorldToMinimapNormalized(*LastPos);
                    I.Color = MC->IconColor;
                    I.Size = MC->IconSize > 0 ? MC->IconSize : 6.0f;
                    I.TeamID = OwnerTeamID;
                    I.bGhost = true;
                    I.IconType = MC->IconType;
                    CachedIcons.Add(I);
                    AddedActors.Add(Owner);
                    // Log the ghost addition with timestamp info if verbose logging is enabled
                    if (bEnableMinimapLogging && bMinimapVerboseLogging)
                    {
                        double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
                        double LastSeen = LastSeenTimes.Contains(Owner) ? LastSeenTimes[Owner] : 0.0;
                        double Age = (LastSeen > 0.0) ? (Now - LastSeen) : -1.0;
                        UE_LOG(LogCoding, Log, TEXT("PlayerHUDWidget: Ghost icon added for %s Team=%d LastKnown=(%.1f,%.1f,%.1f) Age=%.2f"), *Owner->GetName(), OwnerTeamID, LastPos->X, LastPos->Y, LastPos->Z, Age);
                    }
                    continue; // Skip normal add below
                }
                else
                {
                    // No last known position -> don't show at all
                    bShouldShow = false;
                    UE_LOG(LogCoding, Verbose, TEXT("PlayerHUDWidget: enemy %s is explored but has no LastKnownPos"), *Owner->GetName());
                }
            }
            else
            {
                // Not visible, not explored (or ghost disabled) -> don't show
                bShouldShow = false;
                UE_LOG(LogCoding, Verbose, TEXT("PlayerHUDWidget: enemy %s not visible and not explored - skipping"), *Owner->GetName());
            }
        }

        if (bShouldShow)
        {
            FMinimapIcon I;
            I.WorldLocation = ActorLoc;
            I.OwnerActor = Owner;
            I.Normalized = WorldToMinimapNormalized(I.WorldLocation);
            I.Color = MC->IconColor;
            I.Size = MC->IconSize > 0 ? MC->IconSize : 6.0f;
            I.TeamID = OwnerTeamID;
            I.bGhost = bAsGhost;
            I.IconType = MC->IconType;
            CachedIcons.Add(I);
            AddedActors.Add(Owner);
        }
    }

    // Auto-detect WorldBoundsHalfSize based on registered minimap component locations if requested
    if (bAutoDetectWorldBounds && !bWorldBoundsAutoDetected && RegisteredMinimapComponents.Num() > 0)
    {
        float MinX = FLT_MAX, MinY = FLT_MAX, MaxX = -FLT_MAX, MaxY = -FLT_MAX;
        bool bFoundAny = false;
        for (TWeakObjectPtr<UMinimapComponent>& WeakComp : RegisteredMinimapComponents)
        {
            if (!WeakComp.IsValid()) continue;
            UMinimapComponent* MC = WeakComp.Get();
            if (!MC->bShowOnMinimap) continue;
            AActor* Owner = MC->GetOwner();
            if (!Owner) continue;
            FVector L = Owner->GetActorLocation();
            float X = L.X; float Y = L.Y;
            MinX = FMath::Min(MinX, X);
            MinY = FMath::Min(MinY, Y);
            MaxX = FMath::Max(MaxX, X);
            MaxY = FMath::Max(MaxY, Y);
            bFoundAny = true;
        }
        if (bFoundAny && MaxX > MinX && MaxY > MinY)
        {
            float HalfX = ((MaxX - MinX) * 0.5f) + AutoDetectWorldBoundsMargin;
            float HalfY = ((MaxY - MinY) * 0.5f) + AutoDetectWorldBoundsMargin;
            WorldBoundsHalfSize = FVector2D(HalfX, HalfY);
            // Compute center as midpoint
            WorldCenter = FVector2D((MaxX + MinX) * 0.5f, (MaxY + MinY) * 0.5f);
            bWorldBoundsAutoDetected = true;
            UE_LOG(LogCoding, Log, TEXT("PlayerHUDWidget: Auto-detected WorldBoundsHalfSize=(%.1f,%.1f) from %d components (min=(%.1f,%.1f) max=(%.1f,%.1f))"), WorldBoundsHalfSize.X, WorldBoundsHalfSize.Y, RegisteredMinimapComponents.Num(), MinX, MinY, MaxX, MaxY);
        }
    }

    // Update debug text (number of icons)
    if (Debug_IconCount)
    {
        Debug_IconCount->SetText(FText::AsNumber(CachedIcons.Num()));
    }

    // Throttled logging: avoid spamming the Output log every tick. Use compact logging by default and
    // allow verbose per-icon logging only when `bMinimapVerboseLogging` is set.
    if (bEnableMinimapLogging)
    {
        float Now = 0.0f;
        if (GetWorld()) Now = GetWorld()->GetTimeSeconds();

        if (Now - LastIconLogTime >= MinimapLogInterval)
        {
            LastIconLogTime = Now;

            if (bMinimapVerboseLogging)
            {
                const int32 LogCount = FMath::Min(6, CachedIcons.Num());
                for (int32 i = 0; i < LogCount; ++i)
                {
                    const FMinimapIcon& MI = CachedIcons[i];
                    FString Name = MI.OwnerActor.IsValid() ? MI.OwnerActor->GetName() : TEXT("(no-owner)");
                    UE_LOG(LogCoding, Display, TEXT("MinimapIcon[%d] Owner=%s World=%s Norm=(%.3f,%.3f) Ghost=%d"), i, *Name, *MI.WorldLocation.ToString(), MI.Normalized.X, MI.Normalized.Y, MI.bGhost ? 1 : 0);
                }
            }
            else
            {
                // Compact one-line summary to keep logs readable
                int32 Total = CachedIcons.Num();
                FString SampleNames;
                const int32 Samples = FMath::Min(3, Total);
                for (int32 i = 0; i < Samples; ++i)
                {
                    if (CachedIcons[i].OwnerActor.IsValid())
                    {
                        SampleNames += CachedIcons[i].OwnerActor->GetName();
                        if (i < Samples - 1) SampleNames += TEXT(",");
                    }
                }
                UE_LOG(LogCoding, Log, TEXT("MinimapIcons: Count=%d Samples=%s"), Total, *SampleNames);
            }
        }
    }

    // --- Dynamic Nav Path Update (throttled) ---
    PathUpdateAccumulator += InDeltaTime;
    // Only compute while we have a target click recorded
    if (bHasLastClick)
    {
        if (APlayerController* PC = GetOwningPlayer())
        {
            if (APawn* Pawn = PC->GetPawn())
            {
                const FVector PawnLoc = Pawn->GetActorLocation();
                const bool bTargetChanged = !LastPathTarget.Equals(LastClickWorld, 1.0f);
                const float PawnMoved = FVector::DistSquared(PawnLoc, LastPawnLocation);
                const bool bPawnMovedEnough = PawnMoved > FMath::Square(PathRecomputeDistanceThreshold);

                if (bTargetChanged || bPawnMovedEnough || PathUpdateAccumulator >= MinimapPathUpdateInterval)
                {
                    // Recompute nav path from pawn to last click target
                    LastPathTarget = LastClickWorld;
                    LastPawnLocation = PawnLoc;
                    PathUpdateAccumulator = 0.0;

                    if (UWorld* W = GetWorld())
                    {
                        UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(W, PawnLoc, LastPathTarget, Pawn);
                        if (Path && Path->PathPoints.Num() > 0)
                        {
                            // Update displayed minimap path (copy points)
                            SetMinimapPath(Path->PathPoints);
                        }
                        else
                        {
                            // Clear path if none
                            NavPathPoints.Empty();
                        }
                    }
                }
                // If we're close to the target, clear the path to avoid lingering lines
                const float CloseDist = 120.0f;
                if (FVector::DistSquared(PawnLoc, LastClickWorld) <= FMath::Square(CloseDist))
                {
                    NavPathPoints.Empty();
                }
            }
        }
    }
}

void UPlayerHUDWidget::SetLastClickDebug(const FVector2D& Normalized, const FVector& WorldLocation)
{
    LastClickNormalized = Normalized;
    LastClickWorld = WorldLocation;
    bHasLastClick = true;
}

void UPlayerHUDWidget::SetMinimapInversion(bool InvertX, bool InvertY, bool bSwapXY)
{
    bInvertMinimapX = InvertX;
    bInvertMinimapY = InvertY;
    bSwapMinimapXY = bSwapXY;
    UE_LOG(LogCoding, Log, TEXT("PlayerHUDWidget: SetMinimapInversion -> X=%d Y=%d Swap=%d"), bInvertMinimapX?1:0, bInvertMinimapY?1:0, bSwapMinimapXY?1:0);
}

void UPlayerHUDWidget::ToggleSwapMinimapXY()
{
    bSwapMinimapXY = !bSwapMinimapXY;
    UE_LOG(LogCoding, Log, TEXT("PlayerHUDWidget: ToggleSwapMinimapXY -> Swap=%d"), bSwapMinimapXY?1:0);
}

void UPlayerHUDWidget::ToggleInvertMinimapX()
{
    bInvertMinimapX = !bInvertMinimapX;
    UE_LOG(LogCoding, Log, TEXT("PlayerHUDWidget: ToggleInvertMinimapX -> InvertX=%d"), bInvertMinimapX?1:0);
}

void UPlayerHUDWidget::ToggleInvertMinimapY()
{
    bInvertMinimapY = !bInvertMinimapY;
    UE_LOG(LogCoding, Log, TEXT("PlayerHUDWidget: ToggleInvertMinimapY -> InvertY=%d"), bInvertMinimapY?1:0);
}

void UPlayerHUDWidget::SetMinimapLogging(bool bEnable, bool bVerbose, float IntervalSeconds)
{
    bEnableMinimapLogging = bEnable;
    bMinimapVerboseLogging = bVerbose;
    MinimapLogInterval = FMath::Max(0.01f, IntervalSeconds);
    UE_LOG(LogCoding, Log, TEXT("PlayerHUDWidget: SetMinimapLogging -> Enable=%d Verbose=%d Interval=%.2f"), bEnableMinimapLogging?1:0, bMinimapVerboseLogging?1:0, MinimapLogInterval);
}

bool UPlayerHUDWidget::RegisterMinimapComponent(UMinimapComponent* Comp)
{
    if (!Comp) return false;
    int32 OldNum = RegisteredMinimapComponents.Num();
    RegisteredMinimapComponents.AddUnique(Comp);
    return RegisteredMinimapComponents.Num() > OldNum;
}

void UPlayerHUDWidget::UnregisterMinimapComponent(UMinimapComponent* Comp)
{
    if (!Comp) return;
    RegisteredMinimapComponents.RemoveSingleSwap(Comp);
}

FReply UPlayerHUDWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    // Entry log for debugging click handling
    FVector2D EntryScreen = InMouseEvent.GetScreenSpacePosition();
    const FKey EntryButton = InMouseEvent.GetEffectingButton();
    UE_LOG(LogCoding, Display, TEXT("PlayerHUDWidget: NativeOnMouseButtonDown called Screen=(%.0f,%.0f) Button=%s Ctrl=%d"), EntryScreen.X, EntryScreen.Y, *EntryButton.ToString(), InMouseEvent.IsControlDown() ? 1 : 0);

    // Compute minimap top-left using helper to ensure consistent mapping between paint and clicks
    FVector2D Size = MinimapSize;
    if (Size.X <= 0 || Size.Y <= 0)
    {
        return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
    }
    FVector2D MinimapTopLeft = GetMinimapTopLeftLocal(InGeometry);

    // Screen position of the click
    FVector2D ScreenPos = InMouseEvent.GetScreenSpacePosition();

    // If Ctrl is held, show ping wheel / send a ping; otherwise only request move (no automatic ping)
    bool bCtrl = InMouseEvent.IsControlDown();
    // Fallback: if slate/modifier state or player controller input indicates control pressed, honor it too
    if (!bCtrl)
    {
        if (FSlateApplication::IsInitialized())
        {
            bCtrl = FSlateApplication::Get().GetModifierKeys().IsControlDown();
        }
        if (!bCtrl)
        {
            if (APlayerController* PC = GetOwningPlayer())
            {
                bCtrl = PC->IsInputKeyDown(EKeys::LeftControl) || PC->IsInputKeyDown(EKeys::RightControl);
            }
        }
    }
    // Convert the screen pos to local coords and to world-space (if click falls on the minimap) and get minimap-local coords
    FVector2D LocalPos = InGeometry.AbsoluteToLocal(ScreenPos);
    FVector2D MinimapLocalPos;
    FVector WorldHit = FVector::ZeroVector;
    bool bInsideMap = ScreenPositionToWorld(InGeometry, ScreenPos, WorldHit, &MinimapLocalPos);

    const FKey Button = InMouseEvent.GetEffectingButton();

    // Check if camera is unlocked (for left-click camera move on minimap)
    bool bCameraUnlocked = false;
    AMOBAPlayerController* MPC = nullptr;
    if (APlayerController* PC = GetOwningPlayer())
    {
        MPC = Cast<AMOBAPlayerController>(PC);
        if (MPC)
        {
            bCameraUnlocked = !MPC->bCameraLocked;
        }
    }

    if (bInsideMap)
    {
        // Left-click on minimap:
        // - Normal click with camera unlocked: move camera to that location
        // - Alt+click or Ctrl+click: open ping wheel at that location
        // - Normal click with camera locked: open ping wheel (for convenience)
        // Right-click on minimap: always move/attack
        if (Button == EKeys::LeftMouseButton)
        {
            if (Size.X <= 0 || Size.Y <= 0)
            {
                return FReply::Handled();
            }
            // WorldHit computed by ScreenPositionToWorld helper when inside minimap

            // Check if Alt is held (for ping)
            bool bAlt = InMouseEvent.IsAltDown();
            if (!bAlt && FSlateApplication::IsInitialized())
            {
                bAlt = FSlateApplication::Get().GetModifierKeys().IsAltDown();
            }
            if (!bAlt && MPC)
            {
                bAlt = MPC->IsInputKeyDown(EKeys::LeftAlt) || MPC->IsInputKeyDown(EKeys::RightAlt);
            }

            // Ctrl+Click or Alt+Click: open ping wheel
            if (bCtrl || bAlt)
            {
                UE_LOG(LogCoding, Display, TEXT("PlayerHUDWidget: Minimap Ctrl/Alt+left-click - opening ping wheel at world %s"), *WorldHit.ToString());
                if (PingWheelClass)
                {
                    ShowPingWheelAtScreenLocation(ScreenPos, WorldHit);
                }
                else if (MPC)
                {
                    MPC->Server_RequestPing(WorldHit, EMapPingType::Ping_OnMyWay);
                }
                return FReply::Handled();
            }

            // Normal left-click: move camera if unlocked
            if (bCameraUnlocked && MPC)
            {
                UE_LOG(LogCoding, Display, TEXT("PlayerHUDWidget: Minimap left-click with camera unlocked - moving camera to %s"), *WorldHit.ToString());
                MPC->MoveCameraToWorldLocation(WorldHit);
                return FReply::Handled();
            }

            // Camera is locked and no modifier - open ping wheel as convenience
            UE_LOG(LogCoding, Display, TEXT("PlayerHUDWidget: Minimap left-click with camera locked - opening ping wheel at world %s"), *WorldHit.ToString());
            if (PingWheelClass)
            {
                ShowPingWheelAtScreenLocation(ScreenPos, WorldHit);
            }
            else if (MPC)
            {
                MPC->Server_RequestPing(WorldHit, EMapPingType::Ping_OnMyWay);
            }
            return FReply::Handled();
        }
        
        // Right-click handling below
        if (Button != EKeys::RightMouseButton)
        {
            return FReply::Handled();
        }

        if (Size.X <= 0 || Size.Y <= 0)
        {
            return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
        }

        // Normalized coordinates (0..1) based on minimap-local position
        FVector2D Normalized(MinimapLocalPos.X / Size.X, MinimapLocalPos.Y / Size.Y);

            UE_LOG(LogCoding, Verbose, TEXT("PlayerHUDWidget: Minimap RightClick Ctrl=%d Normalized=(%.3f,%.3f) LocalPos=(%.1f,%.1f) Size=(%.1f,%.1f) WorldBoundsHalfSize=(%.1f,%.1f)"), bCtrl ? 1 : 0, Normalized.X, Normalized.Y, LocalPos.X, LocalPos.Y, Size.X, Size.Y, WorldBoundsHalfSize.X, WorldBoundsHalfSize.Y);

        // WorldHit already calculated by ScreenPositionToWorld if this is inside minimap.
    }
    else
    {
        // Click was outside minimap: only handle RightMouseButton here as well
        // Consume non-right clicks to avoid letting them fall through and trigger movement.
        if (Button != EKeys::RightMouseButton)
        {
            return FReply::Handled();
        }

        // Deproject screen pos to world and intersect with Z=0 plane
        if (APlayerController* PC = GetOwningPlayer())
        {
            FVector WorldOrigin, WorldDir;
            if (PC->DeprojectScreenPositionToWorld(ScreenPos.X, ScreenPos.Y, WorldOrigin, WorldDir))
            {
                if (!FMath::IsNearlyZero(WorldDir.Z))
                {
                    float T = (0.0f - WorldOrigin.Z) / WorldDir.Z;
                    if (T > 0.0f)
                    {
                        WorldHit = WorldOrigin + WorldDir * T;
                    }
                    else
                    {
                        WorldHit = WorldOrigin + WorldDir * 10000.0f;
                    }
                }
                else
                {
                    WorldHit = WorldOrigin + WorldDir * 10000.0f;
                }
            }
            else
            {
                // Fallback: can't deproject, just ignore
                return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
            }
        }
        else
        {
            return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
        }
    }

    APlayerController* PC = GetOwningPlayer();
    if (PC)
    {
        MPC = Cast<AMOBAPlayerController>(PC);
        if (MPC)
        {
            UE_LOG(LogCoding, Display, TEXT("PlayerHUDWidget: MPC found, bCtrl=%d, Button=%s, bInsideMap=%d"), bCtrl ? 1 : 0, *Button.ToString(), bInsideMap ? 1 : 0);
            if (bCtrl)
            {
                UE_LOG(LogCoding, Display, TEXT("PlayerHUDWidget: Ctrl pressed - opening ping wheel"));
                // Ctrl+Click -> show ping wheel if available, otherwise send a default ping
                if (PingWheelClass)
                {
                    // Show ping wheel centered on the click using the actual screen pos and world location
                    ShowPingWheelAtScreenLocation(ScreenPos, WorldHit);
                }
                else
                {
                    // fallback: send a simple 'OnMyWay' ping
                    MPC->Server_RequestPing(WorldHit, EMapPingType::Ping_OnMyWay);
                }
            }
            else
            {
                UE_LOG(LogCoding, Display, TEXT("PlayerHUDWidget: Not Ctrl - processing move request"));
                // Not Ctrl: Right click -> request move or camera move
                UE_LOG(LogCoding, Display, TEXT("PlayerHUDWidget: Requesting move to %s (Button=%s, InsideMap=%d) Norm=(%.3f,%.3f) Local=(%.1f,%.1f) Size=(%.1f,%.1f) WorldBounds=(%.1f,%.1f)"), *WorldHit.ToString(), *Button.ToString(), bInsideMap ? 1 : 0, LocalPos.X / Size.X, LocalPos.Y / Size.Y, LocalPos.X, LocalPos.Y, Size.X, Size.Y, WorldBoundsHalfSize.X, WorldBoundsHalfSize.Y);
                
                // If this was a right-click inside the minimap: request pawn movement (server) and
                // also move the camera if it's unlocked. We want right-click on minimap to move the
                // player to that world location (server-authoritative), not only move the camera.
                if (bInsideMap && Button == EKeys::RightMouseButton)
                {
                    UE_LOG(LogCoding, Display, TEXT("PlayerHUDWidget: Inside minimap right-click - requesting pawn move to %s"), *WorldHit.ToString());

                    // Always request server-side movement so the pawn moves to the minimap location.
                    MPC->Server_RequestMoveTo(WorldHit);
                    // Spawn cursor effect via server RPC as well (will multicast to clients).
                    MPC->Server_SpawnCursorEffect(WorldHit);

                    // Do not move the camera on minimap right-click; right-click should request pawn move only
                    UE_LOG(LogCoding, Verbose, TEXT("PlayerHUDWidget: Right-click on minimap - camera not moved (only pawn move requested)."));
                }
                else
                {
                    UE_LOG(LogCoding, Display, TEXT("PlayerHUDWidget: Outside minimap or not right-click - calling Server_RequestMoveTo"));
                    // Default behavior: request movement for the pawn (right-click outside minimap)
                    UE_LOG(LogCoding, Display, TEXT("PlayerHUDWidget: Calling Server_RequestMoveTo to %s and spawning cursor effect"), *WorldHit.ToString());
                    MPC->Server_RequestMoveTo(WorldHit);
                    MPC->Server_SpawnCursorEffect(WorldHit);
                }
            }
        }
        else
        {
            UE_LOG(LogCoding, Error, TEXT("PlayerHUDWidget: MPC cast failed - PC is %s"), *PC->GetName());
        }
    }
    else
    {
        UE_LOG(LogCoding, Error, TEXT("PlayerHUDWidget: GetOwningPlayer() returned null"));
    }

    UE_LOG(LogCoding, Display, TEXT("Map click (screen %.0f,%.0f) mapped to world %s"), ScreenPos.X, ScreenPos.Y, *WorldHit.ToString());

    // Store debug info so we can draw it in NativePaint
    FVector2D ClickNorm;
    if (bInsideMap)
    {
        // recompute normalized from local position/size (safe even if Normalized symbol was local)
        ClickNorm = FVector2D(LocalPos.X / Size.X, LocalPos.Y / Size.Y);
    }
    else
    {
        ClickNorm = WorldToMinimapNormalized(WorldHit);
    }
    SetLastClickDebug(ClickNorm, WorldHit);

    // For right-clicks outside minimap, let the controller handle the movement instead
    if (!bInsideMap && Button == EKeys::RightMouseButton && !bCtrl)
    {
        UE_LOG(LogCoding, Display, TEXT("PlayerHUDWidget: Right-click outside minimap - letting controller handle movement"));
        return FReply::Unhandled(); // Let the controller's OnRightClickTriggered handle it
    }

    return FReply::Handled();
}

int32 UPlayerHUDWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    int32 RetLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

    // Calculate minimap position based on exposed properties (no MapImage dependency)
    FVector2D WidgetSize = AllottedGeometry.GetLocalSize();
    FVector2D Size = MinimapSize;
    
    // Skip if size is invalid
    if (Size.X <= 0 || Size.Y <= 0 || WidgetSize.X <= 0 || WidgetSize.Y <= 0)
    {
        return RetLayer;
    }
    
    // Calculate minimap top-left position based on anchor corner and offset
    // MinimapAnchorCorner: 0 = TopLeft, 1 = TopRight, 2 = BottomLeft, 3 = BottomRight
    // Compute minimap top-left using helper
    FVector2D MinimapTopLeft = GetMinimapTopLeftLocal(AllottedGeometry);
    
    // Create a paint geometry for the minimap area
    FPaintGeometry MinimapPaintGeom = AllottedGeometry.ToPaintGeometry(MinimapTopLeft, Size);
    
    // Create a virtual "MapGeometry" for icon/fog drawing calculations
    // This is a child geometry offset to the minimap position
    FGeometry MapGeometry = AllottedGeometry.MakeChild(Size, FSlateLayoutTransform(MinimapTopLeft));

    // Debug: log minimap geometry if requested
    if (bShowMinimapDebugLabels)
    {
        UE_LOG(LogCoding, Display, TEXT("PlayerHUDWidget: Minimap TopLeft=(%.1f,%.1f) Size=(%.1f,%.1f) AnchorCorner=%d"), 
            MinimapTopLeft.X, MinimapTopLeft.Y, Size.X, Size.Y, MinimapAnchorCorner);
    }

    const float MarkerSize = 8.0f;

    // Draw the minimap background texture
    if (MinimapTexture)
    {
        FSlateBrush MapBrush;
        MapBrush.SetResourceObject(const_cast<UTexture2D*>(MinimapTexture));
        MapBrush.ImageSize = Size;
        MapBrush.DrawAs = ESlateBrushDrawType::Image;
        
        FSlateDrawElement::MakeBox(
            OutDrawElements,
            RetLayer + 1,
            MinimapPaintGeom,
            &MapBrush,
            ESlateDrawEffect::None,
            FLinearColor::White
        );
        RetLayer += 1;
    }

    // Debug: draw clickable area overlay if requested
    if (bShowClickableAreaDebug)
    {
        // Use a semi-transparent color and an outline to make the clickable area visible
        const FLinearColor FillColor(0.0f, 0.5f, 1.0f, 0.15f);
        const FLinearColor BorderColor(0.0f, 0.5f, 1.0f, 0.85f);

        // Draw filled rectangle
        FSlateBrush FillBrush;
        FillBrush.TintColor = FSlateColor(FillColor);
        FillBrush.DrawAs = ESlateBrushDrawType::Box;
        FillBrush.ImageSize = Size;

        FSlateDrawElement::MakeBox(
            OutDrawElements,
            RetLayer + 100,
            MinimapPaintGeom,
            &FillBrush,
            ESlateDrawEffect::None,
            FillColor
        );

        // Border: thin lines around the rectangle
        TArray<FVector2f> BorderPoints;
        BorderPoints.Add(FVector2f(MinimapTopLeft.X, MinimapTopLeft.Y));
        BorderPoints.Add(FVector2f(MinimapTopLeft.X + Size.X, MinimapTopLeft.Y));
        BorderPoints.Add(FVector2f(MinimapTopLeft.X + Size.X, MinimapTopLeft.Y + Size.Y));
        BorderPoints.Add(FVector2f(MinimapTopLeft.X, MinimapTopLeft.Y + Size.Y));
        BorderPoints.Add(FVector2f(MinimapTopLeft.X, MinimapTopLeft.Y));

        FSlateDrawElement::MakeLines(
            OutDrawElements,
            RetLayer + 101,
            AllottedGeometry.ToPaintGeometry(FVector2D::ZeroVector, AllottedGeometry.GetLocalSize()),
            BorderPoints,
            ESlateDrawEffect::None,
            BorderColor,
            true,
            2.0f
        );
        RetLayer += 2;
    }

    // Debug: force-draw helpers (temporary)
    const int32 DebugLayerOffset = 1000; // draw well above normal UI for visibility while debugging
    const bool bForceDebugDraw = (bShowMinimapDebugOutline || bShowMinimapDebugLabels);

    // ===== Draw FOW Overlay on Minimap (Texture-based smooth) =====
    // Prefer drawing a supplied fog texture (or render target) for best smoothness; otherwise fall back to grid blending
    if (bDrawFOWOverlay && FogManager)
    {
        const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
        // Clamp grid size at runtime to reasonable values
        int32 GridSizeLocal = FMath::Clamp(FOWOverlayGridSize, 2, 512);
        const float CellWidth = Size.X / GridSizeLocal;
        const float CellHeight = Size.Y / GridSizeLocal;

        // Attempt to draw a high-resolution fog texture/render target provided by FogManager
        // We use custom vertices to apply the same UV transformations as the minimap icons
        // (swap XY, invert X/Y) so the fog aligns correctly with the map
        if (bUseFogTextureOverlay)
        {
            UTexture* FogTexture = nullptr;
            // Prefer team-specific texture (UTexture2D) if available; else try render target
            FogTexture = FogManager->GetLocalTeamFogTexture();
            if (!FogTexture)
            {
                FogTexture = FogManager->GetFogRenderTarget();
            }

            if (FogTexture)
            {
                // If we prefer grid overlay computation, skip directly to the grid overlay
                if (bPreferGridFOWOverlay)
                {
                    // Skip texture drawing; grid-based overlay will proceed below
                }
                else
                {
                FSlateBrush FogBrush;
                FogBrush.SetResourceObject(FogTexture);
                FogBrush.ImageSize = Size;
                FogBrush.DrawAs = ESlateBrushDrawType::Image;

                FLinearColor Tint = FLinearColor::White;
                Tint.A = FogOverlayTextureOpacity;

                // Calculate transformed UVs to match minimap coordinate system
                // The fog texture uses world coordinates directly, but minimap may have:
                // - bSwapMinimapXY: swap U and V
                // - bInvertMinimapX: flip U
                // - bInvertMinimapY: flip V
                // We need to apply the INVERSE transformations to the UVs

                // Base UV corners: TopLeft(0,0), TopRight(1,0), BottomRight(1,1), BottomLeft(0,1)
                FVector2D UV_TL(0.0f, 0.0f);
                FVector2D UV_TR(1.0f, 0.0f);
                FVector2D UV_BR(1.0f, 1.0f);
                FVector2D UV_BL(0.0f, 1.0f);

                // Apply inversion first (before swap)
                if (bInvertMinimapX)
                {
                    // Flip horizontally: swap left and right U values
                    Swap(UV_TL.X, UV_TR.X);
                    Swap(UV_BL.X, UV_BR.X);
                }
                if (bInvertMinimapY)
                {
                    // Flip vertically: swap top and bottom V values
                    Swap(UV_TL.Y, UV_BL.Y);
                    Swap(UV_TR.Y, UV_BR.Y);
                }

                // Apply XY swap (rotates the texture 90 degrees and flips)
                if (bSwapMinimapXY)
                {
                    // Swap U and V for each corner
                    Swap(UV_TL.X, UV_TL.Y);
                    Swap(UV_TR.X, UV_TR.Y);
                    Swap(UV_BR.X, UV_BR.Y);
                    Swap(UV_BL.X, UV_BL.Y);
                }

                // Optional Y flip for fog texture
                if (bForceFogTextureFlipY)
                {
                    Swap(UV_TL.Y, UV_BL.Y);
                    Swap(UV_TR.Y, UV_BR.Y);
                }

                // Get absolute position/size for vertices
                FVector2D AbsPos = MapGeometry.GetAbsolutePosition();
                FVector2D AbsSize = MapGeometry.GetAbsoluteSize();

                // Build custom vertices (clockwise: TL, TR, BR, BL for two triangles)
                TArray<FSlateVertex> Vertices;
                Vertices.SetNum(4);

                // Colors as FColor
                FColor VertexColor = Tint.ToFColor(true);

                // TopLeft
                Vertices[0].Position = FVector2f(AbsPos.X, AbsPos.Y);
                Vertices[0].TexCoords[0] = UV_TL.X;
                Vertices[0].TexCoords[1] = UV_TL.Y;
                Vertices[0].TexCoords[2] = 0.0f;
                Vertices[0].TexCoords[3] = 0.0f;
                Vertices[0].Color = VertexColor;

                // TopRight
                Vertices[1].Position = FVector2f(AbsPos.X + AbsSize.X, AbsPos.Y);
                Vertices[1].TexCoords[0] = UV_TR.X;
                Vertices[1].TexCoords[1] = UV_TR.Y;
                Vertices[1].TexCoords[2] = 0.0f;
                Vertices[1].TexCoords[3] = 0.0f;
                Vertices[1].Color = VertexColor;

                // BottomRight
                Vertices[2].Position = FVector2f(AbsPos.X + AbsSize.X, AbsPos.Y + AbsSize.Y);
                Vertices[2].TexCoords[0] = UV_BR.X;
                Vertices[2].TexCoords[1] = UV_BR.Y;
                Vertices[2].TexCoords[2] = 0.0f;
                Vertices[2].TexCoords[3] = 0.0f;
                Vertices[2].Color = VertexColor;

                // BottomLeft
                Vertices[3].Position = FVector2f(AbsPos.X, AbsPos.Y + AbsSize.Y);
                Vertices[3].TexCoords[0] = UV_BL.X;
                Vertices[3].TexCoords[1] = UV_BL.Y;
                Vertices[3].TexCoords[2] = 0.0f;
                Vertices[3].TexCoords[3] = 0.0f;
                Vertices[3].Color = VertexColor;

                // Two triangles: (0,1,2) and (0,2,3)
                TArray<SlateIndex> Indices;
                Indices.Add(0);
                Indices.Add(1);
                Indices.Add(2);
                Indices.Add(0);
                Indices.Add(2);
                Indices.Add(3);

                // Get the resource handle from the brush
                const FSlateResourceHandle& ResourceHandle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(FogBrush);

                FSlateDrawElement::MakeCustomVerts(
                    OutDrawElements,
                    RetLayer + 1,
                    ResourceHandle,
                    Vertices,
                    Indices,
                    nullptr, // InstanceData
                    0,       // InstanceOffset
                    0        // NumInstances
                );

                    RetLayer += 1;
                    goto AfterFOWOverlay;
                }
            }
        }

        // Lambda to convert normalized minimap coords to world position
        auto NormToWorld = [this](float NormX, float NormY) -> FVector
        {
            float WX = NormX - 0.5f;
            float WY = 0.5f - NormY;
            if (bSwapMinimapXY) { float Tmp = WX; WX = WY; WY = Tmp; }
            if (bInvertMinimapX) WX = -WX;
            if (bInvertMinimapY) WY = -WY;
            float WorldX = WX * 2.0f * WorldBoundsHalfSize.X + WorldCenter.X;
            float WorldY = WY * 2.0f * WorldBoundsHalfSize.Y + WorldCenter.Y;
            return FVector(WorldX, WorldY, 0.0f);
        };

        // Lambda to get visibility value (0 = hidden, 0.5 = explored, 1 = visible)
        auto GetVisibilityValue = [this](const FVector& WorldPos) -> float
        {
            EFogState State = FogManager->GetFogStateAtLocation(WorldPos, LocalPlayerTeamID);
            if (State == EFogState::Visible) return 1.0f;
            if (State == EFogState::Explored) return 0.5f;
            return 0.0f; // Hidden
        };

        for (int32 GridY = 0; GridY < FOWOverlayGridSize; ++GridY)
        {
            for (int32 GridX = 0; GridX < FOWOverlayGridSize; ++GridX)
            {
                // Multi-sample: check center and surrounding samples for smooth blending
                float NormCenterX = (GridX + 0.5f) / (float)GridSizeLocal;
                float NormCenterY = (GridY + 0.5f) / (float)GridSizeLocal;

                float VisSum = 0.0f;
                int32 SampleCount = 0;

                int32 SmoothSamples = FMath::Clamp(FOWOverlaySmoothSamples, 1, 9);
                if (SmoothSamples <= 1)
                {
                    // Single sample: center only
                    FVector WorldCenterPos = NormToWorld(NormCenterX, NormCenterY);
                    VisSum += GetVisibilityValue(WorldCenterPos);
                    SampleCount += 1;
                }
                else if (SmoothSamples <= 5)
                {
                    // 5-sample: center (weighted) + 4 corners, like previous approach
                    const float SampleOffset = 0.35f / (float)GridSizeLocal; // Slightly inside cell edges
                    FVector WorldCenterPos = NormToWorld(NormCenterX, NormCenterY);
                    VisSum += GetVisibilityValue(WorldCenterPos) * 2.0f;
                    SampleCount += 2;

                    FVector WorldTL = NormToWorld(NormCenterX - SampleOffset, NormCenterY - SampleOffset);
                    FVector WorldTR = NormToWorld(NormCenterX + SampleOffset, NormCenterY - SampleOffset);
                    FVector WorldBL = NormToWorld(NormCenterX - SampleOffset, NormCenterY + SampleOffset);
                    FVector WorldBR = NormToWorld(NormCenterX + SampleOffset, NormCenterY + SampleOffset);
                    VisSum += GetVisibilityValue(WorldTL);
                    VisSum += GetVisibilityValue(WorldTR);
                    VisSum += GetVisibilityValue(WorldBL);
                    VisSum += GetVisibilityValue(WorldBR);
                    SampleCount += 4;
                }
                else
                {
                    // 9-sample: 3x3 grid inside the cell
                    float Step = 1.0f / (float)GridSizeLocal; // normalized step per cell
                    float OffsetScale = (1.0f / 3.0f) * Step; // positions at -1/3, 0, +1/3 of cell
                    for (int32 iy = -1; iy <= 1; ++iy)
                    {
                        for (int32 ix = -1; ix <= 1; ++ix)
                        {
                            float SampleX = NormCenterX + ix * OffsetScale;
                            float SampleY = NormCenterY + iy * OffsetScale;
                            FVector WorldPos = NormToWorld(SampleX, SampleY);
                            VisSum += GetVisibilityValue(WorldPos);
                            SampleCount += 1;
                        }
                    }
                }
                
                // Average visibility (0 = fully hidden, 1 = fully visible)
                float AvgVisibility = VisSum / SampleCount;
                
                // Skip if fully visible
                if (AvgVisibility >= 0.99f)
                {
                    continue;
                }
                
                // Interpolate between hidden and explored colors based on visibility
                // AvgVisibility: 0 = hidden, 0.5 = explored, 1 = visible
                FLinearColor OverlayColor;
                if (AvgVisibility <= 0.5f)
                {
                    // Blend from hidden (0) to explored (0.5)
                    float T = AvgVisibility * 2.0f; // 0 to 1
                    OverlayColor = FMath::Lerp(FOWHiddenColor, FOWExploredColor, T);
                }
                else
                {
                    // Blend from explored (0.5) to visible (1.0 = transparent)
                    float T = (AvgVisibility - 0.5f) * 2.0f; // 0 to 1
                    FLinearColor Transparent = FOWExploredColor;
                    Transparent.A = 0.0f;
                    OverlayColor = FMath::Lerp(FOWExploredColor, Transparent, T);
                }
                
                // Skip if effectively transparent
                if (OverlayColor.A < 0.01f)
                {
                    continue;
                }

                // Calculate pixel position for this cell
                FVector2D TopLeft(GridX * CellWidth, GridY * CellHeight);
                FVector2D CellSize(CellWidth, CellHeight);

                FSlateDrawElement::MakeBox(
                    OutDrawElements,
                    RetLayer + 1,
                    MapGeometry.ToPaintGeometry(TopLeft, CellSize),
                    WhiteBrush,
                    ESlateDrawEffect::None,
                    OverlayColor
                );
            }
        }
        RetLayer += 1;
    }
AfterFOWOverlay:

    // Draw pings as small crosses on the minimap
    for (TWeakObjectPtr<AMapPing> WeakPing : CachedPings)
    {
        if (!WeakPing.IsValid()) continue;
        AMapPing* Ping = WeakPing.Get();
        FVector2D Norm = WorldToMinimapNormalized(Ping->PingLocation);
        FVector2D PixelPos = FVector2D(Norm.X * Size.X, Norm.Y * Size.Y);
        // Clamp PixelPos to minimap bounds to avoid accidental off-by-one outside drawing area
        bool bOutOfBounds = false;
        if (PixelPos.X < 0.0f || PixelPos.Y < 0.0f || PixelPos.X > Size.X || PixelPos.Y > Size.Y)
        {
            bOutOfBounds = true;
            PixelPos.X = FMath::Clamp(PixelPos.X, 0.0f, Size.X);
            PixelPos.Y = FMath::Clamp(PixelPos.Y, 0.0f, Size.Y);
        }

        // Lines for cross
        TArray<FVector2D> LinePoints1;
        LinePoints1.Add(PixelPos + FVector2D(-MarkerSize, -MarkerSize));
        LinePoints1.Add(PixelPos + FVector2D(MarkerSize, MarkerSize));

        TArray<FVector2D> LinePoints2;
        LinePoints2.Add(PixelPos + FVector2D(-MarkerSize, MarkerSize));
        LinePoints2.Add(PixelPos + FVector2D(MarkerSize, -MarkerSize));

        FLinearColor Col = FLinearColor::Yellow;
        switch (Ping->PingType)
        {
            case EMapPingType::Ping_OnMyWay: Col = FLinearColor::Green; break;
            case EMapPingType::Ping_Help: Col = FLinearColor::Red; break;
            case EMapPingType::Ping_Question: Col = FLinearColor::Blue; break;
            default: Col = FLinearColor::Yellow; break;
        }

        const int32 PingLayer = bForceDebugDraw ? (RetLayer + DebugLayerOffset + 1) : (RetLayer + 1);
        FSlateDrawElement::MakeLines(
            OutDrawElements,
            PingLayer,
            MapGeometry.ToPaintGeometry(),
            LinePoints1,
            ESlateDrawEffect::None,
            Col,
            true,
            3.0f
        );

        FSlateDrawElement::MakeLines(
            OutDrawElements,
            PingLayer,
            MapGeometry.ToPaintGeometry(),
            LinePoints2,
            ESlateDrawEffect::None,
            Col,
            true,
            3.0f
        );

        if (!bForceDebugDraw) RetLayer += 1;
    }

    // Draw navigation path (if any) on top
    if (NavPathPoints.Num() >= 2)
    {
        TArray<FVector2D> Points;
        Points.Reserve(NavPathPoints.Num());
        for (const FVector& WP : NavPathPoints)
        {
            FVector2D Norm = WorldToMinimapNormalized(WP);
            Points.Add(FVector2D(Norm.X * Size.X, Norm.Y * Size.Y));
        }

        // Draw polyline
        FLinearColor PathColor = FLinearColor::White;
        FSlateDrawElement::MakeLines(
            OutDrawElements,
            RetLayer + 1,
            MapGeometry.ToPaintGeometry(),
            Points,
            ESlateDrawEffect::None,
            PathColor,
            true,
            2.5f
        );
        RetLayer += 1;
    }

    // Draw minimap icons (allies, minions, towers, champions)
    for (const FMinimapIcon& Icon : CachedIcons)
    {
        // Visibility is already filtered in NativeTick with FOW checks
        // Ghost icons will have reduced alpha

        FLinearColor DrawColor = Icon.Color;
        if (Icon.bGhost)
        {
            // Apply faded alpha for ghost icons (explored but not visible)
            DrawColor.A *= ExploredGhostAlpha;
            // Optional: desaturate ghost icons slightly for visual distinction
            float Luminance = 0.299f * DrawColor.R + 0.587f * DrawColor.G + 0.114f * DrawColor.B;
            DrawColor.R = FMath::Lerp(DrawColor.R, Luminance, 0.5f);
            DrawColor.G = FMath::Lerp(DrawColor.G, Luminance, 0.5f);
            DrawColor.B = FMath::Lerp(DrawColor.B, Luminance, 0.5f);
        }

        FVector2D Norm = Icon.Normalized;
        FVector2D PixelPos = FVector2D(Norm.X * Size.X, Norm.Y * Size.Y);
        float S = Icon.Size;

        // Draw based on IconType
        switch (Icon.IconType)
        {
        case EMinimapIconType::Icon_Minion:
        {
            // Draw a filled circle for minions
            const int32 NumSegments = 12;
            TArray<FVector2D> CirclePoints;
            for (int32 i = 0; i <= NumSegments; ++i)
            {
                float Angle = 2.0f * PI * i / NumSegments;
                CirclePoints.Add(PixelPos + FVector2D(FMath::Cos(Angle) * S, FMath::Sin(Angle) * S));
            }
            // Draw filled circle using lines (approximate with thick lines)
            FSlateDrawElement::MakeLines(
                OutDrawElements,
                RetLayer + 1,
                MapGeometry.ToPaintGeometry(),
                CirclePoints,
                ESlateDrawEffect::None,
                DrawColor,
                true,
                S * 0.8f  // Thick lines to appear filled
            );
            RetLayer += 1;
            break;
        }
        case EMinimapIconType::Icon_Tower:
        {
            // Draw tower: if texture exists use it, otherwise draw a diamond shape
            // Prefer team-specific texture if set, fall back to generic texture
            UTexture2D* TowerTexture = (Icon.TeamID == 0) ? MinimapIcon_Tower_Team0 : MinimapIcon_Tower_Team1;
            if (TowerTexture)
            {
                FSlateBrush TowerBrush;
                TowerBrush.SetResourceObject(TowerTexture);
                TowerBrush.ImageSize = FVector2D(S * 2.5f, S * 2.5f);
                TowerBrush.DrawAs = ESlateBrushDrawType::Image;
                FVector2D BoxSize(S * 2.5f, S * 2.5f);
                FVector2D TopLeft = PixelPos - BoxSize * 0.5f;
                FSlateDrawElement::MakeBox(
                    OutDrawElements,
                    RetLayer + 1,
                    MapGeometry.ToPaintGeometry(TopLeft, BoxSize),
                    &TowerBrush,
                    ESlateDrawEffect::None,
                    DrawColor
                );
            }
            else
            {
                // Diamond shape fallback
                TArray<FVector2D> DiamondPoints;
                DiamondPoints.Add(PixelPos + FVector2D(0, -S * 1.5f));
                DiamondPoints.Add(PixelPos + FVector2D(S * 1.5f, 0));
                DiamondPoints.Add(PixelPos + FVector2D(0, S * 1.5f));
                DiamondPoints.Add(PixelPos + FVector2D(-S * 1.5f, 0));
                DiamondPoints.Add(PixelPos + FVector2D(0, -S * 1.5f));
                FSlateDrawElement::MakeLines(
                    OutDrawElements,
                    RetLayer + 1,
                    MapGeometry.ToPaintGeometry(),
                    DiamondPoints,
                    ESlateDrawEffect::None,
                    DrawColor,
                    true,
                    3.0f
                );
            }
            RetLayer += 1;
            break;
        }
        case EMinimapIconType::Icon_Object:
        {
            // Nexus: larger star or image
            float NexusSize = S * 2.0f;
            // Prefer team-specific texture if set, fall back to generic texture
            UTexture2D* NexusTexture = (Icon.TeamID == 0) ? MinimapIcon_Nexus_Team0 : MinimapIcon_Nexus_Team1;
            if (NexusTexture)
            {
                FSlateBrush NexusBrush;
                NexusBrush.SetResourceObject(NexusTexture);
                NexusBrush.ImageSize = FVector2D(NexusSize * 2.0f, NexusSize * 2.0f);
                NexusBrush.DrawAs = ESlateBrushDrawType::Image;
                FVector2D BoxSize(NexusSize * 2.0f, NexusSize * 2.0f);
                FVector2D TopLeft = PixelPos - BoxSize * 0.5f;
                FSlateDrawElement::MakeBox(
                    OutDrawElements,
                    RetLayer + 1,
                    MapGeometry.ToPaintGeometry(TopLeft, BoxSize),
                    &NexusBrush,
                    ESlateDrawEffect::None,
                    DrawColor
                );
            }
            else
            {
                // Star shape fallback (6-pointed)
                TArray<FVector2D> StarPoints;
                for (int32 i = 0; i <= 12; ++i)
                {
                    float Angle = PI * i / 6.0f;
                    float R = (i % 2 == 0) ? NexusSize : NexusSize * 0.5f;
                    StarPoints.Add(PixelPos + FVector2D(FMath::Cos(Angle) * R, FMath::Sin(Angle) * R));
                }
                FSlateDrawElement::MakeLines(
                    OutDrawElements,
                    RetLayer + 1,
                    MapGeometry.ToPaintGeometry(),
                    StarPoints,
                    ESlateDrawEffect::None,
                    DrawColor,
                    true,
                    2.5f
                );
            }
            RetLayer += 1;
            break;
        }
        case EMinimapIconType::Icon_Hero:
        {
            // Champion: circular portrait with team-colored border
            float ChampSize = S * 2.0f;
            FLinearColor BorderColor = (Icon.TeamID == 0) ? FLinearColor(0.2f, 0.4f, 1.0f, 1.0f) : FLinearColor(1.0f, 0.2f, 0.2f, 1.0f);
            if (Icon.bGhost) BorderColor.A *= ExploredGhostAlpha;

            // Draw border circle first (larger)
            const int32 NumSegments = 16;
            TArray<FVector2D> BorderCircle;
            float BorderRadius = ChampSize + ChampionBorderThickness;
            for (int32 i = 0; i <= NumSegments; ++i)
            {
                float Angle = 2.0f * PI * i / NumSegments;
                BorderCircle.Add(PixelPos + FVector2D(FMath::Cos(Angle) * BorderRadius, FMath::Sin(Angle) * BorderRadius));
            }
            FSlateDrawElement::MakeLines(
                OutDrawElements,
                RetLayer + 1,
                MapGeometry.ToPaintGeometry(),
                BorderCircle,
                ESlateDrawEffect::None,
                BorderColor,
                true,
                ChampionBorderThickness * 2.0f
            );

            // Draw champion portrait or filled circle
            if (MinimapIcon_Champion)
            {
                FSlateBrush ChampBrush;
                ChampBrush.SetResourceObject(MinimapIcon_Champion);
                ChampBrush.ImageSize = FVector2D(ChampSize * 2.0f, ChampSize * 2.0f);
                ChampBrush.DrawAs = ESlateBrushDrawType::Image;
                FVector2D BoxSize(ChampSize * 2.0f, ChampSize * 2.0f);
                FVector2D TopLeft = PixelPos - BoxSize * 0.5f;
                FSlateDrawElement::MakeBox(
                    OutDrawElements,
                    RetLayer + 2,
                    MapGeometry.ToPaintGeometry(TopLeft, BoxSize),
                    &ChampBrush,
                    ESlateDrawEffect::None,
                    DrawColor
                );
            }
            else
            {
                // Filled circle fallback
                TArray<FVector2D> InnerCircle;
                for (int32 i = 0; i <= NumSegments; ++i)
                {
                    float Angle = 2.0f * PI * i / NumSegments;
                    InnerCircle.Add(PixelPos + FVector2D(FMath::Cos(Angle) * ChampSize, FMath::Sin(Angle) * ChampSize));
                }
                FSlateDrawElement::MakeLines(
                    OutDrawElements,
                    RetLayer + 2,
                    MapGeometry.ToPaintGeometry(),
                    InnerCircle,
                    ESlateDrawEffect::None,
                    DrawColor,
                    true,
                    ChampSize * 0.8f
                );
            }
            RetLayer += 2;
            break;
        }
        default:
        {
            // Default: simple filled box (legacy)
            const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
            FVector2D BoxSize(2.0f * S, 2.0f * S);
            FVector2D TopLeft = PixelPos - BoxSize * 0.5f;
            FSlateDrawElement::MakeBox(
                OutDrawElements,
                RetLayer + 1,
                MapGeometry.ToPaintGeometry(TopLeft, BoxSize),
                WhiteBrush,
                ESlateDrawEffect::None,
                DrawColor
            );
            RetLayer += 1;
            break;
        }
        }
    }

    // Debug: draw a visible border around the minimap area so we can see where MapImage is located
    if (bShowMinimapDebugOutline)
    {
        TArray<FVector2D> RectPoints;
        RectPoints.Add(FVector2D(0.0f, 0.0f));
        RectPoints.Add(FVector2D(Size.X, 0.0f));
        RectPoints.Add(FVector2D(Size.X, Size.Y));
        RectPoints.Add(FVector2D(0.0f, Size.Y));
        RectPoints.Add(FVector2D(0.0f, 0.0f));

        FSlateDrawElement::MakeLines(
            OutDrawElements,
            RetLayer + DebugLayerOffset + 3,
            MapGeometry.ToPaintGeometry(),
            RectPoints,
            ESlateDrawEffect::None,
            FLinearColor::White,
            true,
            4.0f
        );
    }

    // Debug: draw small numeric labels next to first few icons to help verify mapping
    const int32 MaxLabels = 8;
    FSlateFontInfo FontInfo = FCoreStyle::Get().GetFontStyle(TEXT("NormalFont"));
    int32 LabelCount = FMath::Min(CachedIcons.Num(), MaxLabels);
    if (bShowMinimapDebugLabels)
    {
        for (int32 i = 0; i < LabelCount; ++i)
    {
        const FMinimapIcon& Icon = CachedIcons[i];
        FVector2D Norm = Icon.Normalized;
        FVector2D PixelPos = FVector2D(Norm.X * Size.X, Norm.Y * Size.Y);

        FString Label;
        if (Icon.OwnerActor.IsValid())
        {
            AActor* A = Icon.OwnerActor.Get();
            Label = FString::Printf(TEXT("%s\n%s"), *A->GetName(), *Icon.WorldLocation.ToString());
        }
        else
        {
            Label = FString::Printf(TEXT("%d"), i);
        }

            FSlateDrawElement::MakeText(
                OutDrawElements,
                RetLayer + 1,
                MapGeometry.ToOffsetPaintGeometry(PixelPos + FVector2D(6.0f, 0.0f)),
                FText::FromString(Label),
                FontInfo,
                ESlateDrawEffect::None,
                FLinearColor::White
            );
        RetLayer += 1;
        }
    }

    // Draw destruction marks
    if (DestructionMarks.Num() > 0)
    {
        for (const FVector& M : DestructionMarks)
        {
            FVector2D Norm = WorldToMinimapNormalized(M);
            FVector2D PixelPos = FVector2D(Norm.X * Size.X, Norm.Y * Size.Y);
            const float MarkSize = 6.0f;
            TArray<FVector2D> Points;
            Points.Add(PixelPos + FVector2D(-MarkSize, -MarkSize));
            Points.Add(PixelPos + FVector2D(MarkSize, MarkSize));
            Points.Add(PixelPos + FVector2D(-MarkSize, MarkSize));
            Points.Add(PixelPos + FVector2D(MarkSize, -MarkSize));

            FSlateDrawElement::MakeLines(
                OutDrawElements,
                RetLayer + 1,
                MapGeometry.ToPaintGeometry(),
                Points,
                ESlateDrawEffect::None,
                FLinearColor::Black,
                true,
                2.5f
            );
            RetLayer += 1;
        }
    }

    // Draw last click debug info (if any)
    if (bHasLastClick)
    {
        FString ClickInfo = FString::Printf(TEXT("ClickNorm=(%.3f,%.3f) World=%s"), LastClickNormalized.X, LastClickNormalized.Y, *LastClickWorld.ToString());
        FSlateDrawElement::MakeText(
            OutDrawElements,
            RetLayer + 1,
            MapGeometry.ToOffsetPaintGeometry(FVector2D(6.0f, 6.0f)),
            FText::FromString(ClickInfo),
            FontInfo,
            ESlateDrawEffect::None,
            FLinearColor::Yellow
        );
        RetLayer += 1;
    }

    return RetLayer;
}

void UPlayerHUDWidget::BindButtonHandlers()
{
    if (SpellButton_Q)
    {
        SpellButton_Q->OnClicked.AddDynamic(this, &UPlayerHUDWidget::OnSpellQClicked);
    }
    if (SpellButton_W)
    {
        SpellButton_W->OnClicked.AddDynamic(this, &UPlayerHUDWidget::OnSpellWClicked);
    }
    if (SummonerButton_1)
    {
        SummonerButton_1->OnClicked.AddDynamic(this, &UPlayerHUDWidget::OnSummoner1Clicked);
    }
    if (SummonerButton_2)
    {
        SummonerButton_2->OnClicked.AddDynamic(this, &UPlayerHUDWidget::OnSummoner2Clicked);
    }
}

void UPlayerHUDWidget::OnSpellQClicked()
{
    UE_LOG(LogCoding, Display, TEXT("HUD: Spell Q clicked"));
    if (APlayerController* PC = GetOwningPlayer())
    {
        if (APawn* P = PC->GetPawn())
        {
            if (UAbilityComponent* Ab = Cast<UAbilityComponent>(P->GetComponentByClass(UAbilityComponent::StaticClass())))
            {
                Ab->TryUseAbility(0, 0.0f, 0.0f);
            }
        }
    }
}

void UPlayerHUDWidget::OnSpellWClicked()
{
    UE_LOG(LogCoding, Display, TEXT("HUD: Spell W clicked"));
    if (APlayerController* PC = GetOwningPlayer())
    {
        if (APawn* P = PC->GetPawn())
        {
            if (UAbilityComponent* Ab = Cast<UAbilityComponent>(P->GetComponentByClass(UAbilityComponent::StaticClass())))
            {
                Ab->TryUseAbility(1, 0.0f, 0.0f);
            }
        }
    }
}

void UPlayerHUDWidget::OnSummoner1Clicked()
{
    UE_LOG(LogCoding, Display, TEXT("HUD: Summoner 1 clicked"));
}

void UPlayerHUDWidget::OnSummoner2Clicked()
{
    UE_LOG(LogCoding, Display, TEXT("HUD: Summoner 2 clicked"));
}

void UPlayerHUDWidget::ShowPingWheelAtWorldLocation(const FVector& WorldLocation)
{
    if (!PingWheelClass)
    {
        UE_LOG(LogCoding, Warning, TEXT("ShowPingWheelAtWorldLocation: PingWheelClass not set"));
        return;
    }

    // Remove previous
    if (PingWheelInstance)
    {
        PingWheelInstance->RemoveFromParent();
        PingWheelInstance = nullptr;
    }

    // Create and add
    PingWheelInstance = CreateWidget<UUserWidget>(GetOwningPlayer(), PingWheelClass);
    if (!PingWheelInstance)
    {
        UE_LOG(LogCoding, Warning, TEXT("ShowPingWheelAtWorldLocation: Failed to create PingWheelInstance"));
        return;
    }

    if (UPingWheelWidget* PW = Cast<UPingWheelWidget>(PingWheelInstance))
    {
        PW->SetCenterWorldLocation(WorldLocation);
    }

    PingWheelInstance->AddToViewport();

    // Force layout calculation before positioning
    PingWheelInstance->ForceLayoutPrepass();

    // Compute screen pos from cursor position (GetMousePosition returns screen pixels, compatible with SetPositionInViewport)
    FVector2D WheelScreenPos = FVector2D::ZeroVector;
    if (APlayerController* PC = GetOwningPlayer())
    {
        float MouseX = 0.f, MouseY = 0.f;
        PC->GetMousePosition(MouseX, MouseY);
        WheelScreenPos = FVector2D(MouseX, MouseY);
        
        // Fallback if mouse pos is invalid
        if (WheelScreenPos.IsNearlyZero())
        {
            FVector2D Projected;
            if (PC->ProjectWorldLocationToScreen(WorldLocation, Projected, true))
            {
                WheelScreenPos = Projected;
            }
        }
    }

    UE_LOG(LogCoding, Verbose, TEXT("ShowPingWheelAtWorldLocation: WheelScreenPos=(%.1f,%.1f) World=%s"), WheelScreenPos.X, WheelScreenPos.Y, *WorldLocation.ToString());

    // Try to compute the widget size and place the ping wheel centered on the click location
    FVector2D DesiredSize = PingWheelInstance->GetDesiredSize();
    if (DesiredSize.IsNearlyZero())
    {
        // Widget hasn't calculated layout yet - fallback to centered alignment
        PingWheelInstance->SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
        PingWheelInstance->SetPositionInViewport(WheelScreenPos, false);
    }
    else
    {
        // Position so the ping wheel is centered on WheelScreenPos
        FVector2D TopLeft = WheelScreenPos - (DesiredSize * 0.5f);
        PingWheelInstance->SetAlignmentInViewport(FVector2D(0.0f, 0.0f));
        PingWheelInstance->SetPositionInViewport(TopLeft, false);
    }
}

void UPlayerHUDWidget::ShowPingWheelAtScreenLocation(const FVector2D& ScreenLocation, const FVector& WorldLocation)
{
    if (!PingWheelClass)
    {
        UE_LOG(LogCoding, Warning, TEXT("ShowPingWheelAtScreenLocation: PingWheelClass not set"));
        return;
    }

    if (PingWheelInstance)
    {
        PingWheelInstance->RemoveFromParent();
        PingWheelInstance = nullptr;
    }

    PingWheelInstance = CreateWidget<UUserWidget>(GetOwningPlayer(), PingWheelClass);
    if (!PingWheelInstance)
    {
        UE_LOG(LogCoding, Warning, TEXT("ShowPingWheelAtScreenLocation: Failed to create PingWheelInstance"));
        return;
    }

    // Set the world location on the ping wheel widget so the ping appears at the correct location
    if (UPingWheelWidget* PW = Cast<UPingWheelWidget>(PingWheelInstance))
    {
        PW->SetCenterWorldLocation(WorldLocation);
        UE_LOG(LogCoding, Display, TEXT("ShowPingWheelAtScreenLocation: Set ping wheel world location to %s"), *WorldLocation.ToString());
    }

    PingWheelInstance->AddToViewport();
    // Force layout to calculate desired size now
    PingWheelInstance->ForceLayoutPrepass();

    FVector2D DesiredSize = PingWheelInstance->GetDesiredSize();
    FVector2D WheelScreenPos = ScreenLocation;
    UE_LOG(LogCoding, Verbose, TEXT("ShowPingWheelAtScreenLocation: ScreenLocation=(%.1f,%.1f) DesiredSize=(%.1f,%.1f)"), WheelScreenPos.X, WheelScreenPos.Y, DesiredSize.X, DesiredSize.Y);

    if (DesiredSize.IsNearlyZero())
    {
        PingWheelInstance->SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
        PingWheelInstance->SetPositionInViewport(WheelScreenPos, false);
    }
    else
    {
        FVector2D TopLeft = WheelScreenPos - (DesiredSize * 0.5f);
        PingWheelInstance->SetAlignmentInViewport(FVector2D(0.0f, 0.0f));
        PingWheelInstance->SetPositionInViewport(TopLeft, false);
    }
}

void UPlayerHUDWidget::RequestPurchase(AShopActor* Shop, UItemData* Item)
{
    if (!Shop || !Item) return;
    if (APlayerController* PC = GetOwningPlayer())
    {
        if (AMOBAPlayerController* MPC = Cast<AMOBAPlayerController>(PC))
        {
            // Call server RPC on playercontroller to request purchase from the shop
            MPC->Server_RequestPurchase(Shop, Item);
        }
    }
}

void UPlayerHUDWidget::RefreshInventory(const TArray<UItemData*>& Items)
{
    // Array of the 6 item slot images
    UImage* Slots[6] = { Item_1, Item_2, Item_3, Item_4, Item_5, Item_6 };

    for (int32 SlotIndex = 0; SlotIndex < 6; ++SlotIndex)
    {
        UImage* SlotImage = Slots[SlotIndex];
        if (!SlotImage)
        {
            continue;
        }

        if (SlotIndex < Items.Num() && Items[SlotIndex] && Items[SlotIndex]->Icon)
        {
            // Set the item icon
            SlotImage->SetBrushFromTexture(Items[SlotIndex]->Icon);
            SlotImage->SetVisibility(ESlateVisibility::Visible);
            SlotImage->SetColorAndOpacity(FLinearColor::White);
            UE_LOG(LogTemp, Log, TEXT("RefreshInventory: Slot %d set to %s"), SlotIndex, *Items[SlotIndex]->GetName());
        }
        else
        {
            // Clear the slot (make transparent or hide)
            SlotImage->SetBrushFromTexture(nullptr);
            SlotImage->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.2f)); // Semi-transparent for empty slot
        }
    }
}

void UPlayerHUDWidget::ShowStatsPopup()
{
    if (StatsPopupPanel)
    {
        StatsPopupPanel->SetVisibility(ESlateVisibility::Visible);
    }
}

void UPlayerHUDWidget::HideStatsPopup()
{
    if (StatsPopupPanel)
    {
        StatsPopupPanel->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UPlayerHUDWidget::ToggleStatsPopup()
{
    if (!StatsPopupPanel) return;
    ESlateVisibility V = StatsPopupPanel->GetVisibility();
    StatsPopupPanel->SetVisibility(V == ESlateVisibility::Visible ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
}

// ===== Shop Panel Functions =====
void UPlayerHUDWidget::OpenShop(AShopActor* Shop)
{
    // Use the designer-added ShopPanel if present, otherwise create a runtime instance from ShopPanelClass
    if (ShopPanel && Shop)
    {
        ShopPanel->OpenShop(Shop);
        return;
    }

    if (!ShopPanel && ShopPanelClass)
    {
        if (!RuntimeShopPanelInstance)
        {
            RuntimeShopPanelInstance = CreateWidget<UShopPanelWidget>(GetOwningPlayer(), ShopPanelClass);
            if (RuntimeShopPanelInstance)
            {
                RuntimeShopPanelInstance->AddToViewport(9999);
                UE_LOG(LogTemp, Log, TEXT("PlayerHUDWidget: Created runtime ShopPanelInstance and added to viewport"));
            }
        }

        if (RuntimeShopPanelInstance)
        {
            RuntimeShopPanelInstance->OpenShop(Shop);
            return;
        }

        // If we created a runtime panel or the designer panel is present, also create a debug overlay (to ensure UI is visible)
        if (!DebugShopOverlayInstance)
        {
            if (DebugShopOverlayClass)
            {
                UUserWidget* W = CreateWidget(GetOwningPlayer(), DebugShopOverlayClass);
                DebugShopOverlayInstance = Cast<UDebugShopOverlayWidget>(W);
                if (DebugShopOverlayInstance)
                {
                    DebugShopOverlayInstance->AddToViewport(10000);
                    UE_LOG(LogTemp, Log, TEXT("PlayerHUDWidget: Created DebugShopOverlayInstance and added to viewport"));
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("PlayerHUDWidget: DebugShopOverlayClass exists but created instance does not cast to UDebugShopOverlayWidget"));
                }
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("PlayerHUDWidget: DebugShopOverlayClass is not set - no debug overlay created"));
            }
            // set item count if possible
            if (DebugShopOverlayInstance && Shop && Shop->ShopComponent)
            {
                DebugShopOverlayInstance->SetItemCount(Shop->ShopComponent->AvailableItems.Num());
            }
        }
    }
}

void UPlayerHUDWidget::CloseShop()
{
    if (ShopPanel)
    {
        UE_LOG(LogTemp, Log, TEXT("PlayerHUDWidget: Closing shop panel (designer bind)"));
        ShopPanel->CloseShop();
    }

    // Also if we created a runtime instance, close and remove it
    if (RuntimeShopPanelInstance)
    {
        UE_LOG(LogTemp, Log, TEXT("PlayerHUDWidget: Closing and removing runtime ShopPanelInstance"));
        RuntimeShopPanelInstance->CloseShop();
        RuntimeShopPanelInstance->RemoveFromParent();
        RuntimeShopPanelInstance = nullptr;
    }

    if (DebugShopOverlayInstance)
    {
        DebugShopOverlayInstance->RemoveFromParent();
        DebugShopOverlayInstance = nullptr;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerHUDWidget: CloseShop called but ShopPanel is NULL"));
    }
}

void UPlayerHUDWidget::ToggleShop(AShopActor* Shop)
{
    UE_LOG(LogTemp, Log, TEXT("PlayerHUDWidget: ToggleShop called with Shop=%s"), Shop ? *Shop->GetName() : TEXT("NULL"));
    if (!ShopPanel)
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerHUDWidget: ToggleShop called but ShopPanel is NULL"));
        return;
    }

    if (ShopPanel && ShopPanel->IsShopOpen())
    {
        UE_LOG(LogTemp, Log, TEXT("PlayerHUDWidget: ShopPanel was open - closing"));
        ShopPanel->CloseShop();
    }
    else if (Shop)
    {
        UE_LOG(LogTemp, Log, TEXT("PlayerHUDWidget: Opening ShopPanel for %s"), *Shop->GetName());
        ShopPanel->OpenShop(Shop);
    }

    // Also fallback to runtime instance usage if necessary
    if (!ShopPanel && ShopPanelClass)
    {
        if (RuntimeShopPanelInstance && RuntimeShopPanelInstance->IsShopOpen())
        {
            RuntimeShopPanelInstance->CloseShop();
            RuntimeShopPanelInstance->RemoveFromParent();
            RuntimeShopPanelInstance = nullptr;
        }
        else if (Shop)
        {
            if (!RuntimeShopPanelInstance)
            {
                RuntimeShopPanelInstance = CreateWidget<UShopPanelWidget>(GetOwningPlayer(), ShopPanelClass);
                if (RuntimeShopPanelInstance)
                {
                    RuntimeShopPanelInstance->AddToViewport(9999);
                    UE_LOG(LogTemp, Log, TEXT("PlayerHUDWidget: Created runtime ShopPanelInstance and added to viewport (toggle path)"));
                }
            }

            if (RuntimeShopPanelInstance)
            {
                RuntimeShopPanelInstance->OpenShop(Shop);
            }
        }
    }
}

bool UPlayerHUDWidget::IsShopOpen() const
{
    if (ShopPanel && ShopPanel->IsShopOpen()) return true;
    if (RuntimeShopPanelInstance && RuntimeShopPanelInstance->IsShopOpen()) return true;
    if (DebugShopOverlayInstance) return true;
    return false;
}

// (Previously added balancing lines removed; file now uses correct per-function braces.)
