// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MinionSpawner.generated.h"

class AEnemyCharacter;

UCLASS()
class CODING_API AMinionSpawner : public AActor
{
	GENERATED_BODY()
	
public:	
	AMinionSpawner();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

	// Spawn settings - Melee minions
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	TSubclassOf<AEnemyCharacter> MeleeMinionClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	int32 MeleePerWave;

	// Spawn settings - Ranged minions
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	TSubclassOf<AEnemyCharacter> RangedMinionClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	int32 RangedPerWave;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	float TimeBetweenWaves;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	float TimeBetweenMinions;

	// Team ID (0 = Blue, 1 = Red)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	int32 TeamID;

	// Target waypoint (where minions should go)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner")
	class AWaypoint* TargetWaypoint;

	UPROPERTY(BlueprintReadOnly, Category = "Spawner")
	int32 CurrentWave;

private:
	float TimeSinceLastWave;
	float TimeSinceLastMinion;
	int32 MinionsSpawnedThisWave;
	int32 TotalMinionsPerWave;
	bool bSpawningWave;

	void StartNewWave();
	void SpawnMinion();
	TSubclassOf<AEnemyCharacter> GetMinionClassForIndex(int32 Index);
};
