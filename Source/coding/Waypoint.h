// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Waypoint.generated.h"

UCLASS()
class CODING_API AWaypoint : public AActor
{
	GENERATED_BODY()
	
public:	
	AWaypoint();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* WaypointMesh;

public:
	// Next waypoint in the path
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waypoint")
	AWaypoint* NextWaypoint;

	// Team ID (0 = Blue, 1 = Red)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waypoint")
	int32 TeamID;

	// Is this the final waypoint (enemy base)?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waypoint")
	bool bIsBase;
};
