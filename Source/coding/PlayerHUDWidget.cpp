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

void UPlayerHUDWidget::UpdateFromStats(UPlayerStatsComponent* Stats)
{
    // Detailed stats
    if (Armor)
    {
        Armor->SetText(FText::AsNumber(Stats->GetArmor()));
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

    UE_LOG(LogCoding, Log, TEXT("PlayerHUDWidget::NativeConstruct - HUD constructed and ready. MapImage=%s WorldBoundsHalfSize=(%.1f,%.1f) WorldCenter=(%.1f,%.1f) AutoDetectWorldBounds=%d"), MapImage ? TEXT("Valid") : TEXT("Null"), WorldBoundsHalfSize.X, WorldBoundsHalfSize.Y, WorldCenter.X, WorldCenter.Y, bAutoDetectWorldBounds ? 1 : 0);

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
    CachedIcons.Empty();
    for (TWeakObjectPtr<UMinimapComponent>& WeakComp : RegisteredMinimapComponents)
    {
        if (!WeakComp.IsValid()) continue;
        UMinimapComponent* MC = WeakComp.Get();
        if (!MC->bShowOnMinimap) continue;
        AActor* Owner = MC->GetOwner();
        if (!Owner) continue;

        FMinimapIcon I;
        I.WorldLocation = Owner->GetActorLocation();
        I.OwnerActor = Owner;
        I.Normalized = WorldToMinimapNormalized(I.WorldLocation);
        I.Color = MC->IconColor;
        I.Size = MC->IconSize > 0 ? MC->IconSize : 6.0f;
        I.TeamID = MC->TeamID;
        CachedIcons.Add(I);
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
                    UE_LOG(LogCoding, Display, TEXT("MinimapIcon[%d] Owner=%s World=%s Norm=(%.3f,%.3f)"), i, *Name, *MI.WorldLocation.ToString(), MI.Normalized.X, MI.Normalized.Y);
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

    // Check if click is within the MinimapContainer/MapImage bounds
    if (!MapImage || !MinimapContainer)
    {
        UE_LOG(LogCoding, Verbose, TEXT("PlayerHUDWidget: MapImage or MinimapContainer is null. MapImage=%s MinimapContainer=%s"), MapImage ? TEXT("Valid") : TEXT("Null"), MinimapContainer ? TEXT("Valid") : TEXT("Null"));
        return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
    }

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
    // Get the geometry of the MapImage (used only if click is inside minimap)
    FGeometry MapGeometry = MapImage->GetCachedGeometry();
    FVector2D LocalPos = MapGeometry.AbsoluteToLocal(ScreenPos);
    FVector2D Size = MapGeometry.GetLocalSize();

    // Prepare world hit; we'll compute differently depending on whether click was inside minimap
    FVector WorldHit = FVector::ZeroVector;

    bool bInsideMap = true;
    if (LocalPos.X < 0 || LocalPos.Y < 0 || LocalPos.X > Size.X || LocalPos.Y > Size.Y)
    {
        bInsideMap = false;
    }

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
        // - If camera is unlocked: move camera to that location
        // - If camera is locked: consume and do nothing (pings are world only)
        // Right-click on minimap: always move/attack
        if (Button == EKeys::LeftMouseButton)
        {
            // Left-click on minimap: if Ctrl is held, open ping wheel; otherwise consume without moving camera
            if (Size.X <= 0 || Size.Y <= 0)
            {
                return FReply::Handled();
            }

            FVector2D Normalized(LocalPos.X / Size.X, LocalPos.Y / Size.Y);
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

            WorldHit.X = WorldCenter.X + NX * 2.0f * WorldBoundsHalfSize.X;
            WorldHit.Y = WorldCenter.Y + NY * 2.0f * WorldBoundsHalfSize.Y;
            WorldHit.Z = 0.0f;

            if (bCtrl)
            {
                // Open ping wheel at this world location
                ShowPingWheelAtWorldLocation(WorldHit);
                return FReply::Handled();
            }

            // If the camera is unlocked, allow left-click on minimap to move the camera
            if (bCameraUnlocked && MPC)
            {
                UE_LOG(LogCoding, Display, TEXT("PlayerHUDWidget: Minimap left-click with camera unlocked - moving camera to %s (LocalPos=%.1f,%.1f Size=%.1f,%.1f Norm=%.3f,%.3f NX=%.3f NY=%.3f WorldCenter=%.1f,%.1f WorldBounds=%.1f,%.1f)"), 
                    *WorldHit.ToString(), LocalPos.X, LocalPos.Y, Size.X, Size.Y, Normalized.X, Normalized.Y, NX, NY, WorldCenter.X, WorldCenter.Y, WorldBoundsHalfSize.X, WorldBoundsHalfSize.Y);
                MPC->MoveCameraToWorldLocation(WorldHit);
            }

            // Consume left-clicks on minimap (do not move pawn unless it's a right-click)
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

        // Normalized coordinates (0..1)
        FVector2D Normalized(LocalPos.X / Size.X, LocalPos.Y / Size.Y);

            UE_LOG(LogCoding, Verbose, TEXT("PlayerHUDWidget: Minimap RightClick Ctrl=%d Normalized=(%.3f,%.3f) LocalPos=(%.1f,%.1f) Size=(%.1f,%.1f) WorldBoundsHalfSize=(%.1f,%.1f)"), bCtrl ? 1 : 0, Normalized.X, Normalized.Y, LocalPos.X, LocalPos.Y, Size.X, Size.Y, WorldBoundsHalfSize.X, WorldBoundsHalfSize.Y);

        // Convert to world coordinates: assume minimap is top-down with +X to right and +Y down
        float NX = (Normalized.X - 0.5f);
        float NY = (0.5f - Normalized.Y);

        UE_LOG(LogCoding, Log, TEXT("PlayerHUDWidget: Click raw NX=%.4f NY=%.4f (before swap/invert)"), NX, NY);

        if (bSwapMinimapXY)
        {
            float Tmp = NX;
            NX = NY;
            NY = Tmp;
            UE_LOG(LogCoding, Log, TEXT("PlayerHUDWidget: Applied swap -> NX=%.4f NY=%.4f"), NX, NY);
        }

        if (bInvertMinimapX)
        {
            NX = -NX;
            UE_LOG(LogCoding, Log, TEXT("PlayerHUDWidget: Applied invert X -> NX=%.4f"), NX);
        }
        if (bInvertMinimapY)
        {
            NY = -NY;
            UE_LOG(LogCoding, Log, TEXT("PlayerHUDWidget: Applied invert Y -> NY=%.4f"), NY);
        }

        WorldHit.X = WorldCenter.X + NX * 2.0f * WorldBoundsHalfSize.X;
        WorldHit.Y = WorldCenter.Y + NY * 2.0f * WorldBoundsHalfSize.Y;
        WorldHit.Z = 0.0f;
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
                    // Show ping wheel centered on the minimap click using the actual screen pos (ensures correct placement)
                    ShowPingWheelAtScreenLocation(ScreenPos);
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

    // Only draw minimap elements if we have a MapImage
    if (!MapImage)
    {
        return RetLayer;
    }

    // Get the geometry of the MapImage for drawing
    FGeometry MapGeometry = MapImage->GetCachedGeometry();
    FVector2D Size = MapGeometry.GetLocalSize();

    if (Size.X <= 0 || Size.Y <= 0)
    {
        return RetLayer;
    }

    const float MarkerSize = 8.0f;

    // Debug: force-draw helpers (temporary)
    const int32 DebugLayerOffset = 1000; // draw well above normal UI for visibility while debugging
    const bool bForceDebugDraw = true;
    // Draw pings as small crosses on the minimap
    for (TWeakObjectPtr<AMapPing> WeakPing : CachedPings)
    {
        if (!WeakPing.IsValid()) continue;
        AMapPing* Ping = WeakPing.Get();
        FVector2D Norm = WorldToMinimapNormalized(Ping->PingLocation);
        FVector2D PixelPos = FVector2D(Norm.X * Size.X, Norm.Y * Size.Y);

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

    // Draw minimap icons (allies, minions, towers)
    for (const FMinimapIcon& Icon : CachedIcons)
    {
        // Fog of War visibility check: hide enemy icons if not in local player's vision
        if (Icon.TeamID != LocalPlayerTeamID)
        {
            // This is an enemy icon - check fog of war
            if (FogManager)
            {
                if (!FogManager->IsLocationVisibleToTeam(Icon.WorldLocation, LocalPlayerTeamID))
                {
                    // Enemy not in our vision - skip drawing this icon
                    continue;
                }
            }
        }

        FVector2D Norm = Icon.Normalized;
        FVector2D PixelPos = FVector2D(Norm.X * Size.X, Norm.Y * Size.Y);
        float S = Icon.Size;

        // If forcing debug draw, draw a filled box using the white brush tinted to Icon.Color and larger size
        if (bForceDebugDraw)
        {
            const float DrawSize = FMath::Max(12.0f, S * 3.0f);
            const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
            // Draw a thick outline rectangle as a visible marker (avoid using ToOffsetPaintGeometry overloads)
            TArray<FVector2D> BoxPoints;
            BoxPoints.Add(PixelPos + FVector2D(-DrawSize, -DrawSize));
            BoxPoints.Add(PixelPos + FVector2D(DrawSize, -DrawSize));
            BoxPoints.Add(PixelPos + FVector2D(DrawSize, DrawSize));
            BoxPoints.Add(PixelPos + FVector2D(-DrawSize, DrawSize));
            BoxPoints.Add(PixelPos + FVector2D(-DrawSize, -DrawSize));
            FSlateDrawElement::MakeLines(
                OutDrawElements,
                RetLayer + DebugLayerOffset + 2,
                MapGeometry.ToPaintGeometry(),
                BoxPoints,
                ESlateDrawEffect::None,
                Icon.Color,
                true,
                6.0f
            );
        }
        else
        {
            TArray<FVector2D> Box;
            Box.Add(PixelPos + FVector2D(-S, -S));
            Box.Add(PixelPos + FVector2D(S, -S));
            Box.Add(PixelPos + FVector2D(S, S));
            Box.Add(PixelPos + FVector2D(-S, S));
            Box.Add(PixelPos + FVector2D(-S, -S));

            FSlateDrawElement::MakeLines(
                OutDrawElements,
                RetLayer + 1,
                MapGeometry.ToPaintGeometry(),
                Box,
                ESlateDrawEffect::None,
                Icon.Color,
                true,
                2.0f
            );
            RetLayer += 1;
        }
    }

    // Debug: draw a visible border around the minimap area so we can see where MapImage is located
    if (bForceDebugDraw)
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

void UPlayerHUDWidget::ShowPingWheelAtScreenLocation(const FVector2D& ScreenLocation)
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

    // Optionally set the world location on the internal widget if it expects it (don't require)
    if (UPingWheelWidget* PW = Cast<UPingWheelWidget>(PingWheelInstance))
    {
        // no world location provided here
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
