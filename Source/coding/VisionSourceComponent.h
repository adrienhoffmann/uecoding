// Vision Source Component - Attached to units that reveal fog of war
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VisionSourceComponent.generated.h"

class AFogOfWarManager;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class CODING_API UVisionSourceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVisionSourceComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Vision radius in world units (like LoL: ~1200 for champions, ~800 for minions)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vision", meta = (ClampMin = "0"))
	float VisionRadius = 1200.f;

	// Team ID (0 = blue, 1 = red, etc.)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vision")
	int32 TeamID = 0;

	// Whether this vision source grants vision (can be toggled, e.g., when dead)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vision")
	bool bGrantsVision = true;

	// If true, vision is blocked by obstacles (line of sight check)
	// Note: Expensive - use sparingly
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vision|Advanced")
	bool bUseLineOfSight = false;

	// Height offset for line of sight origin (eye level)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vision|Advanced")
	float EyeHeightOffset = 100.f;

	// ===== API =====

	UFUNCTION(BlueprintCallable, Category = "Vision")
	float GetVisionRadius() const { return VisionRadius; }

	UFUNCTION(BlueprintCallable, Category = "Vision")
	void SetVisionRadius(float NewRadius);

	UFUNCTION(BlueprintCallable, Category = "Vision")
	int32 GetTeamID() const { return TeamID; }

	UFUNCTION(BlueprintCallable, Category = "Vision")
	void SetTeamID(int32 NewTeamID);

	// Temporarily disable/enable vision (e.g., when dead or invisible)
	UFUNCTION(BlueprintCallable, Category = "Vision")
	void SetGrantsVision(bool bGrants);

	// Check if this component is actively granting vision
	UFUNCTION(BlueprintCallable, Category = "Vision")
	bool IsGrantingVision() const { return bGrantsVision; }

protected:
	// Cached pointer to the fog manager
	UPROPERTY(Transient)
	AFogOfWarManager* FogManager = nullptr;

	// Timer for registration retry (in case manager isn't ready yet)
	FTimerHandle RegistrationRetryTimer;

	void TryRegisterWithManager();
};
