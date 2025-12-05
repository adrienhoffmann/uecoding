// FogOfWarMaterialFunction.cpp
// Runtime creation of FogOfWar Material Function

#include "FogOfWarMaterialFunction.h"
#include "Materials/MaterialFunction.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionClamp.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionSceneTexture.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionIf.h"
#include "UObject/Package.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Logging.h"

UMaterialFunction* UFogOfWarMaterialFunctionLibrary::GetOrCreateFogOfWarFunction()
{
	// Try to load existing function
	UMaterialFunction* ExistingFunc = LoadObject<UMaterialFunction>(nullptr, 
		TEXT("/Game/FogOfWar/MF_FogOfWarOverlay.MF_FogOfWarOverlay"));
	
	if (ExistingFunc)
	{
		return ExistingFunc;
	}

	UE_LOG(LogCoding, Warning, TEXT("FogOfWar Material Function not found. Please create it in the editor:"));
	UE_LOG(LogCoding, Warning, TEXT("1. Create Material Function: /Game/FogOfWar/MF_FogOfWarOverlay"));
	UE_LOG(LogCoding, Warning, TEXT("2. See instructions below for node setup"));
	
	return nullptr;
}

UMaterial* UFogOfWarMaterialFunctionLibrary::CreateFogOfWarPostProcessMaterial()
{
	// Try to load existing material
	UMaterial* ExistingMat = LoadObject<UMaterial>(nullptr, 
		TEXT("/Game/FogOfWar/M_FogOfWarPostProcess.M_FogOfWarPostProcess"));
	
	if (ExistingMat)
	{
		return ExistingMat;
	}

	UE_LOG(LogCoding, Warning, TEXT("FogOfWar PostProcess Material not found at /Game/FogOfWar/M_FogOfWarPostProcess"));
	return nullptr;
}
