// Fill out your copyright notice in the Description page of Project Settings.

#include "MOBAPlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "InventoryComponent.h"
#include "ShopActor.h"
#include "ShopComponent.h"
#include "MOBACharacter.h"
#include "HealthComponent.h"
#include "CombatComponent.h"
#include "PlayerStatsComponent.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Slate/SceneViewport.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraActor.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "PlayerHUDWidget.h"
#include "Blueprint/UserWidget.h"
#include "PingWheelWidget.h"
#include "AbilityComponent.h"
#include "MapPing.h"
#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Logging.h"
#include "NavigationPath.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Blueprint/WidgetLayoutLibrary.h"

AMOBAPlayerController::AMOBAPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	TargetingRange = 10000.0f;
	HoveredActor = nullptr;
	SelectedTarget = nullptr;
	PreviousHoveredActor = nullptr;
	FollowTime = 0.0f;
	bIsToDestination = false;
	bIsFollowingTarget = false;
	// Camera defaults
	bCameraLocked = true;
	EdgeScrollMargin = 48.0f;
	EdgeScrollSpeed = 4000.0f;
	EdgeScrollOutsideMargin = 300.0f;
	FreeCameraActor = nullptr;
}

void AMOBAPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (DefaultMappingContext)
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("MOBA: DefaultMappingContext is NULL!"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("MOBA: No Subsystem found!"));
	}

	// Create player HUD widget (client-side only)
	if (PlayerHUDClass)
	{
		UUserWidget* W = CreateWidget<UUserWidget>(this, PlayerHUDClass);
		if (W)
		{
			W->AddToViewport();
			PlayerHUDWidget = Cast<UPlayerHUDWidget>(W);
				UE_LOG(LogTemp, Log, TEXT("MOBA: PlayerHUDWidget created: %s"), PlayerHUDWidget ? *PlayerHUDWidget->GetName() : TEXT("NULL"));

			// Bind to stats changed for live updates
			UPlayerStatsComponent* Stats = GetPlayerStatsComponent();
			if (Stats)
			{
				Stats->OnStatsChanged.AddDynamic(this, &AMOBAPlayerController::OnStatsChanged_Handler);
				// Initial populate
				OnStatsChanged_Handler();
			}

			// Also bind to Health and Mana change events for immediate updates
			APawn* P = GetPawn();
			if (P)
			{
				if (UHealthComponent* HC = P->FindComponentByClass<UHealthComponent>())
				{
					HC->OnHealthChanged.AddDynamic(this, &AMOBAPlayerController::OnHealthChanged_Handler);
				}
				if (UAbilityComponent* AC = P->FindComponentByClass<UAbilityComponent>())
				{
					AC->OnManaChanged.AddDynamic(this, &AMOBAPlayerController::OnManaChanged_Handler);
				}
			}

			// Bind inventory changed for UI updates
			UInventoryComponent* Inv = GetPlayerInventoryComponent();
			if (Inv)
			{
				Inv->OnInventoryChanged.AddDynamic(this, &AMOBAPlayerController::OnInventoryChanged_Handler);
				// Initial populate
				if (PlayerHUDWidget)
				{
					PlayerHUDWidget->RefreshInventory(Inv->GetItems());
				}
			}
		}
	}
}

void AMOBAPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (SetDestinationClickAction)
		{
			EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Started, this, &AMOBAPlayerController::OnInputStarted);
			EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Triggered, this, &AMOBAPlayerController::OnSetDestinationTriggered);
			EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Completed, this, &AMOBAPlayerController::OnSetDestinationReleased);
			EnhancedInputComponent->BindAction(SetDestinationClickAction, ETriggerEvent::Canceled, this, &AMOBAPlayerController::OnSetDestinationReleased);
		}

		if (SetDestinationTouchAction)
		{
			EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Started, this, &AMOBAPlayerController::OnInputStarted);
			EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Triggered, this, &AMOBAPlayerController::OnTouchTriggered);
			EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Completed, this, &AMOBAPlayerController::OnTouchReleased);
			EnhancedInputComponent->BindAction(SetDestinationTouchAction, ETriggerEvent::Canceled, this, &AMOBAPlayerController::OnTouchReleased);
		}

		if (RightClickAction)
		{
			EnhancedInputComponent->BindAction(RightClickAction, ETriggerEvent::Triggered, this, &AMOBAPlayerController::OnRightClickTriggered);
		}

		// Zoom binding (mouse wheel axis via Enhanced Input)
		if (ZoomAction)
		{
			EnhancedInputComponent->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &AMOBAPlayerController::OnZoom);
		}

		// Toggle camera lock binding (Enhanced Input)
		if (ToggleCameraLockAction)
		{
			EnhancedInputComponent->BindAction(ToggleCameraLockAction, ETriggerEvent::Started, this, &AMOBAPlayerController::OnToggleCameraLock);
		}
	}

    // Fallback: bind Y key to toggle camera lock even without Enhanced Input action
    if (InputComponent)
    {
        InputComponent->BindKey(EKeys::Y, EInputEvent::IE_Pressed, this, &AMOBAPlayerController::OnToggleCameraLock);
		// Bind left mouse button to allow ctrl+click ping or click-to-move on the world
		InputComponent->BindKey(EKeys::LeftMouseButton, EInputEvent::IE_Pressed, this, &AMOBAPlayerController::OnLeftClickPressed);

		// Debug: bind K to toggle minimap X<->Y swap at runtime
		InputComponent->BindKey(EKeys::K, EInputEvent::IE_Pressed, this, &AMOBAPlayerController::OnToggleMinimapSwap);
	// Debug: bind U to cycle minimap force mapping mode (Auto/Geo/Cached)
	InputComponent->BindKey(EKeys::U, EInputEvent::IE_Pressed, this, &AMOBAPlayerController::OnCycleMinimapForceMapping);

		// Debug: bind I/O to toggle invert X and invert Y on the minimap at runtime
		InputComponent->BindKey(EKeys::I, EInputEvent::IE_Pressed, this, &AMOBAPlayerController::OnToggleMinimapInvertX);
		InputComponent->BindKey(EKeys::O, EInputEvent::IE_Pressed, this, &AMOBAPlayerController::OnToggleMinimapInvertY);

		// Fallback: bind RightMouseButton directly if No Enhanced Input action is assigned for it
		if (!RightClickAction)
		{
			UE_LOG(LogTemp, Warning, TEXT("MOBA: RightClickAction not set - binding Key fallback for RightMouseButton"));
			InputComponent->BindKey(EKeys::RightMouseButton, EInputEvent::IE_Pressed, this, &AMOBAPlayerController::OnRightClickTriggered);
		}
        // Bind P to toggle the shop (fallback raw key input)
		InputComponent->BindKey(EKeys::P, EInputEvent::IE_Pressed, this, &AMOBAPlayerController::OnOpenShopPressed);
        UE_LOG(LogTemp, Log, TEXT("MOBA: Bound raw key P to shop open handler"));
		// Bind P/M for clickable area tuning in-widget
		InputComponent->BindKey(EKeys::P, EInputEvent::IE_Pressed, this, &AMOBAPlayerController::OnIncreaseClickablePaddingPressed);
		InputComponent->BindKey(EKeys::B, EInputEvent::IE_Pressed, this, &AMOBAPlayerController::OnDecreaseClickablePaddingPressed);
		InputComponent->BindKey(EKeys::M, EInputEvent::IE_Pressed, this, &AMOBAPlayerController::OnIncreaseClickableOffsetPressed);
		InputComponent->BindKey(EKeys::N, EInputEvent::IE_Pressed, this, &AMOBAPlayerController::OnDecreaseClickableScalePressed);
		// Toggle minimap debug overlay with T
		InputComponent->BindKey(EKeys::T, EInputEvent::IE_Pressed, this, &AMOBAPlayerController::OnToggleMinimapDebugOverlay);
		// Diagnostics: print minimap overlay values
		InputComponent->BindKey(EKeys::G, EInputEvent::IE_Pressed, this, &AMOBAPlayerController::OnPrintMinimapDiagnostics);
		// Apply last click reprojection delta to clickable offset (calibrate) on L
		InputComponent->BindKey(EKeys::L, EInputEvent::IE_Pressed, this, &AMOBAPlayerController::OnApplyLastClickOffset);
		UE_LOG(LogTemp, Log, TEXT("MOBA: Bound raw keys P/M to adjust clickable area (Padding/Offset)"));
    }
}

void AMOBAPlayerController::OnToggleMinimapSwap()
{
	if (!PlayerHUDWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOBA: PlayerHUDWidget not available to toggle minimap swap"));
		return;
	}

	// Use the widget's own toggle method to avoid accessing protected members directly
	PlayerHUDWidget->ToggleSwapMinimapXY();
	UE_LOG(LogTemp, Log, TEXT("MOBA: Requested PlayerHUDWidget swap toggle"));
}

void AMOBAPlayerController::OnToggleMinimapInvertX()
{
	if (!PlayerHUDWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOBA: PlayerHUDWidget not available to toggle minimap invert X"));
		return;
	}
	PlayerHUDWidget->ToggleInvertMinimapX();
	UE_LOG(LogTemp, Log, TEXT("MOBA: Requested PlayerHUDWidget invert X toggle"));
	}

void AMOBAPlayerController::OnToggleMinimapInvertY()
{
	if (!PlayerHUDWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOBA: PlayerHUDWidget not available to toggle minimap invert Y"));
		return;
	}
	PlayerHUDWidget->ToggleInvertMinimapY();
	UE_LOG(LogTemp, Log, TEXT("MOBA: Requested PlayerHUDWidget invert Y toggle"));
}

void AMOBAPlayerController::OnCycleMinimapForceMapping()
{
	if (!PlayerHUDWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOBA: PlayerHUDWidget not available to cycle minimap mapping mode"));
		return;
	}
	PlayerHUDWidget->CycleMinimapForceMappingMode();
	UE_LOG(LogTemp, Log, TEXT("MOBA: Requested PlayerHUDWidget cycle force mapping mode"));
}

void AMOBAPlayerController::OnToggleMinimapDebugOverlay()
{
	if (!PlayerHUDWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOBA: PlayerHUDWidget not available to toggle minimap overlay"));
		return;
	}
	PlayerHUDWidget->ToggleMinimapDebugOverlay();
	UE_LOG(LogTemp, Log, TEXT("MOBA: Requested PlayerHUDWidget toggle minimap debug overlay"));
}

void AMOBAPlayerController::OnPrintMinimapDiagnostics()
{
	if (!PlayerHUDWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOBA: PlayerHUDWidget not available to print minimap diagnostics"));
		return;
	}
	PlayerHUDWidget->PrintMinimapDiagnostics();
}

void AMOBAPlayerController::OnApplyLastClickOffset()
{
	if (!PlayerHUDWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOBA: PlayerHUDWidget not available to apply last click offset"));
		return;
	}
	// Default apply with Multiplier 1.0 to fully correct measured delta
	PlayerHUDWidget->ApplyLastClickOffsetToClickableArea(1.0f);
}

void AMOBAPlayerController::OnOpenShopPressed()
{
	UE_LOG(LogTemp, Log, TEXT("MOBA: OnOpenShopPressed called on controller %s"), *GetName());
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, TEXT("MOBA: OnOpenShopPressed pressed"));
	}
	OpenClosestShop();
}

void AMOBAPlayerController::OnIncreaseClickablePaddingPressed()
{
	UE_LOG(LogTemp, Log, TEXT("MOBA: OnIncreaseClickablePaddingPressed called"));
	if (!PlayerHUDWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOBA: PlayerHUDWidget not available to increment clickable padding"));
		return;
	}
	PlayerHUDWidget->IncrementClickableAreaPadding();
}

void AMOBAPlayerController::OnIncreaseClickableOffsetPressed()
{
	UE_LOG(LogTemp, Log, TEXT("MOBA: OnIncreaseClickableOffsetPressed called"));
	if (!PlayerHUDWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOBA: PlayerHUDWidget not available to increment clickable offset"));
		return;
	}
	// Increment offset on X axis by default step (positive direction)
	PlayerHUDWidget->IncrementClickableAreaOffset(PlayerHUDWidget->ClickableAreaOffsetStep, 0.0f);
}

void AMOBAPlayerController::OnDecreaseClickablePaddingPressed()
{
	UE_LOG(LogTemp, Log, TEXT("MOBA: OnDecreaseClickablePaddingPressed called"));
	if (!PlayerHUDWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOBA: PlayerHUDWidget not available to decrement clickable padding"));
		return;
	}
	// Call increment with negative multiplier to reduce padding
	PlayerHUDWidget->IncrementClickableAreaPadding(-1.0f);
}

void AMOBAPlayerController::OnDecreaseClickableScalePressed()
{
	UE_LOG(LogTemp, Log, TEXT("MOBA: OnDecreaseClickableScalePressed called"));
	if (!PlayerHUDWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOBA: PlayerHUDWidget not available to decrement clickable scale"));
		return;
	}
	PlayerHUDWidget->IncrementClickableAreaScale(-1.0f);
}

void AMOBAPlayerController::OpenClosestShop()
{
	UE_LOG(LogTemp, Log, TEXT("MOBA: OpenClosestShop called"));
	if (!GetWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("MOBA: OpenClosestShop: No world"));
		return;
	}

	if (!PlayerHUDWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOBA: PlayerHUDWidget not available to open shop [controller=%s]"), *GetName());
		return;
	}

	APawn* P = GetPawn();
	if (!P)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOBA: No pawn available; cannot find nearest shop"));
		return;
	}

	TArray<AActor*> ShopActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AShopActor::StaticClass(), ShopActors);
	UE_LOG(LogTemp, Log, TEXT("MOBA: Found %d shop actors in the world"), ShopActors.Num());

	AShopActor* BestShop = nullptr;
	float BestDist2 = FLT_MAX;
	const FVector MyLoc = P->GetActorLocation();

	for (AActor* A : ShopActors)
	{
		AShopActor* Shop = Cast<AShopActor>(A);
		if (!Shop) continue;
		const float Dist2 = FVector::DistSquared(MyLoc, Shop->GetActorLocation());
		UE_LOG(LogTemp, Log, TEXT("MOBA: Shop candidate=%s dist2=%f"), *Shop->GetName(), Dist2);
		if (Dist2 < BestDist2)
		{
			BestDist2 = Dist2;
			BestShop = Shop;
		}
	}

	if (!BestShop || BestDist2 > (ShopOpenRange * ShopOpenRange))
	{
		UE_LOG(LogTemp, Log, TEXT("MOBA: No shop nearby (within %f). Best found distance^2=%f"), ShopOpenRange, BestDist2);
		if (PlayerHUDWidget->IsShopOpen())
		{
			PlayerHUDWidget->ToggleShop(nullptr);
		}
		return;
	}

	PlayerHUDWidget->ToggleShop(BestShop);
	UE_LOG(LogTemp, Log, TEXT("MOBA: Toggled shop UI for actor %s (distance^2=%f)"), BestShop ? *BestShop->GetName() : TEXT("NULL"), BestDist2);
}

void AMOBAPlayerController::OnZoom(const FInputActionValue& Value)
{
	// Enhanced Input: directly read the 1D axis value
	float Axis = 0.0f;
	Axis = Value.Get<float>();

	APawn* P = GetPawn();
	if (!P)
	{
		return;
	}

	if (AMOBACharacter* MOBAChar = Cast<AMOBACharacter>(P))
	{
		MOBAChar->AdjustZoom(Axis);
	}
}

void AMOBAPlayerController::OnLeftClickPressed()
{
	// Check if user is clicking on the minimap first (screen-space UI check)
	if (PlayerHUDWidget)
	{
		// Get mouse position in viewport coordinates
		float MouseX, MouseY;
		if (GetMousePosition(MouseX, MouseY))
		{
			FVector2D ViewportMousePos(MouseX, MouseY);
			FVector2D ViewportSize(1920, 1080);
			
			// Get viewport size
			if (UGameViewportClient* ViewportClient = GetWorld()->GetGameViewport())
			{
				ViewportClient->GetViewportSize(ViewportSize);
			}
			
			// Use widget's cached absolute adjusted clickable rect (calculated in NativePaint)
			// This ensures perfect alignment with the debug overlay
			FVector2D CursorAbs(0.0f, 0.0f);
			if (FSlateApplication::IsInitialized()) CursorAbs = FSlateApplication::Get().GetCursorPos();
			bool bOverMinimap = PlayerHUDWidget->IsScreenPositionOverAdjustedClickableRect(CursorAbs);

			if (bOverMinimap)
			{
				bool bCtrl = IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl);
				bool bAlt = IsInputKeyDown(EKeys::LeftAlt) || IsInputKeyDown(EKeys::RightAlt);
				UE_LOG(LogCoding, Display, TEXT("MOBA: Left click on minimap detected at ViewportPos=(%.1f,%.1f) CursorAbs=(%.1f,%.1f) -> routing to HUD"), ViewportMousePos.X, ViewportMousePos.Y, CursorAbs.X, CursorAbs.Y);
				bool bHandled = PlayerHUDWidget->HandleMinimapClickViewport(ViewportMousePos, ViewportSize, false, bCtrl, bAlt);
				UE_LOG(LogCoding, Display, TEXT("MOBA: HandleMinimapClickViewport returned %d"), bHandled ? 1 : 0);
				return;
			}
			else
			{
				UE_LOG(LogCoding, Verbose, TEXT("MOBA: Left click NOT on minimap ViewportPos=(%.1f,%.1f), CursorAbs=(%.1f,%.1f), continuing"), ViewportMousePos.X, ViewportMousePos.Y, CursorAbs.X, CursorAbs.Y);
			}
		}
	}

	// Check if user is clicking on UI; if so, skip world trace (we handle minimap clicks in the widget)
	FHitResult Hit;
	GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), true, Hit);
	bool bHitUI = false;
	if (Hit.bBlockingHit)
	{
		// If the hit is a widget component, treat as UI
		if (Hit.GetComponent() && Hit.GetComponent()->IsA(UWidgetComponent::StaticClass()))
		{
			bHitUI = true;
		}
	}

	// If clicking on UI widget (minimap), let widget handle it
	if (bHitUI) return;

	// Otherwise, perform a world trace and either ping (Ctrl) or move (left click)
	bool bCtrl = IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl);

	FVector WorldOrigin, WorldDir;
	DeprojectMousePositionToWorld(WorldOrigin, WorldDir);
	FVector TraceEnd = WorldOrigin + WorldDir * 100000.0f;

	FHitResult WorldHit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(GetPawn());
	if (GetWorld()->LineTraceSingleByChannel(WorldHit, WorldOrigin, TraceEnd, ECC_Visibility, Params))
	{
		FVector Location = WorldHit.Location;
		// If Ctrl is held, open the ping wheel UI so player can select ping type
		if (bCtrl)
		{
			if (PlayerHUDWidget && PlayerHUDWidget->PingWheelClass)
			{
				// Ensure any existing instance is removed
				if (PlayerHUDWidget->PingWheelInstance)
				{
					PlayerHUDWidget->PingWheelInstance->RemoveFromParent();
					PlayerHUDWidget->PingWheelInstance = nullptr;
				}

				UUserWidget* PW = CreateWidget<UUserWidget>(this, PlayerHUDWidget->PingWheelClass);
				if (PW)
				{
					// If it's our C++ PingWheelWidget, set world center
					if (UPingWheelWidget* Ping = Cast<UPingWheelWidget>(PW))
					{
						Ping->SetCenterWorldLocation(Location);
					}
					PW->AddToViewport();
					PlayerHUDWidget->PingWheelInstance = PW;

					// Position the ping wheel using ProjectWorldLocationToScreen
					// This uses the same coordinate system as the ping rendering
					PW->ForceLayoutPrepass();
					FVector2D DesiredSize = PW->GetDesiredSize();
					
					FVector2D ScreenPos = FVector2D::ZeroVector;
					FVector2D ProjectedScreen;
					if (ProjectWorldLocationToScreen(Location, ProjectedScreen, false))
					{
						ScreenPos = ProjectedScreen;
					}
					else
					{
						// Fallback to mouse position
						float MouseX = 0.f, MouseY = 0.f;
						GetMousePosition(MouseX, MouseY);
						ScreenPos = FVector2D(MouseX, MouseY);
					}
					
					// Get viewport scale
					float ViewportScale = UWidgetLayoutLibrary::GetViewportScale(this);
					if (ViewportScale <= 0.0f) ViewportScale = 1.0f;
					
					// Calculate top-left for centering (DesiredSize in viewport coords, ScreenPos in absolute)
					FVector2D TopLeftPos = ScreenPos - (DesiredSize * ViewportScale * 0.5f);
					
					// SetPositionInViewport with bRemoveDPIScale=true divides by ViewportScale
					PW->SetPositionInViewport(TopLeftPos, true);
				}
			}
			else
			{
				// Fallback: send a default OnMyWay ping
				Server_RequestPing(Location, EMapPingType::Ping_OnMyWay);
			}
		}
		else
		{
			// Plain left-click no longer issues movement; selection logic could go here.
			// For now, do nothing on plain left-click to avoid accidental movement.
		}
	}
}

void AMOBAPlayerController::Client_ReceiveNavPath_Implementation(const TArray<FVector>& PathPoints)
{
	if (PlayerHUDWidget)
	{
		PlayerHUDWidget->SetMinimapPath(PathPoints);
	}
}

bool AMOBAPlayerController::Server_RequestPing_Validate(const FVector& WorldLocation, EMapPingType PingType)
{
	return true;
}

void AMOBAPlayerController::Server_RequestPing_Implementation(const FVector& WorldLocation, EMapPingType PingType)
{
	UWorld* W = GetWorld();
	if (!W) return;

	// Use MapPingClass if set (should be BP_MapPing with sounds/visuals), otherwise fallback to C++ class
	UClass* ClassToSpawn = MapPingClass ? MapPingClass.Get() : AMapPing::StaticClass();

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = GetPawn();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AMapPing* Ping = W->SpawnActor<AMapPing>(ClassToSpawn, WorldLocation, FRotator::ZeroRotator, Params);
	if (Ping)
	{
		Ping->PingType = PingType;
		Ping->PingLocation = WorldLocation;
		UE_LOG(LogCoding, Log, TEXT("Server spawned ping at %s type %d (class %s)"), *WorldLocation.ToString(), (int32)PingType, *ClassToSpawn->GetName());
	}
}

bool AMOBAPlayerController::Server_RequestPurchase_Validate(AShopActor* Shop, UItemData* Item)
{
	return Shop != nullptr && Item != nullptr;
}

void AMOBAPlayerController::Server_RequestPurchase_Implementation(AShopActor* Shop, UItemData* Item)
{
	if (!Shop || !Shop->ShopComponent) return;
	// Call server-side shop handling
	Shop->ShopComponent->HandlePurchase(this, Item);
}

bool AMOBAPlayerController::Server_RequestMoveTo_Validate(const FVector& WorldLocation)
{
	return true;
}

void AMOBAPlayerController::Server_RequestMoveTo_Implementation(const FVector& WorldLocation)
{
	APawn* P = GetPawn();
	if (!P) return;

	// Extra debug info to help diagnose why movement may not occur
	UE_LOG(LogCoding, Display, TEXT("Server_RequestMoveTo_Implementation: Authority=%d IsLocalController=%d ControllerRole=%d"), HasAuthority() ? 1 : 0, IsLocalController() ? 1 : 0, (int32)GetLocalRole());
	UE_LOG(LogCoding, Display, TEXT("Server_RequestMoveTo_Implementation: Pawn=%s PawnRole=%d IsPlayerControlled=%d PossessedBy=%s"), P ? *P->GetName() : TEXT("NULL"), P ? (int32)P->GetLocalRole() : -1, P ? (P->IsPlayerControlled() ? 1 : 0) : 0, P && P->GetController() ? *P->GetController()->GetName() : TEXT("NULL"));

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys) return;

	FNavLocation Projected;
	FVector Extent(50.f, 50.f, 200.f);
	UE_LOG(LogCoding, Display, TEXT("Server_RequestMoveTo_Implementation: received move request to %s from client %s (Pawn=%s PawnLocation=%s)"), *WorldLocation.ToString(), *GetName(), P ? *P->GetName() : TEXT("NULL"), P ? *P->GetActorLocation().ToString() : TEXT("NULL"));

	if (NavSys->ProjectPointToNavigation(WorldLocation, Projected, Extent))
	{
		// Use SimpleMoveToLocation - this will request server-side movement for the controller
		UAIBlueprintHelperLibrary::SimpleMoveToLocation(this, Projected.Location);
		UE_LOG(LogCoding, Verbose, TEXT("Server requested move to %s (projected %s) using controller %s"), *WorldLocation.ToString(), *Projected.Location.ToString(), *GetName());

		// If the controller does not possess the pawn (edge case), attempt to use pawn's controller
		if (P->GetController() != this)
		{
			AController* PawnController = P->GetController();
			if (PawnController && PawnController != this)
			{
				UE_LOG(LogCoding, Warning, TEXT("Server_RequestMoveTo: Controller mismatch (this=%s pawn->controller=%s), trying PawnController SimpleMoveTo"), *GetName(), *PawnController->GetName());
				UAIBlueprintHelperLibrary::SimpleMoveToLocation(PawnController, Projected.Location);
			}
		}

		// Compute navigation path and send to client for minimap drawing
		UNavigationPath* NavPath = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), P->GetActorLocation(), Projected.Location);
		if (NavPath && NavPath->IsValid())
		{
			TArray<FVector> Points = NavPath->PathPoints;
			Client_ReceiveNavPath(Points);
		}
		else
		{
			UE_LOG(LogCoding, Warning, TEXT("Server_RequestMoveTo: NavPath invalid or null - will try direct movement fallback"));
			// Fallback: attempt to move pawn directly if CharacterMovement exists
			if (P->IsA<ACharacter>())
			{
				ACharacter* C = Cast<ACharacter>(P);
				if (C && C->GetCharacterMovement())
				{
					FVector Dir = (Projected.Location - C->GetActorLocation()).GetSafeNormal();
					C->AddMovementInput(Dir, 1.0f);
					UE_LOG(LogCoding, Verbose, TEXT("Server_RequestMoveTo: Fallback AddMovementInput toward %s"), *Projected.Location.ToString());
				}
			}
		}
	}
	else
	{
		UE_LOG(LogCoding, Warning, TEXT("Server_RequestMoveTo: ProjectPointToNavigation failed for %s"), *WorldLocation.ToString());
		// Attempt SimpleMoveToLocation directly with the raw location
		UAIBlueprintHelperLibrary::SimpleMoveToLocation(this, WorldLocation);
	}
	}

	bool AMOBAPlayerController::Server_SpawnCursorEffect_Validate(const FVector& WorldLocation)
	{
		return true;
	}

	void AMOBAPlayerController::Server_SpawnCursorEffect_Implementation(const FVector& WorldLocation)
	{
		UE_LOG(LogCoding, Display, TEXT("Server_SpawnCursorEffect_Implementation: spawn request from %s to %s"), *GetName(), *WorldLocation.ToString());
		// Broadcast to all clients so everyone sees the cursor effect
		Multicast_SpawnCursorEffect(WorldLocation);
	}

	void AMOBAPlayerController::Multicast_SpawnCursorEffect_Implementation(const FVector& WorldLocation)
	{
		if (CursorClickEffect)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), CursorClickEffect, WorldLocation, FRotator::ZeroRotator, FVector(1.0f), true, true, ENCPoolMethod::AutoRelease);
			UE_LOG(LogCoding, Verbose, TEXT("Multicast_SpawnCursorEffect: spawned effect at %s"), *WorldLocation.ToString());
		}
		else
		{
			static bool bLoggedMissingCursorEffect = false;
			if (!bLoggedMissingCursorEffect)
			{
				UE_LOG(LogCoding, Warning, TEXT("Multicast_SpawnCursorEffect: CursorClickEffect not set - cannot spawn"));
				bLoggedMissingCursorEffect = true;
			}
		}
	}

void AMOBAPlayerController::OnToggleCameraLock()
{
	UE_LOG(LogTemp, Log, TEXT("MOBA: OnToggleCameraLock pressed (Y)"));

	bCameraLocked = !bCameraLocked;
	UE_LOG(LogTemp, Log, TEXT("MOBA: Camera lock toggled -> %s"), bCameraLocked ? TEXT("Locked") : TEXT("Unlocked"));

	if (bCameraLocked)
	{
		DestroyFreeCamera();
		APawn* P = GetPawn();
		if (P)
		{
			SetViewTargetWithBlend(P, 0.25f);
			if (AMOBACharacter* MOBAChar = Cast<AMOBACharacter>(P))
			{
				if (MOBAChar->CameraBoom)
				{
					MOBAChar->CameraBoom->bUsePawnControlRotation = false;
					MOBAChar->CameraBoom->bInheritPitch = false;
					MOBAChar->CameraBoom->bInheritYaw = false;
					MOBAChar->CameraBoom->bInheritRoll = false;
					MOBAChar->CameraBoom->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f));
				}
			}
		}
	}
	else
	{
		FVector CamLoc;
		FRotator CamRot;
		GetPlayerViewPoint(CamLoc, CamRot);
		SpawnFreeCameraAt(CamLoc, CamRot);
		if (FreeCameraActor)
		{
			SetViewTargetWithBlend(FreeCameraActor, 0.2f);
		}
	}
}

bool AMOBAPlayerController::Server_RequestSetTarget_Validate(AActor* NewTarget)
{
	// Validate we can target that actor, basic sanity checks
	if (!NewTarget) return false;
	UHealthComponent* HealthComp = NewTarget->FindComponentByClass<UHealthComponent>();
	return HealthComp != nullptr && !HealthComp->bIsDead;
}

void AMOBAPlayerController::Server_RequestSetTarget_Implementation(AActor* NewTarget)
{
	// Server-side selection: ensure we can target and set the target on the controlled pawn's combat component
	if (!NewTarget) return;
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn) return;

	// Check not ally
	UHealthComponent* OwnerHealth = ControlledPawn->FindComponentByClass<UHealthComponent>();
	UHealthComponent* TargetHealth = NewTarget->FindComponentByClass<UHealthComponent>();
	int32 OwnerTeam = OwnerHealth ? OwnerHealth->TeamID : -1;
	int32 TargetTeam = TargetHealth ? TargetHealth->TeamID : -1;
	bool bAreEnemies = UHealthComponent::AreEnemies(ControlledPawn, NewTarget);
	UE_LOG(LogCoding, Display, TEXT("Server_RequestSetTarget: OwnerTeam=%d TargetTeam=%d AreEnemies=%d"), OwnerTeam, TargetTeam, bAreEnemies ? 1 : 0);
	if (OwnerHealth && !bAreEnemies)
	{
		UE_LOG(LogCoding, Warning, TEXT("Server_RequestSetTarget: Rejecting target selection - same team or invalid"));
		return; // can't target allies
	}

	// Set SelectedTarget server-side for authoritative logic
	SelectedTarget = NewTarget;
	UE_LOG(LogCoding, Display, TEXT("Server_RequestSetTarget_Implementation: SelectedTarget set to %s for controller %s"), *NewTarget->GetName(), *GetName());

	// Set in CombatComponent for server-side attacking
	UCombatComponent* CombatComp = ControlledPawn->FindComponentByClass<UCombatComponent>();
	if (CombatComp)
	{
		CombatComp->SetTarget(NewTarget);
	}

	// Ensure server-side follow/attack loop is enabled
	bIsFollowingTarget = true;
}

void AMOBAPlayerController::SpawnFreeCameraAt(const FVector& WorldLocation, const FRotator& WorldRotation)
{
	DestroyFreeCamera();
	UWorld* W = GetWorld();
	if (!W) return;
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = GetPawn();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	FreeCameraActor = W->SpawnActor<ACameraActor>(WorldLocation, WorldRotation, Params);
	if (FreeCameraActor)
	{
		UE_LOG(LogTemp, Log, TEXT("MOBA: Spawned FreeCameraActor at %s"), *WorldLocation.ToString());
	}
}

void AMOBAPlayerController::DestroyFreeCamera()
{
	if (FreeCameraActor)
	{
		UE_LOG(LogTemp, Log, TEXT("MOBA: Destroying FreeCameraActor"));
		FreeCameraActor->Destroy();
		FreeCameraActor = nullptr;
	}
}

void AMOBAPlayerController::MoveCameraToWorldLocation(const FVector& WorldLocation)
{
	// Calculate elevated camera position for top-down view
	// Use the same height concept as CameraBoom (TargetArmLength ~1200) + pitch offset
	const float CameraHeight = 1500.f; // Height above ground for top-down view
	const FRotator CamRot = FRotator(-60.f, 0.f, 0.f); // Top-down pitch
	
	// Calculate camera position: offset backwards based on pitch to look at the target point
	// With -60 pitch, the camera should be positioned back and up to look at WorldLocation
	FVector CameraPos;
	CameraPos.X = WorldLocation.X;
	CameraPos.Y = WorldLocation.Y;
	CameraPos.Z = WorldLocation.Z + CameraHeight;
	
	UE_LOG(LogTemp, Log, TEXT("MOBA: MoveCameraToWorldLocation called -> Target=%s CameraPos=%s"), *WorldLocation.ToString(), *CameraPos.ToString());

	// If camera is locked, unlock it first
	if (bCameraLocked)
	{
		bCameraLocked = false;
	}

	// If we already have a free camera, just move it there
	if (FreeCameraActor)
	{
		FreeCameraActor->SetActorLocation(CameraPos);
		FreeCameraActor->SetActorRotation(CamRot);
	}
	else
	{
		// Otherwise spawn one and set view
		SpawnFreeCameraAt(CameraPos, CamRot);
		if (FreeCameraActor)
		{
			SetViewTargetWithBlend(FreeCameraActor, 0.2f);
		}
	}
}

void AMOBAPlayerController::OnStatsChanged_Handler()
{
	UPlayerStatsComponent* Stats = GetPlayerStatsComponent();
	if (!Stats) return;

	if (PlayerHUDWidget)
	{
		PlayerHUDWidget->UpdateFromStats(Stats);
	}
}

void AMOBAPlayerController::OnInventoryChanged_Handler()
{
	if (!PlayerHUDWidget) return;
	UInventoryComponent* Inv = GetPlayerInventoryComponent();
	if (!Inv) return;
	PlayerHUDWidget->RefreshInventory(Inv->GetItems());
}

void AMOBAPlayerController::RefreshHUDInventory()
{
	if (!PlayerHUDWidget) return;
	UInventoryComponent* Inv = GetPlayerInventoryComponent();
	if (!Inv) return;
	PlayerHUDWidget->RefreshInventory(Inv->GetItems());
}

void AMOBAPlayerController::OnHealthChanged_Handler(float Health, float MaxHealth, float DamageTaken)
{
	// Forward to the generic stats handler to update HUD
	OnStatsChanged_Handler();
}

void AMOBAPlayerController::OnManaChanged_Handler(float Mana, float MaxMana)
{
	OnStatsChanged_Handler();
}

void AMOBAPlayerController::HandleEdgeScroll(float DeltaTime)
{
	if (bCameraLocked) return;

	// Use global cursor position so we detect outside-window edges (fullscreen-windowed)
	FVector2D CursorPos = FSlateApplication::Get().GetCursorPos();

	if (!GEngine || !GEngine->GameViewport) return;

	TSharedPtr<SWindow> Window = GEngine->GameViewport->GetWindow();
	if (!Window.IsValid()) return;

	const FVector2D WindowPos = Window->GetPositionInScreen();
	const FVector2D WindowSize = Window->GetSizeInScreen();
	const FVector2D LocalPos = CursorPos - WindowPos; // may be outside [0..WindowSize]

	const float Margin = EdgeScrollMargin;
	const float Outside = EdgeScrollOutsideMargin;

	float Horizontal = 0.f;
	float Vertical = 0.f;

	// Horizontal (left)
	if (LocalPos.X <= Margin && LocalPos.X >= -Outside)
	{
		float Factor = (Margin - LocalPos.X) / (Margin + Outside);
		Horizontal = -FMath::Clamp(Factor, 0.f, 1.f);
	}
	else if (LocalPos.X < -Outside)
	{
		Horizontal = -1.f;
	}
	else
	{
		// Right side
		float RightEdge = WindowSize.X - Margin;
		if (LocalPos.X >= RightEdge && LocalPos.X <= WindowSize.X + Outside)
		{
			float Factor = (LocalPos.X - RightEdge) / (Margin + Outside);
			Horizontal = FMath::Clamp(Factor, 0.f, 1.f);
		}
		else if (LocalPos.X > WindowSize.X + Outside)
		{
			Horizontal = 1.f;
		}
	}

	// Vertical (top -> forward; bottom -> backward)
	if (LocalPos.Y <= Margin && LocalPos.Y >= -Outside)
	{
		float Factor = (Margin - LocalPos.Y) / (Margin + Outside);
		Vertical = FMath::Clamp(Factor, 0.f, 1.f);
	}
	else if (LocalPos.Y < -Outside)
	{
		Vertical = 1.f;
	}
	else
	{
		float BottomEdge = WindowSize.Y - Margin;
		if (LocalPos.Y >= BottomEdge && LocalPos.Y <= WindowSize.Y + Outside)
		{
			float Factor = (LocalPos.Y - BottomEdge) / (Margin + Outside);
			Vertical = -FMath::Clamp(Factor, 0.f, 1.f);
		}
		else if (LocalPos.Y > WindowSize.Y + Outside)
		{
			Vertical = -1.f;
		}
	}

	if (FMath::IsNearlyZero(Horizontal) && FMath::IsNearlyZero(Vertical)) return;

	const FRotator CamRot = GetControlRotation();
	const FRotator YawRot(0.f, CamRot.Yaw, 0.f);
	const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);

	FVector MoveVec = Right * Horizontal + Forward * Vertical;
	if (MoveVec.SizeSquared() < SMALL_NUMBER) return;

	// Prevent diagonal speed boost: clamp combined vector length to 1
	if (MoveVec.SizeSquared() > 1.f)
	{
		MoveVec = MoveVec.GetSafeNormal();
	}

	const FVector Delta = MoveVec * EdgeScrollSpeed * DeltaTime;

	if (FreeCameraActor)
	{
		UE_LOG(LogTemp, Verbose, TEXT("MOBA: EdgeScroll moving FreeCameraActor Delta=%s (H=%.3f V=%.3f) LocalPos=(%.1f,%.1f) WindowSize=(%.1f,%.1f)"), *Delta.ToString(), Horizontal, Vertical, LocalPos.X, LocalPos.Y, WindowSize.X, WindowSize.Y);
		FreeCameraActor->AddActorWorldOffset(Delta, true);
	}
	else
	{
		APawn* ControlledPawn = GetPawn();
		if (ControlledPawn)
		{
			UE_LOG(LogTemp, Verbose, TEXT("MOBA: EdgeScroll moving Pawn Delta=%s (H=%.3f V=%.3f) LocalPos=(%.1f,%.1f) WindowSize=(%.1f,%.1f)"), *Delta.ToString(), Horizontal, Vertical, LocalPos.X, LocalPos.Y, WindowSize.X, WindowSize.Y);
			ControlledPawn->AddActorWorldOffset(Delta, true);
		}
	}
}

void AMOBAPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateMouseHover();

	// Follow target to attack
	if (bIsFollowingTarget && SelectedTarget)
	{
		FollowAndAttackTarget();
	}
	else if (bIsToDestination)
	{
		APawn* ControlledPawn = GetPawn();
		if (ControlledPawn)
		{
			const FVector WorldDirection = (CachedDestination - ControlledPawn->GetActorLocation()).GetSafeNormal();
			ControlledPawn->AddMovementInput(WorldDirection, 1.0f, false);
		}
	}

	// Additionally, when locking ensure the pawn's spring arm uses a fixed rotation
	if (bCameraLocked)
	{
		APawn* P = GetPawn();
		if (P)
		{
			if (AMOBACharacter* MOBAChar = Cast<AMOBACharacter>(P))
			{
				if (MOBAChar->CameraBoom)
				{
					MOBAChar->CameraBoom->bUsePawnControlRotation = false;
					MOBAChar->CameraBoom->bInheritPitch = false;
					MOBAChar->CameraBoom->bInheritYaw = false;
					MOBAChar->CameraBoom->bInheritRoll = false;
					MOBAChar->CameraBoom->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f));
				}
			}
		}
	}

	// Edge scrolling when unlocked
	if (!bCameraLocked)
	{
		HandleEdgeScroll(DeltaTime);
	}
}

void AMOBAPlayerController::UpdateMouseHover()
{
	FHitResult HitResult;
	GetHitResultUnderCursor(ECC_Visibility, false, HitResult);

	AActor* NewHoveredActor = nullptr;

	if (HitResult.bBlockingHit)
	{
		AActor* HitActor = HitResult.GetActor();
		if (HitActor)
		{
			// Debug: log what we're hitting
			UE_LOG(LogTemp, Verbose, TEXT("Cursor hit: %s"), *HitActor->GetName());
			
			UHealthComponent* HealthComp = HitActor->FindComponentByClass<UHealthComponent>();
			if (HealthComp)
			{
				NewHoveredActor = HitActor;
			}
			else
			{
				// Try parent actor (in case we hit a component)
				AActor* OwnerActor = HitActor->GetOwner();
				if (OwnerActor)
				{
					HealthComp = OwnerActor->FindComponentByClass<UHealthComponent>();
					if (HealthComp)
					{
						NewHoveredActor = OwnerActor;
					}
				}
			}
		}
	}

	if (NewHoveredActor != PreviousHoveredActor)
	{
		if (PreviousHoveredActor && PreviousHoveredActor != SelectedTarget)
		{
			HighlightActor(PreviousHoveredActor, false);
		}

		if (NewHoveredActor && NewHoveredActor != SelectedTarget)
		{
			HighlightActor(NewHoveredActor, true);
		}

		PreviousHoveredActor = NewHoveredActor;
	}

	HoveredActor = NewHoveredActor;
}

void AMOBAPlayerController::OnInputStarted()
{
	// Left click - select/target hovered actor
	if (HoveredActor)
	{
		UHealthComponent* HealthComp = HoveredActor->FindComponentByClass<UHealthComponent>();
		if (HealthComp && !HealthComp->bIsDead)
		{
			SelectTarget(HoveredActor);
			return;
		}
	}
	
	// Clicked on nothing - clear target
	ClearSelectedTarget();
}

void AMOBAPlayerController::OnSetDestinationTriggered()
{
	// Left click hold - do nothing (or could show target info)
	FollowTime += GetWorld()->GetDeltaSeconds();
}

void AMOBAPlayerController::OnSetDestinationReleased()
{
	FollowTime = 0.0f;
}

void AMOBAPlayerController::OnTouchTriggered()
{
	FHitResult Hit;
	if (GetHitResultUnderFinger(ETouchIndex::Touch1, ECollisionChannel::ECC_Visibility, true, Hit))
	{
		UAIBlueprintHelperLibrary::SimpleMoveToLocation(this, Hit.Location);
	}
}

void AMOBAPlayerController::OnTouchReleased()
{
	bIsToDestination = false;
}

void AMOBAPlayerController::HighlightActor(AActor* Actor, bool bHighlight)
{
	if (!Actor)
	{
		return;
	}

	TArray<UStaticMeshComponent*> MeshComponents;
	Actor->GetComponents<UStaticMeshComponent>(MeshComponents);

	for (UStaticMeshComponent* MeshComp : MeshComponents)
	{
		if (MeshComp)
		{
			MeshComp->SetRenderCustomDepth(bHighlight);
			
			if (bHighlight)
			{
				MeshComp->SetCustomDepthStencilValue(Actor == SelectedTarget ? 252 : 251);
			}
		}
	}

	TArray<USkeletalMeshComponent*> SkeletalMeshComponents;
	Actor->GetComponents<USkeletalMeshComponent>(SkeletalMeshComponents);

	for (USkeletalMeshComponent* SkeletalMeshComp : SkeletalMeshComponents)
	{
		if (SkeletalMeshComp)
		{
			SkeletalMeshComp->SetRenderCustomDepth(bHighlight);
			
			if (bHighlight)
			{
				SkeletalMeshComp->SetCustomDepthStencilValue(Actor == SelectedTarget ? 252 : 251);
			}
		}
	}
}

UHealthComponent* AMOBAPlayerController::GetHoveredActorHealth() const
{
	if (HoveredActor)
	{
		return HoveredActor->FindComponentByClass<UHealthComponent>();
	}
	return nullptr;
}

UHealthComponent* AMOBAPlayerController::GetSelectedTargetHealth() const
{
	if (SelectedTarget)
	{
		return SelectedTarget->FindComponentByClass<UHealthComponent>();
	}
	return nullptr;
}

UPlayerStatsComponent* AMOBAPlayerController::GetPlayerStatsComponent() const
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn) return nullptr;

	// Try native component first (AMOBACharacter has it as a member)
	if (AMOBACharacter* MOBAChar = Cast<AMOBACharacter>(ControlledPawn))
	{
		return MOBAChar->PlayerStatsComponent;
	}

	// Fallback: FindComponentByClass (for BP-only pawns)
	return ControlledPawn->FindComponentByClass<UPlayerStatsComponent>();
}

UInventoryComponent* AMOBAPlayerController::GetPlayerInventoryComponent() const
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn) return nullptr;

	if (AMOBACharacter* MOBAChar = Cast<AMOBACharacter>(ControlledPawn))
	{
		return MOBAChar->InventoryComponent;
	}

	return ControlledPawn->FindComponentByClass<UInventoryComponent>();
}

void AMOBAPlayerController::SelectTarget(AActor* NewTarget)
{
	// Check if target is an enemy before selecting
	if (NewTarget)
	{
		APawn* ControlledPawn = GetPawn();
		if (ControlledPawn)
		{
			// Only allow targeting enemies
			if (!UHealthComponent::AreEnemies(ControlledPawn, NewTarget))
			{
				return; // Can't select allies
			}
		}
	}

	// Clear previous highlight
	if (SelectedTarget)
	{
		HighlightActor(SelectedTarget, false);
	}

	SelectedTarget = NewTarget;

	// Highlight new target
	if (SelectedTarget)
	{
		HighlightActor(SelectedTarget, true);
	}

	// Set target on combat component
	APawn* ControlledPawn = GetPawn();
	if (ControlledPawn)
	{
		UCombatComponent* CombatComp = ControlledPawn->FindComponentByClass<UCombatComponent>();
		if (CombatComp)
		{
			CombatComp->SetTarget(SelectedTarget);
		}
	}
}

void AMOBAPlayerController::ClearSelectedTarget()
{
	if (SelectedTarget)
	{
		HighlightActor(SelectedTarget, false);
	}

	SelectedTarget = nullptr;
	bIsFollowingTarget = false;

	// Clear target on combat component
	APawn* ControlledPawn = GetPawn();
	if (ControlledPawn)
	{
		UCombatComponent* CombatComp = ControlledPawn->FindComponentByClass<UCombatComponent>();
		if (CombatComp)
		{
			CombatComp->ClearTarget();
		}
	}
}

void AMOBAPlayerController::OnRightClickTriggered()
{
	UE_LOG(LogCoding, Verbose, TEXT("OnRightClickTriggered: called"));

	// Check if user is clicking on the minimap first (screen-space UI check)
	if (PlayerHUDWidget)
	{
		// Get mouse position in viewport coordinates
		float MouseX, MouseY;
		if (GetMousePosition(MouseX, MouseY))
		{
			FVector2D ViewportMousePos(MouseX, MouseY);
			FVector2D ViewportSize(1920, 1080);
			
			// Get viewport size
			if (UGameViewportClient* ViewportClient = GetWorld()->GetGameViewport())
			{
				ViewportClient->GetViewportSize(ViewportSize);
			}
			
			// Use widget's cached absolute adjusted clickable rect (calculated in NativePaint)
			// This ensures perfect alignment with the debug overlay
			FVector2D CursorAbs(0.0f, 0.0f);
			if (FSlateApplication::IsInitialized()) CursorAbs = FSlateApplication::Get().GetCursorPos();
			bool bOverMinimap = PlayerHUDWidget->IsScreenPositionOverAdjustedClickableRect(CursorAbs);

			if (bOverMinimap)
			{
				bool bCtrl = IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl);
				bool bAlt = IsInputKeyDown(EKeys::LeftAlt) || IsInputKeyDown(EKeys::RightAlt);
				UE_LOG(LogCoding, Display, TEXT("MOBA: Right click on minimap detected at ViewportPos=(%.1f,%.1f) CursorAbs=(%.1f,%.1f) -> routing to HUD"), ViewportMousePos.X, ViewportMousePos.Y, CursorAbs.X, CursorAbs.Y);
				PlayerHUDWidget->HandleMinimapClickViewport(ViewportMousePos, ViewportSize, true, bCtrl, bAlt);
				return;
			}
			else
			{
				UE_LOG(LogCoding, Verbose, TEXT("MOBA: Right click NOT on minimap ViewportPos=(%.1f,%.1f), CursorAbs=(%.1f,%.1f), continuing"), ViewportMousePos.X, ViewportMousePos.Y, CursorAbs.X, CursorAbs.Y);
			}
		}
	}

	// Stop current movement
	StopMovement();
	bIsFollowingTarget = false;

	// Check if we're hovering over an attackable target
	if (HoveredActor)
	{
		UHealthComponent* HealthComp = HoveredActor->FindComponentByClass<UHealthComponent>();
		if (HealthComp && !HealthComp->bIsDead)
		{
			// Select and start following target to attack
			UE_LOG(LogCoding, Log, TEXT("OnRightClickTriggered: Hovered actor %s - selecting and following"), *HoveredActor->GetName());
			// Select locally for visual feedback, and request server to set target for authoritative combat
			SelectTarget(HoveredActor);
			Server_RequestSetTarget(HoveredActor);
			bIsFollowingTarget = true;
			return;
		}
	}

	// No valid target - just move to location (server authoritative)
	FHitResult Hit;
	if (GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, true, Hit))
	{
		UE_LOG(LogCoding, Display, TEXT("OnRightClickTriggered: Requesting server move to %s"), *Hit.Location.ToString());
		// Request server to move the pawn (authoritative)
		Server_RequestMoveTo(Hit.Location);
		// Spawn cursor effect on all clients via server RPC
		Server_SpawnCursorEffect(Hit.Location);
	}
	else
	{
		UE_LOG(LogCoding, Warning, TEXT("OnRightClickTriggered: no hit under cursor"));
	}
}

void AMOBAPlayerController::FollowAndAttackTarget()
{
	if (!SelectedTarget)
	{
		bIsFollowingTarget = false;
		return;
	}

	// Check if target is dead
	UHealthComponent* TargetHealth = SelectedTarget->FindComponentByClass<UHealthComponent>();
	if (!TargetHealth || TargetHealth->bIsDead)
	{
		ClearSelectedTarget();
		return;
	}

	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return;
	}

	UCombatComponent* CombatComp = ControlledPawn->FindComponentByClass<UCombatComponent>();
	if (!CombatComp)
	{
		return;
	}

	float DistanceToTarget = FVector::Dist(ControlledPawn->GetActorLocation(), SelectedTarget->GetActorLocation());
	float AttackRange = CombatComp->AttackRange;

	// Check if in attack range
	if (DistanceToTarget <= AttackRange)
	{
		// In range - stop and attack
		StopMovement();
		if (CombatComp->CanAttack())
		{
			CombatComp->Attack();
		}
	}
	else
	{
		// Not in range - move towards target
		UAIBlueprintHelperLibrary::SimpleMoveToLocation(this, SelectedTarget->GetActorLocation());
	}
}
