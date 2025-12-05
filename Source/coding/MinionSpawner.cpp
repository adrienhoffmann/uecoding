// Fill out your copyright notice in the Description page of Project Settings.

#include "MinionSpawner.h"
#include "EnemyCharacter.h"
#include "HealthComponent.h"
#include "Waypoint.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BillboardComponent.h"
#include "Logging.h"

AMinionSpawner::AMinionSpawner()
{
	PrimaryActorTick.bCanEverTick = true;

	// Visual representation
	UStaticMeshComponent* SpawnerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SpawnerMesh"));
	RootComponent = SpawnerMesh;
	
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube"));
	if (CubeMesh.Succeeded())
	{
		SpawnerMesh->SetStaticMesh(CubeMesh.Object);
		SpawnerMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 0.2f));
		SpawnerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Default values
	MeleePerWave = 3;
	RangedPerWave = 3;
	TimeBetweenWaves = 30.0f;
	TimeBetweenMinions = 1.0f;
	TeamID = 0;
	CurrentWave = 0;
	TimeSinceLastWave = 0.0f;
	TimeSinceLastMinion = 0.0f;
	MinionsSpawnedThisWave = 0;
	TotalMinionsPerWave = 0;
	bSpawningWave = false;
	TargetWaypoint = nullptr;
}

void AMinionSpawner::BeginPlay()
{
	Super::BeginPlay();
	
	// Start first wave after a short delay (2 seconds)
	TimeSinceLastWave = TimeBetweenWaves - 2.0f;
	
	UE_LOG(LogCoding, Log, TEXT("MinionSpawner %s: BeginPlay - Melee=%s Ranged=%s Waypoint=%s"), 
		*GetName(),
		MeleeMinionClass ? *MeleeMinionClass->GetName() : TEXT("NULL"),
		RangedMinionClass ? *RangedMinionClass->GetName() : TEXT("NULL"),
		TargetWaypoint ? *TargetWaypoint->GetName() : TEXT("NULL"));
}

void AMinionSpawner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!MeleeMinionClass && !RangedMinionClass)
	{
		return;
	}

	if (bSpawningWave)
	{
		// Spawn minions one by one
		TimeSinceLastMinion += DeltaTime;
		
		if (TimeSinceLastMinion >= TimeBetweenMinions && MinionsSpawnedThisWave < TotalMinionsPerWave)
		{
			SpawnMinion();
			TimeSinceLastMinion = 0.0f;
			MinionsSpawnedThisWave++;

			if (MinionsSpawnedThisWave >= TotalMinionsPerWave)
			{
				bSpawningWave = false;
				UE_LOG(LogCoding, Log, TEXT("MinionSpawner: Wave %d complete"), CurrentWave);
			}
		}
	}
	else
	{
		// Wait for next wave
		TimeSinceLastWave += DeltaTime;
		
		if (TimeSinceLastWave >= TimeBetweenWaves)
		{
			StartNewWave();
		}
	}
}

void AMinionSpawner::StartNewWave()
{
	CurrentWave++;
	bSpawningWave = true;
	MinionsSpawnedThisWave = 0;
	TotalMinionsPerWave = MeleePerWave + RangedPerWave;
	TimeSinceLastWave = 0.0f;
	TimeSinceLastMinion = 0.0f;
	
	UE_LOG(LogCoding, Log, TEXT("MinionSpawner: Starting Wave %d - Total %d"), CurrentWave, TotalMinionsPerWave);
}

TSubclassOf<AEnemyCharacter> AMinionSpawner::GetMinionClassForIndex(int32 Index)
{
	// First spawn melee minions, then ranged
	if (Index < MeleePerWave)
	{
		return MeleeMinionClass;
	}
	else
	{
		return RangedMinionClass;
	}
}

void AMinionSpawner::SpawnMinion()
{
	TSubclassOf<AEnemyCharacter> ClassToSpawn = GetMinionClassForIndex(MinionsSpawnedThisWave);
	
	if (!ClassToSpawn)
	{
		UE_LOG(LogCoding, Error, TEXT("MinionSpawner: No class to spawn for index %d!"), MinionsSpawnedThisWave);
		return;
	}

	// Offset spawn location to avoid collision (spread minions out)
	float SpreadX = (MinionsSpawnedThisWave % 3) * 150.0f - 150.0f;
	float SpreadY = (MinionsSpawnedThisWave / 3) * 150.0f;
	FVector SpawnLocation = GetActorLocation() + FVector(SpreadX, SpreadY, 50.0f);
	FRotator SpawnRotation = GetActorRotation();

	// Use deferred spawn to set properties BEFORE BeginPlay
	AEnemyCharacter* Minion = GetWorld()->SpawnActorDeferred<AEnemyCharacter>(
		ClassToSpawn, 
		FTransform(SpawnRotation, SpawnLocation),
		nullptr,  // Owner
		nullptr,  // Instigator
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn
	);
	
	if (Minion)
	{
		// Set properties BEFORE BeginPlay is called
		Minion->CurrentWaypoint = TargetWaypoint;
		
		UHealthComponent* MinionHealth = Minion->FindComponentByClass<UHealthComponent>();
		if (MinionHealth)
		{
			MinionHealth->TeamID = TeamID;
		}
		
		// Now finish spawning (this calls BeginPlay)
		Minion->FinishSpawning(FTransform(SpawnRotation, SpawnLocation));
		
		UE_LOG(LogCoding, Verbose, TEXT("MinionSpawner: Spawned %s Waypoint=%s Team=%d"),
			*Minion->GetName(),
			Minion->CurrentWaypoint ? *Minion->CurrentWaypoint->GetName() : TEXT("NULL"),
			MinionHealth ? MinionHealth->TeamID : -999);
	}
}
