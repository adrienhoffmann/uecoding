// Fog of War Manager - Server-authoritative implementation
#include "FogOfWarManager.h"
#include "VisionSourceComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Engine/Texture2D.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Logging.h"

TWeakObjectPtr<AFogOfWarManager> AFogOfWarManager::Instance = nullptr;

AFogOfWarManager::AFogOfWarManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f; // Client-side render update rate
	
	// This actor needs to replicate
	bReplicates = true;
	bAlwaysRelevant = true;
	
	// NetUpdateFrequency controls how often state is sent to clients
	SetNetUpdateFrequency(10.0f);
}

void AFogOfWarManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	// Replicate fog data to all clients
	DOREPLIFETIME(AFogOfWarManager, ReplicatedTeamFogData);
}

void AFogOfWarManager::BeginPlay()
{
	Super::BeginPlay();
	
	// Register singleton
	Instance = this;
	
	UE_LOG(LogCoding, Log, TEXT("FogOfWarManager: BeginPlay - GridSize=%dx%d, NumTeams=%d, IsServer=%d"),
		GridSizeX, GridSizeY, NumTeams, HasAuthority() ? 1 : 0);
	
	// Initialize team grids (server and client both need local copy for queries)
	TeamGrids.SetNum(NumTeams);
	for (int32 i = 0; i < NumTeams; ++i)
	{
		TeamGrids[i].SetNumZeroed(GridSizeX * GridSizeY);
	}
	
	// Initialize replicated data array
	ReplicatedTeamFogData.SetNum(NumTeams);
	for (int32 i = 0; i < NumTeams; ++i)
	{
		ReplicatedTeamFogData[i].GridSizeX = GridSizeX;
		ReplicatedTeamFogData[i].GridSizeY = GridSizeY;
		ReplicatedTeamFogData[i].Version = 0;
	}
	
	// Initialize local client grid
	LocalClientGrid.SetNumZeroed(GridSizeX * GridSizeY);
	
	// Server: start vision update timer
	if (HasAuthority())
	{
		GetWorld()->GetTimerManager().SetTimer(
			ServerUpdateTimer,
			this,
			&AFogOfWarManager::ServerUpdateVision,
			ServerUpdateInterval,
			true
		);
		UE_LOG(LogCoding, Log, TEXT("FogOfWarManager: Server update timer started (interval=%.2fs)"), ServerUpdateInterval);
	}
	
	// Client: create render target for fog display
	FogRenderTarget = UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(
		GetWorld(),
		UCanvasRenderTarget2D::StaticClass(),
		RenderTargetSize,
		RenderTargetSize
	);
	
	if (FogRenderTarget)
	{
		FogRenderTarget->ClearColor = FLinearColor::Black;
		FogRenderTarget->OnCanvasRenderTargetUpdate.AddDynamic(this, &AFogOfWarManager::OnCanvasUpdate);
		UE_LOG(LogCoding, Log, TEXT("FogOfWarManager: Created render target (%dx%d)"), RenderTargetSize, RenderTargetSize);
	}
	
	// Create dynamic texture for fog (R8 format: 0=hidden, 128=explored, 255=visible)
	LocalFogTexture = UTexture2D::CreateTransient(GridSizeX, GridSizeY, PF_G8);
	if (LocalFogTexture)
	{
		LocalFogTexture->CompressionSettings = TC_Grayscale;
		LocalFogTexture->SRGB = false;
		// Use BILINEAR filtering to smooth transitions between fog cells and remove visible grid seams
		LocalFogTexture->Filter = TF_Bilinear;
		LocalFogTexture->MipGenSettings = TMGS_NoMipmaps;
		LocalFogTexture->AddressX = TA_Clamp;
		LocalFogTexture->AddressY = TA_Clamp;
		LocalFogTexture->UpdateResource();
	}
	
	// Cache local player's team ID from their VisionSourceComponent
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			if (UVisionSourceComponent* VSC = Pawn->FindComponentByClass<UVisionSourceComponent>())
			{
				LocalPlayerTeamID = VSC->TeamID;
				UE_LOG(LogCoding, Log, TEXT("FogOfWarManager: Got LocalPlayerTeamID=%d from player pawn %s"),
					LocalPlayerTeamID, *Pawn->GetName());
			}
			else
			{
				LocalPlayerTeamID = 0;
				UE_LOG(LogCoding, Warning, TEXT("FogOfWarManager: Player pawn %s has no VisionSourceComponent, defaulting to Team 0"),
					*Pawn->GetName());
			}
		}
		else
		{
			// Pawn not yet spawned, schedule retry
			LocalPlayerTeamID = 0;
			UE_LOG(LogCoding, Log, TEXT("FogOfWarManager: Player pawn not yet spawned, will retry for team ID"));
		}
	}
}

void AFogOfWarManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(ServerUpdateTimer);
	
	if (FogRenderTarget)
	{
		FogRenderTarget->OnCanvasRenderTargetUpdate.RemoveAll(this);
	}
	
	if (Instance.Get() == this)
	{
		Instance = nullptr;
	}
	
	Super::EndPlay(EndPlayReason);
}

void AFogOfWarManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// Client: update fog render target periodically
	if (!HasAuthority() || GetNetMode() == NM_Standalone)
	{
		UpdateFogRenderTarget();
	}
}

// ===== Singleton access =====
AFogOfWarManager* AFogOfWarManager::GetInstance(UWorld* World)
{
	if (Instance.IsValid())
	{
		return Instance.Get();
	}
	
	// Find in world
	if (World)
	{
		for (TActorIterator<AFogOfWarManager> It(World); It; ++It)
		{
			Instance = *It;
			return *It;
		}
	}
	
	return nullptr;
}

// ===== Registration =====

void AFogOfWarManager::RegisterVisionSource(UVisionSourceComponent* Source)
{
	if (!Source) return;
	VisionSources.AddUnique(Source);
	UE_LOG(LogCoding, Log, TEXT("FogOfWarManager: Registered vision source from %s (TeamID=%d, Radius=%.0f)"),
		Source->GetOwner() ? *Source->GetOwner()->GetName() : TEXT("Unknown"), Source->TeamID, Source->VisionRadius);
}

void AFogOfWarManager::UnregisterVisionSource(UVisionSourceComponent* Source)
{
	if (!Source) return;
	VisionSources.RemoveSingleSwap(Source);
}

// ===== Coordinate conversion =====

FIntPoint AFogOfWarManager::WorldToGrid(const FVector& WorldLocation) const
{
	float WorldWidth = WorldBoundsMax.X - WorldBoundsMin.X;
	float WorldHeight = WorldBoundsMax.Y - WorldBoundsMin.Y;
	
	float NormX = (WorldLocation.X - WorldBoundsMin.X) / WorldWidth;
	float NormY = (WorldLocation.Y - WorldBoundsMin.Y) / WorldHeight;
	
	int32 GridX = FMath::Clamp(FMath::FloorToInt(NormX * GridSizeX), 0, GridSizeX - 1);
	int32 GridY = FMath::Clamp(FMath::FloorToInt(NormY * GridSizeY), 0, GridSizeY - 1);
	
	return FIntPoint(GridX, GridY);
}

FVector AFogOfWarManager::GridToWorld(int32 GridX, int32 GridY) const
{
	float WorldWidth = WorldBoundsMax.X - WorldBoundsMin.X;
	float WorldHeight = WorldBoundsMax.Y - WorldBoundsMin.Y;
	
	float CellWidth = WorldWidth / GridSizeX;
	float CellHeight = WorldHeight / GridSizeY;
	
	float WorldX = WorldBoundsMin.X + (GridX + 0.5f) * CellWidth;
	float WorldY = WorldBoundsMin.Y + (GridY + 0.5f) * CellHeight;
	
	return FVector(WorldX, WorldY, 0.f);
}

// ===== Vision queries =====

EFogState AFogOfWarManager::GetFogStateAtLocation(const FVector& WorldLocation, int32 TeamID) const
{
	if (TeamID < 0 || TeamID >= TeamGrids.Num())
	{
		return EFogState::Hidden;
	}
	
	FIntPoint Grid = WorldToGrid(WorldLocation);
	int32 Index = GetCellIndex(Grid.X, Grid.Y);
	
	if (Index >= 0 && Index < TeamGrids[TeamID].Num())
	{
		return TeamGrids[TeamID][Index];
	}
	
	return EFogState::Hidden;
}

bool AFogOfWarManager::IsLocationVisibleToTeam(const FVector& WorldLocation, int32 TeamID) const
{
	return GetFogStateAtLocation(WorldLocation, TeamID) == EFogState::Visible;
}

bool AFogOfWarManager::IsLocationExploredByTeam(const FVector& WorldLocation, int32 TeamID) const
{
	EFogState State = GetFogStateAtLocation(WorldLocation, TeamID);
	return State == EFogState::Explored || State == EFogState::Visible;
}

// ===== Server vision calculation =====

void AFogOfWarManager::ForceServerUpdate()
{
	if (HasAuthority())
	{
		ServerUpdateVision();
	}
}

void AFogOfWarManager::ServerUpdateVision()
{
	if (!HasAuthority()) return;
	
	// Step 1: Clear current visibility (Visible -> Explored, keep Hidden as Hidden)
	ClearCurrentVisibility();
	
	// Step 2: Apply vision from all active sources
	int32 AppliedCount = 0;
	for (int32 i = VisionSources.Num() - 1; i >= 0; --i)
	{
		if (!VisionSources[i].IsValid())
		{
			VisionSources.RemoveAt(i);
			continue;
		}
		
		UVisionSourceComponent* Source = VisionSources[i].Get();
		if (Source && Source->IsGrantingVision())
		{
			ApplyVisionSource(Source);
			++AppliedCount;
		}
	}
	
	// Log how many vision sources were applied (every ~30 updates to avoid spam)
	static int32 UpdateCounter = 0;
	if (++UpdateCounter % 30 == 1)
	{
		// Count sources by team
		int32 Team0Count = 0, Team1Count = 0;
		for (int32 i = 0; i < VisionSources.Num(); ++i)
		{
			if (VisionSources[i].IsValid())
			{
				UVisionSourceComponent* S = VisionSources[i].Get();
				if (S && S->IsGrantingVision())
				{
					if (S->TeamID == 0) Team0Count++;
					else if (S->TeamID == 1) Team1Count++;
				}
			}
		}
		UE_LOG(LogCoding, Log, TEXT("FogOfWarManager: ServerUpdateVision - VisionSources.Num=%d, Applied=%d (Team0=%d, Team1=%d)"),
			VisionSources.Num(), AppliedCount, Team0Count, Team1Count);
	}
	
	// Step 3: Compress and replicate to clients
	for (int32 TeamID = 0; TeamID < NumTeams; ++TeamID)
	{
		// Compress into a local temp so assignment to the replicated array is detected by replication
		FFogTeamData Temp;
		CompressGridForTeam(TeamID, Temp);
		// Preserve/advance version atomically
		Temp.Version = ReplicatedTeamFogData.IsValidIndex(TeamID) ? ReplicatedTeamFogData[TeamID].Version + 1 : 1;
		// Assign the whole struct so replication system notices the change
		if (ReplicatedTeamFogData.IsValidIndex(TeamID))
		{
			ReplicatedTeamFogData[TeamID] = Temp;
		}
		else
		{
			ReplicatedTeamFogData.SetNum(TeamID + 1);
			ReplicatedTeamFogData[TeamID] = Temp;
		}

		// Diagnostic: log server-side summary for this team
		int32 visible = 0, explored = 0, hidden = 0;
		for (EFogState s : TeamGrids[TeamID])
		{
			if (s == EFogState::Visible) ++visible;
			else if (s == EFogState::Explored) ++explored;
			else ++hidden;
		}
		UE_LOG(LogCoding, Log, TEXT("FogOfWarManager (Server): Team %d -> Visible=%d Explored=%d Hidden=%d CompressedBytes=%d Version=%u"),
			TeamID, visible, explored, hidden, ReplicatedTeamFogData[TeamID].CompressedGrid.Num(), ReplicatedTeamFogData[TeamID].Version);
	}
	
	// Force net update to replicate changes (for dedicated server -> clients)
	ForceNetUpdate();

	// In Standalone or ListenServer mode, the server IS also the local client.
	// OnRep will NOT be called, so we must manually update LocalClientGrid here.
	ENetMode NetMode = GetNetMode();
	if (NetMode == NM_Standalone || NetMode == NM_ListenServer)
	{
		if (LocalPlayerTeamID >= 0 && LocalPlayerTeamID < TeamGrids.Num())
		{
			LocalClientGrid = TeamGrids[LocalPlayerTeamID];
			UE_LOG(LogCoding, Log, TEXT("FogOfWarManager (Standalone/ListenServer): Updated LocalClientGrid for Team %d directly"), LocalPlayerTeamID);
		}
	}
}

void AFogOfWarManager::ClearCurrentVisibility()
{
	for (int32 TeamID = 0; TeamID < TeamGrids.Num(); ++TeamID)
	{
		TArray<EFogState>& Grid = TeamGrids[TeamID];
		for (int32 i = 0; i < Grid.Num(); ++i)
		{
			if (Grid[i] == EFogState::Visible)
			{
				Grid[i] = EFogState::Explored;
			}
		}
	}
}

void AFogOfWarManager::ApplyVisionSource(UVisionSourceComponent* Source)
{
	if (!Source || !Source->GetOwner()) return;
	
	int32 TeamID = Source->TeamID;
	if (TeamID < 0 || TeamID >= TeamGrids.Num()) return;
	
	FVector WorldPos = Source->GetOwner()->GetActorLocation();
	float Radius = Source->VisionRadius;
	
	// Debug log to trace which sources are being applied
	static int32 ApplyLogCounter = 0;
	if (++ApplyLogCounter % 60 == 1)
	{
		UE_LOG(LogCoding, Log, TEXT("ApplyVisionSource: %s Team=%d Radius=%.0f Pos=(%.0f,%.0f,%.0f)"),
			*Source->GetOwner()->GetName(), TeamID, Radius, WorldPos.X, WorldPos.Y, WorldPos.Z);
	}

	// Perform LOS tracing for occlusion (this is authoritative; use TraceStride to reduce cost)

	// Perform per-cell LOS using line traces from the source to each candidate cell center.
	// Note: this is authoritative and accurate but more expensive. Use TraceStride to reduce cost.
	if (!GetWorld())
	{
		RevealCircle(TeamID, WorldPos, Radius);
		return;
	}

	TArray<EFogState>& Grid = TeamGrids[TeamID];

	// Calculate cell size in world units
	float WorldWidth = WorldBoundsMax.X - WorldBoundsMin.X;
	float WorldHeight = WorldBoundsMax.Y - WorldBoundsMin.Y;
	float CellWidth = WorldWidth / GridSizeX;
	float CellHeight = WorldHeight / GridSizeY;

	// Calculate grid bounds to check
	FIntPoint CenterGrid = WorldToGrid(WorldPos);
	int32 RadiusCellsX = FMath::CeilToInt(Radius / CellWidth) + 1;
	int32 RadiusCellsY = FMath::CeilToInt(Radius / CellHeight) + 1;

	int32 MinX = FMath::Max(0, CenterGrid.X - RadiusCellsX);
	int32 MaxX = FMath::Min(GridSizeX - 1, CenterGrid.X + RadiusCellsX);
	int32 MinY = FMath::Max(0, CenterGrid.Y - RadiusCellsY);
	int32 MaxY = FMath::Min(GridSizeY - 1, CenterGrid.Y + RadiusCellsY);

	float RadiusSq = Radius * Radius;

	// Trace configuration
	FVector SourceActorLoc = Source->GetOwner()->GetActorLocation();
	FVector TraceStartBase = FVector(SourceActorLoc.X, SourceActorLoc.Y, SourceActorLoc.Z + VisionTraceStartZOffset);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(FogOfWarLOS), true);
	Params.AddIgnoredActor(Source->GetOwner());

	int32 TotalTraced = 0;
	int32 TotalBlocked = 0;
	int32 TotalMarkedVisible = 0;

	for (int32 Y = MinY; Y <= MaxY; Y += TraceStride)
	{
		for (int32 X = MinX; X <= MaxX; X += TraceStride)
		{
			FVector CellWorld = GridToWorld(X, Y);
			// Use same vertical plane as source to avoid ground sampling issues; adjust with offset
			FVector TraceEnd = FVector(CellWorld.X, CellWorld.Y, SourceActorLoc.Z + VisionTraceEndZOffset);

			float DistSq = FVector::DistSquaredXY(FVector(SourceActorLoc.X, SourceActorLoc.Y, 0.f), FVector(CellWorld.X, CellWorld.Y, 0.f));
			if (DistSq > RadiusSq) continue;

			int32 Index = GetCellIndex(X, Y);
			if (!IsValidGridCoord(X, Y) || !Grid.IsValidIndex(Index)) continue;

			FHitResult Hit;
			bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, TraceStartBase, TraceEnd, ECC_Visibility, Params);

			++TotalTraced;
			if (!bHit)
			{
				Grid[Index] = EFogState::Visible;
				++TotalMarkedVisible;
			}
			else
			{
				++TotalBlocked;
				// leave as is (previously explored/hidden)
				if (bDebugLOS)
				{
					FString HitName = Hit.GetActor() ? Hit.GetActor()->GetName() : TEXT("(none)");
					UE_LOG(LogCoding, VeryVerbose, TEXT("Fog LOS blocked for cell (%d,%d) by %s"), X, Y, *HitName);
				}
			}

			if (bDebugLOS)
			{
				FColor Col = bHit ? FColor::Red : FColor::Green;
				DrawDebugLine(GetWorld(), TraceStartBase, TraceEnd, Col, false, 0.1f, 0, 1.0f);
			}
		}
	}

	// Log a concise summary (throttled) for visibility of LOS work
	static int32 LOSSummaryCounter = 0;
	if (++LOSSummaryCounter % 60 == 1)
	{
		UE_LOG(LogCoding, Log, TEXT("Fog LOS SourceSummary: Owner=%s Team=%d Radius=%.0f Traced=%d Blocked=%d MarkedVisible=%d Stride=%d"),
			*Source->GetOwner()->GetName(), TeamID, Radius, TotalTraced, TotalBlocked, TotalMarkedVisible, TraceStride);
	}
}

void AFogOfWarManager::RevealCircle(int32 TeamID, const FVector& Center, float Radius)
{
	if (TeamID < 0 || TeamID >= TeamGrids.Num()) return;
	
	TArray<EFogState>& Grid = TeamGrids[TeamID];
	
	// Calculate cell size in world units
	float WorldWidth = WorldBoundsMax.X - WorldBoundsMin.X;
	float WorldHeight = WorldBoundsMax.Y - WorldBoundsMin.Y;
	float CellWidth = WorldWidth / GridSizeX;
	float CellHeight = WorldHeight / GridSizeY;
	
	// Calculate grid bounds to check (optimization: don't check entire grid)
	FIntPoint CenterGrid = WorldToGrid(Center);
	int32 RadiusCellsX = FMath::CeilToInt(Radius / CellWidth) + 1;
	int32 RadiusCellsY = FMath::CeilToInt(Radius / CellHeight) + 1;
	
	int32 MinX = FMath::Max(0, CenterGrid.X - RadiusCellsX);
	int32 MaxX = FMath::Min(GridSizeX - 1, CenterGrid.X + RadiusCellsX);
	int32 MinY = FMath::Max(0, CenterGrid.Y - RadiusCellsY);
	int32 MaxY = FMath::Min(GridSizeY - 1, CenterGrid.Y + RadiusCellsY);
	
	float RadiusSq = Radius * Radius;
	
	for (int32 Y = MinY; Y <= MaxY; ++Y)
	{
		for (int32 X = MinX; X <= MaxX; ++X)
		{
			FVector CellWorld = GridToWorld(X, Y);
			float DistSq = FVector::DistSquaredXY(Center, CellWorld);
			
			if (DistSq <= RadiusSq)
			{
				int32 Index = GetCellIndex(X, Y);
				Grid[Index] = EFogState::Visible;
			}
		}
	}
}

// ===== Compression/Decompression =====

void AFogOfWarManager::CompressGridForTeam(int32 TeamID, FFogTeamData& OutData)
{
	if (TeamID < 0 || TeamID >= TeamGrids.Num()) return;
	
	const TArray<EFogState>& Grid = TeamGrids[TeamID];
	int32 NumCells = Grid.Num();
	
	// 2 bits per cell, 4 cells per byte
	int32 NumBytes = (NumCells + 3) / 4;
	OutData.CompressedGrid.SetNumZeroed(NumBytes);
	OutData.GridSizeX = GridSizeX;
	OutData.GridSizeY = GridSizeY;
	
	for (int32 i = 0; i < NumCells; ++i)
	{
		int32 ByteIndex = i / 4;
		int32 BitOffset = (i % 4) * 2;
		uint8 Value = static_cast<uint8>(Grid[i]) & 0x03;
		OutData.CompressedGrid[ByteIndex] |= (Value << BitOffset);
	}
}

void AFogOfWarManager::DecompressAndApplyFogData(const FFogTeamData& Data, int32 TeamID)
{
	// Ensure Data matches our configured grid
	if (Data.GridSizeX != GridSizeX || Data.GridSizeY != GridSizeY)
	{
		UE_LOG(LogCoding, Warning, TEXT("DecompressAndApplyFogData: data grid size mismatch (data=%dx%d expected=%dx%d)"), Data.GridSizeX, Data.GridSizeY, GridSizeX, GridSizeY);
		return;
	}

	int32 NumCells = GridSizeX * GridSizeY;

	// Quick diagnostic: log received data summary
	UE_LOG(LogCoding, Log, TEXT("FogOfWarManager: DecompressAndApplyFogData START - Team=%d Version=%u CompressedBytes=%d"), TeamID, Data.Version, Data.CompressedGrid.Num());
	if (Data.CompressedGrid.Num() > 0)
	{
		FString HexPreview;
		int32 PreviewBytes = FMath::Min(8, Data.CompressedGrid.Num());
		for (int32 b = 0; b < PreviewBytes; ++b)
		{
			HexPreview += FString::Printf(TEXT("%02X "), Data.CompressedGrid[b]);
		}
		UE_LOG(LogCoding, Verbose, TEXT("FogOfWarManager: CompressedGrid preview: %s"), *HexPreview);
	}

	// Ensure TeamGrids has an entry for this team on clients (server will create normally)
	if (TeamID < 0) return;
	if (TeamGrids.Num() <= TeamID)
	{
		TeamGrids.SetNum(TeamID + 1);
	}

	// Ensure the team's grid array is sized
	if (TeamGrids[TeamID].Num() != NumCells)
	{
		TeamGrids[TeamID].SetNumZeroed(NumCells);
	}

	// Decompress into the team's grid
	TArray<EFogState>& Grid = TeamGrids[TeamID];
	for (int32 i = 0; i < NumCells; ++i)
	{
		int32 ByteIndex = i / 4;
		int32 BitOffset = (i % 4) * 2;

		if (ByteIndex < Data.CompressedGrid.Num())
		{
			uint8 Value = (Data.CompressedGrid[ByteIndex] >> BitOffset) & 0x03;
			Grid[i] = static_cast<EFogState>(Value);
		}
		else
		{
			Grid[i] = EFogState::Hidden;
		}
	}

	// If this is the local player's team, update the LocalClientGrid for rendering
	if (TeamID == LocalPlayerTeamID)
	{
		LocalClientGrid = Grid;

		// Diagnostic: count non-hidden cells and log
		int32 nonHidden = 0;
		for (EFogState s : LocalClientGrid) if (s != EFogState::Hidden) ++nonHidden;
		UE_LOG(LogCoding, Log, TEXT("FogOfWarManager: DecompressAndApplyFogData for LocalTeam=%d - nonHiddenCells=%d"), TeamID, nonHidden);

		// Force an immediate update of the render target/texture so the client sees the change
		UpdateFogRenderTarget();
	}
}

// ===== Replication callback =====

void AFogOfWarManager::OnRep_TeamFogData()
{
	// Client received updated fog data
	UE_LOG(LogCoding, Log, TEXT("FogOfWarManager: OnRep_TeamFogData called, LocalPlayerTeamID=%d, ReplicatedData.Num=%d"),
		LocalPlayerTeamID, ReplicatedTeamFogData.Num());

	// Diagnostic: log versions for all teams we received
	for (int32 i = 0; i < ReplicatedTeamFogData.Num(); ++i)
	{
		const FFogTeamData& D = ReplicatedTeamFogData[i];
		UE_LOG(LogCoding, Verbose, TEXT("  ReplicatedTeamFogData[%d] Version=%u CompressedBytes=%d"), i, D.Version, D.CompressedGrid.Num());
	}
	
	// Decompress for the local player's team
	if (LocalPlayerTeamID >= 0 && LocalPlayerTeamID < ReplicatedTeamFogData.Num())
	{
		DecompressAndApplyFogData(ReplicatedTeamFogData[LocalPlayerTeamID], LocalPlayerTeamID);
		
		// Also update LocalClientGrid for rendering
		if (LocalPlayerTeamID < TeamGrids.Num())
		{
			LocalClientGrid = TeamGrids[LocalPlayerTeamID];
			
			// Count states after update
			int32 visible = 0, explored = 0;
			for (EFogState s : LocalClientGrid)
			{
				if (s == EFogState::Visible) ++visible;
				else if (s == EFogState::Explored) ++explored;
			}
			UE_LOG(LogCoding, Warning, TEXT("FogOfWarManager: OnRep updated LocalClientGrid - Visible=%d, Explored=%d"),
				visible, explored);
            
				// Force immediate render target update so material samples latest texture
				UpdateFogRenderTarget();
		}
	}
	else
	{
		UE_LOG(LogCoding, Warning, TEXT("FogOfWarManager: OnRep_TeamFogData - LocalPlayerTeamID=%d is invalid or no data!"),
			LocalPlayerTeamID);
	}
	
	if (ReplicatedTeamFogData.IsValidIndex(LocalPlayerTeamID))
	{
		const FFogTeamData& D = ReplicatedTeamFogData[LocalPlayerTeamID];
		UE_LOG(LogCoding, Log, TEXT("FogOfWarManager: OnRep_TeamFogData - team=%d Version=%u CompressedBytes=%d Grid=%dx%d"),
			LocalPlayerTeamID, D.Version, D.CompressedGrid.Num(), D.GridSizeX, D.GridSizeY);
	}
	else
	{
		UE_LOG(LogCoding, Log, TEXT("FogOfWarManager: OnRep_TeamFogData - Received fog update but no data for LocalPlayerTeamID=%d"), LocalPlayerTeamID);
	}
}

void AFogOfWarManager::FOW_PrintCellAt(float WorldX, float WorldY, int32 TeamID)
{
	FIntPoint Grid = WorldToGrid(FVector(WorldX, WorldY, 0.f));
	if (!IsValidGridCoord(Grid.X, Grid.Y))
	{
		UE_LOG(LogCoding, Warning, TEXT("FOW_PrintCellAt: world(%f,%f) -> grid(%d,%d) INVALID"), WorldX, WorldY, Grid.X, Grid.Y);
		return;
	}

	if (LocalClientGrid.Num() == GridSizeX * GridSizeY && LocalPlayerTeamID == TeamID)
	{
		EFogState State = LocalClientGrid[GetCellIndex(Grid.X, Grid.Y)];
		UE_LOG(LogCoding, Log, TEXT("FOW_PrintCellAt (client) Grid(%d,%d) Team=%d State=%d"), Grid.X, Grid.Y, TeamID, (uint8)State);
		return;
	}

	if (TeamGrids.IsValidIndex(TeamID) && TeamGrids[TeamID].Num() == GridSizeX * GridSizeY)
	{
		EFogState State = TeamGrids[TeamID][GetCellIndex(Grid.X, Grid.Y)];
		UE_LOG(LogCoding, Log, TEXT("FOW_PrintCellAt (server) Grid(%d,%d) Team=%d State=%d"), Grid.X, Grid.Y, TeamID, (uint8)State);
		return;
	}

	UE_LOG(LogCoding, Warning, TEXT("FOW_PrintCellAt: grid not initialized yet (LocalClientGrid=%d TeamGrids[%d]=%d)"), LocalClientGrid.Num(), TeamID, TeamGrids.IsValidIndex(TeamID) ? TeamGrids[TeamID].Num() : -1);
}

void AFogOfWarManager::FOW_DumpTeamFog(int32 TeamID)
{
	if (!ReplicatedTeamFogData.IsValidIndex(TeamID))
	{
		UE_LOG(LogCoding, Warning, TEXT("FOW_DumpTeamFog: invalid TeamID %d (NumTeams=%d)"), TeamID, ReplicatedTeamFogData.Num());
		return;
	}
	const FFogTeamData& Data = ReplicatedTeamFogData[TeamID];
	UE_LOG(LogCoding, Log, TEXT("FOW_DumpTeamFog Team=%d Version=%u CompressedBytes=%d Grid=%dx%d"),
		   TeamID, Data.Version, Data.CompressedGrid.Num(), Data.GridSizeX, Data.GridSizeY);
}

EFogState AFogOfWarManager::GetLocalFogStateAtWorld(const FVector& WorldLocation) const
{
	if (LocalClientGrid.Num() != GridSizeX * GridSizeY) return EFogState::Hidden;

	FIntPoint G = WorldToGrid(WorldLocation);
	if (!IsValidGridCoord(G.X, G.Y)) return EFogState::Hidden;

	int32 Index = GetCellIndex(G.X, G.Y);
	if (LocalClientGrid.IsValidIndex(Index))
	{
		return LocalClientGrid[Index];
	}

	return EFogState::Hidden;
}

// ===== Client-side rendering =====

void AFogOfWarManager::UpdateFogRenderTarget()
{
	if (FogRenderTarget)
	{
		// Only verbose log for render target updates to avoid spamming
		UE_LOG(LogCoding, Verbose, TEXT("FogOfWarManager: Updating FogRenderTarget (ptr=%p) Size=%dx%d"), FogRenderTarget,
			FogRenderTarget->SizeX, FogRenderTarget->SizeY);

		FogRenderTarget->UpdateResource();
	}
	
	// Also update the fog texture from grid data
	if (LocalFogTexture && LocalClientGrid.Num() == GridSizeX * GridSizeY)
	{
		// Lock texture for writing
		FTexture2DMipMap& Mip = LocalFogTexture->GetPlatformData()->Mips[0];
		uint8* TextureData = static_cast<uint8*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
		
		if (TextureData)
		{
			for (int32 i = 0; i < LocalClientGrid.Num(); ++i)
			{
				switch (LocalClientGrid[i])
				{
				case EFogState::Hidden:
					TextureData[i] = 0;
					break;
				case EFogState::Explored:
					TextureData[i] = 128;
					break;
				case EFogState::Visible:
					TextureData[i] = 255;
					break;
				}
			}
			
			Mip.BulkData.Unlock();
			LocalFogTexture->UpdateResource();

				UE_LOG(LogCoding, Verbose, TEXT("FogOfWarManager: Updated LocalFogTexture, cells=%d"), LocalClientGrid.Num());
		}
	}
}

// Static counter to throttle OnCanvasUpdate logs
static int32 CanvasUpdateLogCounter = 0;

void AFogOfWarManager::OnCanvasUpdate(UCanvas* Canvas, int32 Width, int32 Height)
{
	if (!Canvas) return;

	// Reduce canvas update logging frequency to avoid console spam
	bool bShouldLog = (CanvasUpdateLogCounter++ % 120 == 0); // Log every ~4 seconds

	// Diagnostic: log canvas update and grid status
	if (bShouldLog)
	{
		// Count fog states
		int32 hiddenCount = 0, exploredCount = 0, visibleCount = 0;
		for (EFogState s : LocalClientGrid)
		{
			switch (s)
			{
				case EFogState::Hidden: ++hiddenCount; break;
				case EFogState::Explored: ++exploredCount; break;
				case EFogState::Visible: ++visibleCount; break;
			}
		}
		
		UE_LOG(LogCoding, Verbose, TEXT("=== FogOfWarManager OnCanvasUpdate ==="));
		UE_LOG(LogCoding, Verbose, TEXT("  Canvas: %dx%d, Grid: %dx%d, LocalClientGrid.Num=%d"),
			Width, Height, GridSizeX, GridSizeY, LocalClientGrid.Num());
		UE_LOG(LogCoding, Verbose, TEXT("  FogStates: Hidden=%d, Explored=%d, Visible=%d"),
			hiddenCount, exploredCount, visibleCount);
		UE_LOG(LogCoding, Verbose, TEXT("  LocalPlayerTeamID=%d, bDebugForceFill=%d"),
			LocalPlayerTeamID, bDebugForceFill);
	}
	
	// Use local client grid to draw fog
	// This is similar to old implementation but uses the replicated/decompressed data
	
	// Clear to black (fully fogged)
	Canvas->K2_DrawBox(FVector2D(0, 0), FVector2D(Width, Height), 0.f, FLinearColor::Black);
    
	// Debug: force a visible white block in the render target to verify material sampling
	if (bDebugForceFill)
	{
		FVector2D DebugPos(Width * 0.25f, Height * 0.25f);
		FVector2D DebugSize(Width * 0.5f, Height * 0.5f);
		Canvas->K2_DrawBox(DebugPos, DebugSize, 0.f, FLinearColor::White);
		UE_LOG(LogCoding, Verbose, TEXT("FogOfWarManager: OnCanvasUpdate debug fill drawn (bDebugForceFill=1)"));
		// Still allow normal drawing after debug fill; do not early return
	}

	if (LocalClientGrid.Num() != GridSizeX * GridSizeY) return;
	
	// Calculate cell size in render target pixels
	float CellWidth = static_cast<float>(Width) / GridSizeX;
	float CellHeight = static_cast<float>(Height) / GridSizeY;
	
	// Draw each cell based on fog state
	// Note: Canvas origin is top-left, but our grid uses bottom-left origin (Unreal world Y+).
	// We need to flip Y when drawing to match world coordinates.
	for (int32 Y = 0; Y < GridSizeY; ++Y)
	{
		int32 rowDrawn = 0;
		for (int32 X = 0; X < GridSizeX; ++X)
		{
			int32 Index = GetCellIndex(X, Y);
			EFogState State = LocalClientGrid[Index];
			
			if (State == EFogState::Hidden) continue; // Already black
			
			// Flip Y: draw row Y at canvas position (GridSizeY - 1 - Y)
			// We do NOT flip X; keep width left-to-right consistent with world X
			int32 FlippedY = GridSizeY - 1 - Y;
			int32 FlippedX = X;

			// Compute integer pixel coordinates to avoid 1-pixel seams between adjacent boxes
			int32 PixelX0 = FMath::FloorToInt(FlippedX * CellWidth);
			int32 PixelY0 = FMath::FloorToInt(FlippedY * CellHeight);
			int32 PixelX1 = FMath::CeilToInt((FlippedX + 1) * CellWidth);
			int32 PixelY1 = FMath::CeilToInt((FlippedY + 1) * CellHeight);

			FVector2D Pos(static_cast<float>(PixelX0), static_cast<float>(PixelY0));
			FVector2D Size(static_cast<float>(FMath::Max(1, PixelX1 - PixelX0)),
						  static_cast<float>(FMath::Max(1, PixelY1 - PixelY0)));

			FLinearColor Color = (State == EFogState::Visible)
				? FLinearColor::White
				: FLinearColor(0.4f, 0.4f, 0.4f, 1.0f);

			// Draw filled cell
			Canvas->K2_DrawBox(Pos, Size, 0.f, Color);
			++rowDrawn;
		}
		if (rowDrawn > 0)
		{
			UE_LOG(LogCoding, VeryVerbose, TEXT("FogOfWarManager: OnCanvasUpdate drew %d cells in row %d"), rowDrawn, Y);
		}
	}

	// Summary log: count non-hidden cells drawn
	int32 drawn = 0;
	for (EFogState s : LocalClientGrid) if (s != EFogState::Hidden) ++drawn;
	UE_LOG(LogCoding, Log, TEXT("FogOfWarManager: OnCanvasUpdate completed, drawn cells=%d"), drawn);
}
