// FogOfWarPostProcess - Handles 3D world fog visualization using post-process
// Projects the fog of war texture onto the game world for visual effect
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FogOfWarPostProcess.generated.h"

class UPostProcessComponent;
class UMaterialInstanceDynamic;
class AFogOfWarManager;

/**
 * Component that applies fog of war as a post-process effect on the game world.
 * Attach this to your player camera or use in conjunction with a post-process volume.
 * 
 * The fog samples the FogOfWarManager's render target and darkens
 * areas that are Hidden or Explored based on world position.
 */
UCLASS(ClassGroup=(FogOfWar), meta=(BlueprintSpawnableComponent))
class CODING_API UFogOfWarPostProcess : public UActorComponent
{
	GENERATED_BODY()

public:
	UFogOfWarPostProcess();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ===== Configuration =====

	// Base post-process material (must be a post-process material with fog parameters)
	// If null, the component will attempt to create a default fog effect
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FogOfWar|Material")
	UMaterialInterface* BaseFogMaterial = nullptr;

	// Optional simplified material to use for easier debugging (assign a very small material in editor)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FogOfWar|Material")
	UMaterialInterface* SimpleFogMaterial = nullptr;

	// Fog color for hidden areas (default: dark blue-black like LoL)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FogOfWar|Appearance")
	FLinearColor HiddenFogColor = FLinearColor(0.02f, 0.02f, 0.05f, 1.0f);

	// Fog color for explored areas (default: muted gray-blue)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FogOfWar|Appearance")
	FLinearColor ExploredFogColor = FLinearColor(0.15f, 0.15f, 0.2f, 0.7f);

	// Fog blend sharpness (higher = sharper edge between fog states)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FogOfWar|Appearance", meta = (ClampMin = "1.0", ClampMax = "10.0"))
	float FogSharpness = 3.0f;

	// Height fade parameters - fog fades out above this height (for flying units, etc.)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FogOfWar|Height")
	float FogHeightFadeStart = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FogOfWar|Height")
	float FogHeightFadeEnd = 1500.f;

	// Whether to enable the fog effect
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FogOfWar")
	bool bEnableFog = true;

	// Debug: force the material debug sample to full (useful to verify the post-process is applied)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FogOfWar|Debug")
	bool bForceDebugSample = false;

	// Post-process priority (higher = rendered later)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FogOfWar")
	float PostProcessPriority = 1.0f;

	// ===== API =====

	// Enable or disable the fog effect at runtime
	UFUNCTION(BlueprintCallable, Category = "FogOfWar")
	void SetFogEnabled(bool bEnabled);

	// Update fog appearance at runtime
	UFUNCTION(BlueprintCallable, Category = "FogOfWar")
	void SetFogColors(FLinearColor Hidden, FLinearColor Explored);

	// Get the dynamic material instance
	UFUNCTION(BlueprintCallable, Category = "FogOfWar")
	UMaterialInstanceDynamic* GetFogMaterialInstance() const { return FogMaterialInstance; }

protected:
	// The post-process component we add to the owner
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "FogOfWar", meta=(AllowPrivateAccess="true"))
	UPostProcessComponent* PostProcessComp = nullptr;

	// Dynamic material instance for real-time parameter updates
	UPROPERTY(Transient)
	UMaterialInstanceDynamic* FogMaterialInstance = nullptr;

	// Cached reference to fog manager
	UPROPERTY(Transient)
	AFogOfWarManager* FogManager = nullptr;

	// Initialize the post-process setup
	void SetupPostProcess();

	// Update material parameters from fog manager
	void UpdateMaterialParameters();

	// Switch to the simple material (if set) to simplify debugging/rendering
	UFUNCTION(BlueprintCallable, Category = "FogOfWar")
	void UseSimpleMaterial(bool bUseSimple);

public:
	// Expose helper functions to Blueprints so the PostProcess can be controlled from BP
	UFUNCTION(BlueprintCallable, Category = "FogOfWar")
	void SetPostProcessUnbound(bool bUnbound);

	UFUNCTION(BlueprintCallable, Category = "FogOfWar")
	void AddPostProcessBlendable(UMaterialInterface* Material, float Weight = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "FogOfWar")
	void SetFogTexture(UTexture* Texture);
};
