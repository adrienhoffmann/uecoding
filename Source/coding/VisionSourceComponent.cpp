// Vision Source Component - Implementation
#include "VisionSourceComponent.h"
#include "FogOfWarManager.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Logging.h"

UVisionSourceComponent::UVisionSourceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// Component doesn't need to tick - FogOfWarManager pulls data from registered sources
}

void UVisionSourceComponent::BeginPlay()
{
	Super::BeginPlay();
	
	TryRegisterWithManager();
}

void UVisionSourceComponent::TryRegisterWithManager()
{
	// Try to find the fog manager
	FogManager = AFogOfWarManager::GetInstance(GetWorld());
	
	if (FogManager)
	{
		FogManager->RegisterVisionSource(this);
		UE_LOG(LogCoding, Log, TEXT("VisionSourceComponent: Registered %s with FogManager (Team=%d, Radius=%.0f, bGrantsVision=%d)"),
			GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"), TeamID, VisionRadius, bGrantsVision ? 1 : 0);
	}
	else
	{
		// Manager not ready yet, retry shortly
		static int32 RetryCounter = 0;
		++RetryCounter;
		UE_LOG(LogCoding, Log, TEXT("VisionSourceComponent: %s cannot find FogManager, scheduling retry #%d"),
			GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"), RetryCounter);
			
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				RegistrationRetryTimer,
				this,
				&UVisionSourceComponent::TryRegisterWithManager,
				0.1f,
				false
			);
		}
	}
}

void UVisionSourceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clear any pending timers
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RegistrationRetryTimer);
	}
	
	// Unregister from fog manager
	if (FogManager)
	{
		FogManager->UnregisterVisionSource(this);
		FogManager = nullptr;
	}
	
	Super::EndPlay(EndPlayReason);
}

void UVisionSourceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	// Currently no per-tick logic needed
}

void UVisionSourceComponent::SetVisionRadius(float NewRadius)
{
	VisionRadius = FMath::Max(0.f, NewRadius);
}

void UVisionSourceComponent::SetTeamID(int32 NewTeamID)
{
	TeamID = NewTeamID;
}

void UVisionSourceComponent::SetGrantsVision(bool bGrants)
{
	bGrantsVision = bGrants;
}
