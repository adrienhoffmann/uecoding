// Fill out your copyright notice in the Description page of Project Settings.

#include "Waypoint.h"
#include "Components/StaticMeshComponent.h"

AWaypoint::AWaypoint()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create visual mesh
	WaypointMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WaypointMesh"));
	RootComponent = WaypointMesh;
	
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere"));
	if (SphereMesh.Succeeded())
	{
		WaypointMesh->SetStaticMesh(SphereMesh.Object);
		WaypointMesh->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f));
		WaypointMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	TeamID = 0;  // Blue by default
	bIsBase = false;
	NextWaypoint = nullptr;
}

void AWaypoint::BeginPlay()
{
	Super::BeginPlay();
}
