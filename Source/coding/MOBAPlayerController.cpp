// Fill out your copyright notice in the Description page of Project Settings.

#include "MOBAPlayerController.h"
#include "MOBACharacter.h"
#include "HealthComponent.h"
#include "CombatComponent.h"
#include "PlayerStatsComponent.h"
#include "Engine/World.h"
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
#include "AbilityComponent.h"
#include "MapPing.h"
#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Logging.h"
#include "NavigationPath.h"

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

	UE_LOG(LogTemp, Warning, TEXT("MOBA: BeginPlay called"));

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		UE_LOG(LogTemp, Warning, TEXT("MOBA: Subsystem found"));
		if (DefaultMappingContext)
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
			UE_LOG(LogTemp, Warning, TEXT("MOBA: MappingContext added!"));
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
		}
	}

}

void AMOBAPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UE_LOG(LogTemp, Warning, TEXT("MOBA: SetupInputComponent called"));

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		UE_LOG(LogTemp, Warning, TEXT("MOBA: EnhancedInputComponent found"));
		if (SetDestinationClickAction)
		{
			UE_LOG(LogTemp, Warning, TEXT("MOBA: Binding SetDestinationClickAction"));
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

		// Debug: bind I/O to toggle invert X and invert Y on the minimap at runtime
		InputComponent->BindKey(EKeys::I, EInputEvent::IE_Pressed, this, &AMOBAPlayerController::OnToggleMinimapInvertX);
		InputComponent->BindKey(EKeys::O, EInputEvent::IE_Pressed, this, &AMOBAPlayerController::OnToggleMinimapInvertY);
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
			// If Ctrl is held, open/send a ping. Otherwise only request move.
			if (bCtrl)
			{
				Server_RequestPing(Location, EMapPingType::Ping_OnMyWay);
			}
			else
			{
				// Only request movement on plain left-click; pings should come from the ping wheel (Ctrl+click)
				Server_RequestMoveTo(Location);
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

bool AMOBAPlayerController::Server_RequestMoveTo_Validate(const FVector& WorldLocation)
{
	return true;
}

void AMOBAPlayerController::Server_RequestMoveTo_Implementation(const FVector& WorldLocation)
{
	APawn* P = GetPawn();
	if (!P) return;

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys) return;

	FNavLocation Projected;
	FVector Extent(50.f, 50.f, 200.f);
	if (NavSys->ProjectPointToNavigation(WorldLocation, Projected, Extent))
	{
		// Use SimpleMoveToLocation - this will request server-side movement for the controller
		UAIBlueprintHelperLibrary::SimpleMoveToLocation(this, Projected.Location);
		UE_LOG(LogCoding, Verbose, TEXT("Server requested move to %s (projected %s)"), *WorldLocation.ToString(), *Projected.Location.ToString());

		// Compute navigation path and send to client for minimap drawing
		UNavigationPath* NavPath = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), P->GetActorLocation(), Projected.Location);
		if (NavPath && NavPath->IsValid())
		{
			TArray<FVector> Points = NavPath->PathPoints;
			Client_ReceiveNavPath(Points);
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
	UE_LOG(LogTemp, Log, TEXT("MOBA: MoveCameraToWorldLocation called -> %s"), *WorldLocation.ToString());

	// If camera is locked, spawn a free camera and move view to it
	if (bCameraLocked)
	{
		// Unlock camera and spawn free camera at location with a default top-down pitch
		bCameraLocked = false;
		FRotator CamRot = FRotator(-60.f, 0.f, 0.f);
		SpawnFreeCameraAt(WorldLocation, CamRot);
		if (FreeCameraActor)
		{
			SetViewTargetWithBlend(FreeCameraActor, 0.2f);
		}
		return;
	}

	// If we already have a free camera, just move it there
	if (FreeCameraActor)
	{
		FreeCameraActor->SetActorLocation(WorldLocation);
	}
	else
	{
		// Otherwise spawn one and set view
		FRotator CamRot = FRotator(-60.f, 0.f, 0.f);
		SpawnFreeCameraAt(WorldLocation, CamRot);
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
			SelectTarget(HoveredActor);
			bIsFollowingTarget = true;
			return;
		}
	}

	// No valid target - just move to location
	FHitResult Hit;
	if (GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, true, Hit))
	{
		UAIBlueprintHelperLibrary::SimpleMoveToLocation(this, Hit.Location);
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
