// FogOfWarPostProcess - Implementation
#include "FogOfWarPostProcess.h"
#include "FogOfWarManager.h"
#include "Components/PostProcessComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Logging.h"

// Use LogCoding category from Logging.h
DEFINE_LOG_CATEGORY_STATIC(LogFogPostProcess, Log, All);

UFogOfWarPostProcess::UFogOfWarPostProcess()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.033f; // ~30 FPS updates for material params
}

void UFogOfWarPostProcess::BeginPlay()
{
	Super::BeginPlay();

	// Find the fog manager
	FogManager = AFogOfWarManager::GetInstance(GetWorld());
	if (!FogManager)
	{
		UE_LOG(LogFogPostProcess, Warning, TEXT("FogOfWarPostProcess: No FogOfWarManager found in level"));
	}

	SetupPostProcess();
}

void UFogOfWarPostProcess::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clean up the post-process component
	if (PostProcessComp)
	{
		PostProcessComp->DestroyComponent();
		PostProcessComp = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void UFogOfWarPostProcess::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bEnableFog && FogMaterialInstance)
	{
		UpdateMaterialParameters();
	}
}

void UFogOfWarPostProcess::SetupPostProcess()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		UE_LOG(LogFogPostProcess, Error, TEXT("FogOfWarPostProcess: No owner actor"));
		return;
	}

	// Create post-process component if we don't have one
	PostProcessComp = NewObject<UPostProcessComponent>(Owner, TEXT("FogOfWarPostProcessComp"));
	if (!PostProcessComp)
	{
		UE_LOG(LogFogPostProcess, Error, TEXT("FogOfWarPostProcess: Failed to create PostProcessComponent"));
		return;
	}

	PostProcessComp->RegisterComponent();
	
	// Try to attach to root component if available (not required for unbound post-process)
	if (Owner->GetRootComponent())
	{
		PostProcessComp->AttachToComponent(Owner->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		UE_LOG(LogFogPostProcess, Log, TEXT("FogOfWarPostProcess: Attached to %s's RootComponent"), *Owner->GetName());
	}
	else
	{
		UE_LOG(LogFogPostProcess, Warning, TEXT("FogOfWarPostProcess: Owner %s has no RootComponent - PostProcess will still work (bUnbound=true)"), *Owner->GetName());
	}

	// Configure as unbound (affects entire world) - CRITICAL for visibility
	PostProcessComp->bUnbound = true;
	PostProcessComp->bEnabled = true;
	PostProcessComp->Priority = PostProcessPriority;
	
	UE_LOG(LogFogPostProcess, Log, TEXT("FogOfWarPostProcess: PostProcessComp created - bUnbound=%d, bEnabled=%d, Priority=%f"),
		PostProcessComp->bUnbound, PostProcessComp->bEnabled, PostProcessComp->Priority);

	// Create material instance. Prefer BaseFogMaterial, then SimpleFogMaterial, then default path.
	if (BaseFogMaterial)
	{
		FogMaterialInstance = UMaterialInstanceDynamic::Create(BaseFogMaterial, this);
	}
	else if (SimpleFogMaterial)
	{
		FogMaterialInstance = UMaterialInstanceDynamic::Create(SimpleFogMaterial, this);
	}
	else
	{
		// Try to load a default fog material (editor asset)
		UMaterialInterface* DefaultMat = LoadObject<UMaterialInterface>(nullptr, 
			TEXT("/Game/FogOfWar/M_FogOfWarPostProcess.M_FogOfWarPostProcess"));
        
		if (DefaultMat)
		{
			FogMaterialInstance = UMaterialInstanceDynamic::Create(DefaultMat, this);
		}
		else
		{
			UE_LOG(LogFogPostProcess, Warning, TEXT("FogOfWarPostProcess: No BaseFogMaterial or SimpleFogMaterial set and default not found. "
				"Create a post-process material at /Game/FogOfWar/M_FogOfWarPostProcess or assign SimpleFogMaterial"));
		}
	}

	// Add material to post-process settings
	if (FogMaterialInstance)
	{
		PostProcessComp->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.0f, FogMaterialInstance));

		// Initialize material parameters
		UpdateMaterialParameters();

		// If a simple fog material was provided, ensure we use it explicitly (helps testing)
		if (SimpleFogMaterial)
		{
			UseSimpleMaterial(true);
		}
		
		UE_LOG(LogFogPostProcess, Warning, TEXT("=== FogOfWarPostProcess SETUP COMPLETE ==="));
		UE_LOG(LogFogPostProcess, Warning, TEXT("  Owner: %s (Class: %s)"), *Owner->GetName(), *Owner->GetClass()->GetName());
		UE_LOG(LogFogPostProcess, Warning, TEXT("  PostProcessComp: %p, bUnbound=%d, bEnabled=%d"), 
			PostProcessComp, PostProcessComp->bUnbound, PostProcessComp->bEnabled);
		UE_LOG(LogFogPostProcess, Warning, TEXT("  FogMaterialInstance: %p (Parent: %s)"), 
			FogMaterialInstance, FogMaterialInstance->Parent ? *FogMaterialInstance->Parent->GetName() : TEXT("NULL"));
		UE_LOG(LogFogPostProcess, Warning, TEXT("  WeightedBlendables count: %d"), 
			PostProcessComp->Settings.WeightedBlendables.Array.Num());
		UE_LOG(LogFogPostProcess, Warning, TEXT("========================================"));
	}
	else
	{
		UE_LOG(LogFogPostProcess, Error, TEXT("FogOfWarPostProcess: FogMaterialInstance is NULL - PostProcess will NOT render!"));
	}

	// Set initial enabled state
	if (PostProcessComp)
	{
		PostProcessComp->SetVisibility(bEnableFog);
	}
}

void UFogOfWarPostProcess::UseSimpleMaterial(bool bUseSimple)
{
	if (!PostProcessComp) return;

	// If switching to simple material, recreate the MID using SimpleFogMaterial if available
	if (bUseSimple && SimpleFogMaterial)
	{
		FogMaterialInstance = UMaterialInstanceDynamic::Create(SimpleFogMaterial, this);
	}
	else if (!bUseSimple && BaseFogMaterial)
	{
		FogMaterialInstance = UMaterialInstanceDynamic::Create(BaseFogMaterial, this);
	}

	// Replace blendable
	PostProcessComp->Settings.WeightedBlendables.Array.Empty();
	if (FogMaterialInstance)
	{
		PostProcessComp->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.0f, FogMaterialInstance));
		UpdateMaterialParameters();
	}
}

// Static counter to throttle logs
static int32 UpdateParamLogCounter = 0;

void UFogOfWarPostProcess::UpdateMaterialParameters()
{
	if (!FogMaterialInstance) return;

	// Reduce how often we spam logs from the post-process; keep verbose info less frequent
	bool bShouldLog = (UpdateParamLogCounter++ % 300 == 0); // Log every ~10 seconds approx

	// Update fog texture from manager - use LocalFogTexture (G8 128x128 with bilinear filtering)
	// instead of the canvas render target to get smooth fog transitions without grid artifacts
	if (FogManager)
	{
		UTexture2D* FogTex = FogManager->GetLocalTeamFogTexture();
		if (FogTex)
		{
			FogMaterialInstance->SetTextureParameterValue(TEXT("FogTexture"), FogTex);

			// Pass texture size so material can compute UVs correctly
			int32 TexSizeX = FogTex->GetSizeX();
			int32 TexSizeY = FogTex->GetSizeY();
			FogMaterialInstance->SetVectorParameterValue(TEXT("FogTextureSize"),
				FLinearColor((float)TexSizeX, (float)TexSizeY, 0.0f, 0.0f));

			if (bShouldLog)
			{
				UE_LOG(LogFogPostProcess, Warning, TEXT("  FogTextureSize set -> SizeX=%d SizeY=%d (using LocalFogTexture)"), TexSizeX, TexSizeY);
			}

			if (bShouldLog)
			{
				UE_LOG(LogFogPostProcess, Warning, TEXT("=== FogOfWarPostProcess UpdateParams ==="));
				UE_LOG(LogFogPostProcess, Warning, TEXT("  FogTex: %s (Size: %dx%d)"),
					*FogTex->GetName(), TexSizeX, TexSizeY);
			}
		}
		else if (bShouldLog)
		{
			UE_LOG(LogFogPostProcess, Error, TEXT("FogOfWarPostProcess: LocalFogTexture is NULL!"));
		}

		// Set world bounds and precompute inverse size on CPU to simplify material math
		FVector2D WorldMin = FogManager->WorldBoundsMin;
		FVector2D WorldMax = FogManager->WorldBoundsMax;
		FVector2D WorldSize = WorldMax - WorldMin;

		// Compute UV transform on CPU: UV = WorldXY * UVScale + UVOffset
		FVector2D InvWorldSize(1.f / FMath::Max(WorldSize.X, 1e-6f), 1.f / FMath::Max(WorldSize.Y, 1e-6f));
		FVector2D UVScale = InvWorldSize;
		FVector2D UVOffset = FVector2D(-WorldMin.X * InvWorldSize.X, -WorldMin.Y * InvWorldSize.Y);

		FogMaterialInstance->SetVectorParameterValue(TEXT("UVScale"), FLinearColor(UVScale.X, UVScale.Y, 0, 0));
		FogMaterialInstance->SetVectorParameterValue(TEXT("UVOffset"), FLinearColor(UVOffset.X, UVOffset.Y, 0, 0));

		// For compatibility, still expose WorldSize and WorldBoundsMin (some materials may use them)
		FogMaterialInstance->SetVectorParameterValue(TEXT("WorldSize"), FLinearColor(WorldSize.X, WorldSize.Y, 0, 0));
		FogMaterialInstance->SetVectorParameterValue(TEXT("WorldBoundsMin"), FLinearColor(WorldMin.X, WorldMin.Y, 0, 0));

		// Indicate the material can use simplified math (no divides)
		FogMaterialInstance->SetScalarParameterValue(TEXT("UseSimpleMath"), 1.0f);

		if (bShouldLog)
		{
			UE_LOG(LogFogPostProcess, Warning, TEXT("  WorldBoundsMin: (%.1f, %.1f)"), WorldMin.X, WorldMin.Y);
			UE_LOG(LogFogPostProcess, Warning, TEXT("  WorldBoundsMax: (%.1f, %.1f)"), WorldMax.X, WorldMax.Y);
			UE_LOG(LogFogPostProcess, Warning, TEXT("  WorldSize: (%.1f, %.1f)"), WorldSize.X, WorldSize.Y);
			
			// Log player position to check if it's within bounds
			if (AActor* Owner = GetOwner())
			{
					FVector Pos = Owner->GetActorLocation();
				float U = (Pos.X - WorldMin.X) / WorldSize.X;
				float V = (Pos.Y - WorldMin.Y) / WorldSize.Y;
				UE_LOG(LogFogPostProcess, Warning, TEXT("  PlayerPos: (%.1f, %.1f, %.1f) -> UV: (%.3f, %.3f)"), 
					Pos.X, Pos.Y, Pos.Z, U, V);
				
				if (U < 0 || U > 1 || V < 0 || V > 1)
				{
					UE_LOG(LogFogPostProcess, Error, TEXT("  !!! Player is OUTSIDE fog bounds! UV should be 0-1 !!!"));
				}

					// Debug: sample the local fog grid at the player's world location and log state
					if (FogManager)
					{
						EFogState LocalState = FogManager->GetLocalFogStateAtWorld(Pos);
						int32 Val = 0;
						switch (LocalState)
						{
						case EFogState::Hidden: Val = 0; break;
						case EFogState::Explored: Val = 128; break;
						case EFogState::Visible: Val = 255; break;
						}
						UE_LOG(LogFogPostProcess, Warning, TEXT("  Debug: Local fog state at player = %d (raw=%d)"), (int)LocalState, Val);
						// Pass a debug scalar to the material (0..1) representing the sampled value
						float DebugVal = (float)Val / 255.0f;
						// If the editor requests, force the debug sample to full so it's obvious the post-process is active
						if (bForceDebugSample)
						{
							DebugVal = 1.0f;
							if (bShouldLog)
							{
								UE_LOG(LogFogPostProcess, Warning, TEXT("  Debug: bForceDebugSample is TRUE - forcing DebugFogSampleValue=1.0"));
							}
						}
						FogMaterialInstance->SetScalarParameterValue(TEXT("DebugFogSampleValue"), DebugVal);
					}
			}
		}
	}
	else if (bShouldLog)
	{
		UE_LOG(LogFogPostProcess, Error, TEXT("FogOfWarPostProcess: FogManager is NULL!"));
	}

	// Update appearance parameters
	FogMaterialInstance->SetVectorParameterValue(TEXT("HiddenFogColor"), HiddenFogColor);
	FogMaterialInstance->SetVectorParameterValue(TEXT("ExploredFogColor"), ExploredFogColor);
	FogMaterialInstance->SetScalarParameterValue(TEXT("FogSharpness"), FogSharpness);
	FogMaterialInstance->SetScalarParameterValue(TEXT("HeightFadeStart"), FogHeightFadeStart);
	FogMaterialInstance->SetScalarParameterValue(TEXT("HeightFadeEnd"), FogHeightFadeEnd);
}

void UFogOfWarPostProcess::SetFogEnabled(bool bEnabled)
{
	bEnableFog = bEnabled;
	
	if (PostProcessComp)
	{
		PostProcessComp->SetVisibility(bEnabled);
	}
}

void UFogOfWarPostProcess::SetFogColors(FLinearColor Hidden, FLinearColor Explored)
{
	HiddenFogColor = Hidden;
	ExploredFogColor = Explored;
	
	if (FogMaterialInstance)
	{
		FogMaterialInstance->SetVectorParameterValue(TEXT("HiddenFogColor"), HiddenFogColor);
		FogMaterialInstance->SetVectorParameterValue(TEXT("ExploredFogColor"), ExploredFogColor);
	}
}

void UFogOfWarPostProcess::SetPostProcessUnbound(bool bUnbound)
{
	if (PostProcessComp)
	{
		PostProcessComp->bUnbound = bUnbound;
		UE_LOG(LogFogPostProcess, Log, TEXT("FogOfWarPostProcess: Set PostProcess bUnbound=%d"), bUnbound);
	}
	else
	{
		UE_LOG(LogFogPostProcess, Warning, TEXT("FogOfWarPostProcess: Cannot SetPostProcessUnbound - PostProcessComp is null"));
	}
}

void UFogOfWarPostProcess::AddPostProcessBlendable(UMaterialInterface* Material, float Weight)
{
	if (!Material)
	{
		UE_LOG(LogFogPostProcess, Warning, TEXT("FogOfWarPostProcess: AddPostProcessBlendable called with null Material"));
		return;
	}

	if (!PostProcessComp)
	{
		UE_LOG(LogFogPostProcess, Warning, TEXT("FogOfWarPostProcess: Cannot AddPostProcessBlendable - PostProcessComp is null"));
		return;
	}

	PostProcessComp->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(Weight, Material));
	UE_LOG(LogFogPostProcess, Log, TEXT("FogOfWarPostProcess: Added blendable %s (weight=%f)"), *Material->GetName(), Weight);
}

void UFogOfWarPostProcess::SetFogTexture(UTexture* Texture)
{
	if (!Texture)
	{
		UE_LOG(LogFogPostProcess, Warning, TEXT("FogOfWarPostProcess: SetFogTexture called with null Texture"));
		return;
	}

	// Update the material instance if present
	if (FogMaterialInstance)
	{
		FogMaterialInstance->SetTextureParameterValue(TEXT("FogTexture"), Texture);
		UE_LOG(LogFogPostProcess, Log, TEXT("FogOfWarPostProcess: SetFogTexture -> %s"), *Texture->GetName());
	}
	else
	{
		UE_LOG(LogFogPostProcess, Warning, TEXT("FogOfWarPostProcess: Cannot SetFogTexture - FogMaterialInstance is null"));
	}

	// Also ensure the PostProcess component has the material in its blendables (optionally)
	if (PostProcessComp && FogMaterialInstance)
	{
		// If FogMaterialInstance is not already present in the blendables, add it with full weight
		bool bFound = false;
		for (const FWeightedBlendable& WB : PostProcessComp->Settings.WeightedBlendables.Array)
		{
			if (WB.Object == FogMaterialInstance)
			{
				bFound = true;
				break;
			}
		}

		if (!bFound)
		{
			PostProcessComp->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.0f, FogMaterialInstance));
			UE_LOG(LogFogPostProcess, Log, TEXT("FogOfWarPostProcess: Added FogMaterialInstance to PostProcess blendables"));
		}
	}
}
