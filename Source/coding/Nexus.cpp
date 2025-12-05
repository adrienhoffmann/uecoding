// Fill out your copyright notice in the Description page of Project Settings.

#include "Nexus.h"
#include "HealthComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "VisionSourceComponent.h"

ANexus::ANexus()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create mesh
	NexusMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NexusMesh"));
	RootComponent = NexusMesh;
	
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone"));
	if (ConeMesh.Succeeded())
	{
		NexusMesh->SetStaticMesh(ConeMesh.Object);
		NexusMesh->SetRelativeScale3D(FVector(3.0f, 3.0f, 4.0f));
	}

	// Create health component
	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	HealthComponent->MaxHealth = 5000.0f;
	HealthComponent->HealthRegenRate = 0.0f;

	// Create VisionSource so nexus grants vision
	VisionSource = CreateDefaultSubobject<UVisionSourceComponent>(TEXT("VisionSource"));
	VisionSource->VisionRadius = 2000.f; // large radius for nexus
	VisionSource->TeamID = 0;
}

void ANexus::BeginPlay()
{
	Super::BeginPlay();
	
	if (HealthComponent)
	{
		HealthComponent->OnDeath.AddDynamic(this, &ANexus::OnNexusDestroyed);
	}

	// Configure vision source team
	if (VisionSource && HealthComponent)
	{
		VisionSource->SetTeamID(HealthComponent->TeamID);
		VisionSource->SetVisionRadius(2000.f);
		VisionSource->SetGrantsVision(true);
	}
}

void ANexus::OnNexusDestroyed(AActor* KilledActor, AActor* Killer)
{
	// Game over - Team lost
	FString TeamName = TEXT("Unknown");
	if (HealthComponent)
	{
		TeamName = (HealthComponent->TeamID == 0) ? TEXT("Blue") : TEXT("Red");
	}
	UE_LOG(LogTemp, Warning, TEXT("%s Team Nexus destroyed! Game Over!"), *TeamName);
	
	// You could trigger game over UI here
	// For now, just destroy the nexus
	SetLifeSpan(2.0f);
}
