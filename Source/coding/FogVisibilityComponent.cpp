// FogVisibilityComponent - Implementation
#include "FogVisibilityComponent.h"
#include "FogOfWarManager.h"
#include "VisionSourceComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Logging.h"

UFogVisibilityComponent::UFogVisibilityComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
}

void UFogVisibilityComponent::BeginPlay()
{
	Super::BeginPlay();
	
	FogManager = AFogOfWarManager::GetInstance(GetWorld());
	CacheLocalPlayerTeam();
	
	// Initial visibility update
	UpdateVisibility();
}

void UFogVisibilityComponent::CacheLocalPlayerTeam()
{
	// Get local player's team ID for client-side visibility checks
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		// Try to get team from player state or pawn
		// This is project-specific - adjust based on how your game stores team info
		APawn* Pawn = PC->GetPawn();
		if (Pawn)
		{
			// Check if pawn has a VisionSourceComponent to get team
			if (UActorComponent* Comp = Pawn->GetComponentByClass(UVisionSourceComponent::StaticClass()))
			{
				if (UVisionSourceComponent* VSC = Cast<UVisionSourceComponent>(Comp))
				{
					LocalPlayerTeamID = VSC->TeamID;
				}
			}
		}
	}
	
	// Default to team 0 if not found
	if (LocalPlayerTeamID < 0)
	{
		LocalPlayerTeamID = 0;
	}
}

void UFogVisibilityComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	TimeSinceLastCheck += DeltaTime;
	
	if (TimeSinceLastCheck >= VisibilityCheckInterval)
	{
		TimeSinceLastCheck = 0.f;
		UpdateVisibility();
	}
}

void UFogVisibilityComponent::UpdateVisibility()
{
	if (!bHideWhenNotVisible)
	{
		bCurrentlyVisible = true;
		return;
	}
	
	// Check if visible to local player's team
	bCurrentlyVisible = IsVisibleToTeam(LocalPlayerTeamID);
	
	// Apply visibility to the actor (client-side rendering control)
	AActor* Owner = GetOwner();
	if (Owner)
	{
		// Option 1: Hide/show the actor
		bool bIsCurrentlyHidden = Owner->IsHidden();
		if (bIsCurrentlyHidden != !bCurrentlyVisible)
		{
			Owner->SetActorHiddenInGame(!bCurrentlyVisible);
			UE_LOG(LogCoding, Log, TEXT("FogVisibility: %s -> SetActorHiddenInGame(%d) for localTeam=%d visible=%d"),
				*Owner->GetName(), !bCurrentlyVisible ? 1 : 0, LocalPlayerTeamID, bCurrentlyVisible ? 1 : 0);
		}
		
		// Option 2: Could also disable collision when hidden for optimization
		// Owner->SetActorEnableCollision(bCurrentlyVisible);
	}
}

bool UFogVisibilityComponent::IsVisibleToTeam(int32 ObserverTeamID) const
{
	// Always visible to own team
	if (bAlwaysVisibleToOwnTeam && ObserverTeamID == TeamID)
	{
		return true;
	}
	
	// Check fog of war
	if (!FogManager)
	{
		// No fog manager = visible by default
		return true;
	}
	
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return true;
	}
	
	// Check if the actor's location is visible to the observer's team
	return FogManager->IsLocationVisibleToTeam(Owner->GetActorLocation(), ObserverTeamID);
}


