// FogOfWarMaterialFunction.h
// Creates a Material Function that can be used directly in PostProcess materials
// Just connect the output to Emissive Color and it works!

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FogOfWarMaterialFunction.generated.h"

class UMaterialFunction;
class UMaterial;

/**
 * Utility class to create the Fog of War Material Function at runtime
 */
UCLASS()
class CODING_API UFogOfWarMaterialFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Creates or gets the FogOfWar Material Function
	 * This function can be placed in any PostProcess material's Emissive Color
	 */
	UFUNCTION(BlueprintCallable, Category = "FogOfWar")
	static UMaterialFunction* GetOrCreateFogOfWarFunction();

	/**
	 * Creates a complete PostProcess material for Fog of War
	 * Returns a ready-to-use material that just needs the FogTexture parameter set
	 */
	UFUNCTION(BlueprintCallable, Category = "FogOfWar")
	static UMaterial* CreateFogOfWarPostProcessMaterial();
};
