// Fog of War Manager - Server-authoritative, League of Legends style
// Handles vision calculation, team-based visibility, and replication to clients
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "FogOfWarManager.generated.h"

class UVisionSourceComponent;
class UMaterialInstanceDynamic;
class UTexture2D;

// Vision state for each cell in the fog grid
UENUM(BlueprintType)
enum class EFogState : uint8
{
	Hidden = 0,     // Never seen (pitch black)
	Explored = 1,   // Previously seen but not currently visible (dark/grayed)
	Visible = 2     // Currently visible (fully lit)
};

// Compact structure to replicate vision state per team
USTRUCT()
struct FFogTeamData
{
	GENERATED_BODY()

	// Bitfield: 2 bits per cell (Hidden=0, Explored=1, Visible=2)
	// For a 128x128 grid = 128*128*2 bits = 4096 bytes = 4KB per team
	UPROPERTY()
	TArray<uint8> CompressedGrid;

	// Grid dimensions (should match manager)
	UPROPERTY()
	int32 GridSizeX = 0;

	UPROPERTY()
	int32 GridSizeY = 0;

	// Version number for delta updates
	UPROPERTY()
	uint32 Version = 0;
};

UCLASS()
class CODING_API AFogOfWarManager : public AActor
{
	GENERATED_BODY()

public:
	AFogOfWarManager();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ===== Configuration =====

	// Grid resolution (higher = more precise but heavier)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog|Grid", meta = (ClampMin = "32", ClampMax = "512"))
	int32 GridSizeX = 128;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog|Grid", meta = (ClampMin = "32", ClampMax = "512"))
	int32 GridSizeY = 128;

	// World bounds for the fog grid
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog|Grid")
	FVector2D WorldBoundsMin = FVector2D(-12000.f, -12000.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog|Grid")
	FVector2D WorldBoundsMax = FVector2D(12000.f, 12000.f);

	// How often (seconds) the server recalculates vision
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog|Update")
	float ServerUpdateInterval = 0.1f;

	// Number of teams (typically 2 for a MOBA)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog|Teams")
	int32 NumTeams = 2;

	// Render target size for client-side fog texture
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog|Render")
	int32 RenderTargetSize = 512;

	// Optional circle mask texture for smooth edges
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog|Render")
	UTexture2D* CircleMaskTexture = nullptr;

	// Fog opacity for Explored state (0 = fully visible, 1 = fully hidden)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog|Render", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ExploredOpacity = 0.6f;

	// Fog opacity for Hidden state
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog|Render", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HiddenOpacity = 1.0f;

	// ===== API =====

	// Register/Unregister vision sources (called by VisionSourceComponent)
	UFUNCTION(BlueprintCallable, Category = "Fog")
	void RegisterVisionSource(UVisionSourceComponent* Source);

	UFUNCTION(BlueprintCallable, Category = "Fog")
	void UnregisterVisionSource(UVisionSourceComponent* Source);

	// Check if a world position is visible to a specific team
	UFUNCTION(BlueprintCallable, Category = "Fog")
	bool IsLocationVisibleToTeam(const FVector& WorldLocation, int32 TeamID) const;

	// Check if a world position has been explored by a team
	UFUNCTION(BlueprintCallable, Category = "Fog")
	bool IsLocationExploredByTeam(const FVector& WorldLocation, int32 TeamID) const;

	// Get the fog state at a world position for a team
	UFUNCTION(BlueprintCallable, Category = "Fog")
	EFogState GetFogStateAtLocation(const FVector& WorldLocation, int32 TeamID) const;

	// Force immediate vision recalculation (server only)
	UFUNCTION(BlueprintCallable, Category = "Fog", meta = (BlueprintAuthorityOnly))
	void ForceServerUpdate();

	// Get the render target for client-side rendering (minimap, world fog)
	UFUNCTION(BlueprintCallable, Category = "Fog")
	UTextureRenderTarget2D* GetFogRenderTarget() const { return FogRenderTarget; }

	// Get the local player's team fog texture (for rendering)
	UFUNCTION(BlueprintCallable, Category = "Fog")
	UTexture2D* GetLocalTeamFogTexture() const { return LocalFogTexture; }

	// Convert world position to grid coordinates
	UFUNCTION(BlueprintCallable, Category = "Fog")
	FIntPoint WorldToGrid(const FVector& WorldLocation) const;

	// Convert grid coordinates to world position (center of cell)
	UFUNCTION(BlueprintCallable, Category = "Fog")
	FVector GridToWorld(int32 GridX, int32 GridY) const;

	// Return the local client's fog state at a world location (convenience for debug)
	UFUNCTION(BlueprintCallable, Category = "Fog")
	EFogState GetLocalFogStateAtWorld(const FVector& WorldLocation) const;

	// Debug: console-exec helpers
	UFUNCTION(exec)
	void FOW_PrintCellAt(float WorldX, float WorldY, int32 TeamID = 0);

	UFUNCTION(exec)
	void FOW_DumpTeamFog(int32 TeamID = 0);
protected:
	// ===== Server-side data =====

	// Vision grids per team: TeamGrids[TeamID][Y * GridSizeX + X] = EFogState
	TArray<TArray<EFogState>> TeamGrids;

	// Registered vision sources
	TArray<TWeakObjectPtr<UVisionSourceComponent>> VisionSources;

	// Timer for server updates
	FTimerHandle ServerUpdateTimer;

	// ===== Replicated data =====

	// Compressed fog data per team (replicated to clients)
	UPROPERTY(ReplicatedUsing = OnRep_TeamFogData)
	TArray<FFogTeamData> ReplicatedTeamFogData;

	UFUNCTION()
	void OnRep_TeamFogData();

	// ===== Client-side rendering =====

	// Render target for fog display
	UPROPERTY(Transient)
	UCanvasRenderTarget2D* FogRenderTarget = nullptr;

	// Dynamic texture updated from replicated data
	UPROPERTY(Transient)
	UTexture2D* LocalFogTexture = nullptr;

	// Local player's team ID (cached)
	int32 LocalPlayerTeamID = 0;

	// Debug: force filling the render target with a visible white block (temporary)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog|Debug")
	bool bDebugForceFill = true;

	// Debug: perform line-of-sight traces when applying vision (slower, but accurate occlusion)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog|Debug")
	bool bDebugLOS = false;

	// How many grid cells to skip between traces (1 = every cell, 2 = every other cell)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog|Performance", meta = (ClampMin = "1", ClampMax = "8"))
	int32 TraceStride = 1;

	// Vertical offsets for trace start/end (in world units). Start will be Source.Z + StartOffset; End will be Cell.Z + EndOffset
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog|Debug")
	float VisionTraceStartZOffset = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog|Debug")
	float VisionTraceEndZOffset = 50.f;

	// ===== Internal methods =====

	// Server: recalculate vision for all teams
	void ServerUpdateVision();

	// Server: clear current visibility (keep explored)
	void ClearCurrentVisibility();

	// Server: apply vision from a single source
	void ApplyVisionSource(UVisionSourceComponent* Source);

	// Server: mark a circular area as visible for a team
	void RevealCircle(int32 TeamID, const FVector& Center, float Radius);

	// Server: compress grid data for replication
	void CompressGridForTeam(int32 TeamID, FFogTeamData& OutData);

	// Client: decompress and apply received fog data
	void DecompressAndApplyFogData(const FFogTeamData& Data, int32 TeamID);

	// Client: update the fog render target from local grid
	void UpdateFogRenderTarget();

	// Canvas update callback
	UFUNCTION()
	void OnCanvasUpdate(UCanvas* Canvas, int32 Width, int32 Height);

	// Get cell index from grid coordinates
	FORCEINLINE int32 GetCellIndex(int32 X, int32 Y) const { return Y * GridSizeX + X; }

	// Check if grid coordinates are valid
	FORCEINLINE bool IsValidGridCoord(int32 X, int32 Y) const { return X >= 0 && X < GridSizeX && Y >= 0 && Y < GridSizeY; }

	// Local client's decompressed grid (for rendering)
	TArray<EFogState> LocalClientGrid;

public:
	// ===== Singleton access =====
	static AFogOfWarManager* GetInstance(UWorld* World);

private:
	static TWeakObjectPtr<AFogOfWarManager> Instance;
};
