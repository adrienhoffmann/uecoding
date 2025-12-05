// FogVisibilityComponent - Handles actor visibility based on fog of war
// Attached to actors that should be hidden when not in enemy vision
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FogVisibilityComponent.generated.h"

class AFogOfWarManager;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class CODING_API UFogVisibilityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFogVisibilityComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Team ID of this actor (used to determine which team's fog to check against)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FogVisibility")
	int32 TeamID = 0;

	// If true, this actor is hidden from enemies when not in their vision
	// Server-side: controls replication relevancy
	// Client-side: controls rendering visibility
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FogVisibility")
	bool bHideWhenNotVisible = true;

	// If true, actor is always visible to its own team regardless of fog
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FogVisibility")
	bool bAlwaysVisibleToOwnTeam = true;

	// Current visibility state (replicated for client rendering)
	UPROPERTY(BlueprintReadOnly, Category = "FogVisibility")
	bool bCurrentlyVisible = true;

	// ===== API =====

	// Check if this actor should be visible to a specific team
	UFUNCTION(BlueprintCallable, Category = "FogVisibility")
	bool IsVisibleToTeam(int32 ObserverTeamID) const;

	// Force visibility update
	UFUNCTION(BlueprintCallable, Category = "FogVisibility")
	void UpdateVisibility();

protected:
	UPROPERTY(Transient)
	AFogOfWarManager* FogManager = nullptr;

	// How often to check visibility (optimization)
	float VisibilityCheckInterval = 0.1f;
	float TimeSinceLastCheck = 0.f;

	// Cache the local player's team for client-side visibility
	int32 LocalPlayerTeamID = -1;

	void CacheLocalPlayerTeam();
};
