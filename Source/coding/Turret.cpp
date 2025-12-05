// Fill out your copyright notice in the Description page of Project Settings.

#include "Turret.h"
#include "HealthComponent.h"
#include "EnemyCharacter.h"
#include "Projectile.h"
#include "VisionSourceComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Logging.h"

ATurret::ATurret()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create base mesh
	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
	RootComponent = BaseMesh;
	
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		BaseMesh->SetStaticMesh(CylinderMesh.Object);
		BaseMesh->SetRelativeScale3D(FVector(1.5f, 1.5f, 0.5f));
	}

	// Setup collision for projectiles to hit the turret
	BaseMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	BaseMesh->SetCollisionObjectType(ECC_WorldDynamic);
	BaseMesh->SetCollisionResponseToAllChannels(ECR_Block);
	BaseMesh->SetGenerateOverlapEvents(true);

	// Create turret head mesh
	TurretMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretMesh"));
	TurretMesh->SetupAttachment(BaseMesh);
	
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone"));
	if (ConeMesh.Succeeded())
	{
		TurretMesh->SetStaticMesh(ConeMesh.Object);
		TurretMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));
		TurretMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
		TurretMesh->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.2f));
	}

	// Create health component
	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	HealthComponent->MaxHealth = 500.0f;
	HealthComponent->HealthRegenRate = 0.0f;

	// Create VisionSource so turret grants vision
	VisionSource = CreateDefaultSubobject<UVisionSourceComponent>(TEXT("VisionSource"));
	VisionSource->VisionRadius = 1200.f; // turrets have longer vision
	VisionSource->TeamID = 0;

	// Combat settings
	AttackRange = 1000.0f;
	AttackDamage = 20.0f;
	AttackCooldown = 1.0f;
	LastAttackTime = 0.0f;
	CurrentTarget = nullptr;
}

void ATurret::BeginPlay()
{
	Super::BeginPlay();
	
	if (HealthComponent)
	{
		HealthComponent->OnDeath.AddDynamic(this, &ATurret::OnDeath);
		UE_LOG(LogCoding, Log, TEXT("Turret %s: Health OK H=%.0f/%0.f Team=%d"), 
			*GetName(), HealthComponent->Health, HealthComponent->MaxHealth, HealthComponent->TeamID);
	}
	else
	{
		UE_LOG(LogCoding, Error, TEXT("Turret %s: NO HealthComponent!"), *GetName());
	}
	
	// Apply team color to turret meshes
	ApplyTeamColor();

	// Configure vision source team
	if (VisionSource && HealthComponent)
	{
		VisionSource->SetTeamID(HealthComponent->TeamID);
		VisionSource->SetVisionRadius(1200.f);
		VisionSource->SetGrantsVision(true);
	}
}

void ATurret::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (HealthComponent && HealthComponent->bIsDead)
	{
		return;
	}

	FindTarget();

	if (CurrentTarget)
	{
		RotateTowardsTarget(DeltaTime);
		TryAttack();
	}
}

void ATurret::FindTarget()
{
	// Check if current target is still valid
	if (CurrentTarget)
	{
		float Distance = FVector::Dist(GetActorLocation(), CurrentTarget->GetActorLocation());
		
		// Check if target is out of range or dead
		UHealthComponent* TargetHealth = CurrentTarget->FindComponentByClass<UHealthComponent>();
		if (Distance > AttackRange || (TargetHealth && TargetHealth->bIsDead))
		{
			CurrentTarget = nullptr;
		}
		else
		{
			return; // Keep current target
		}
	}

	// Find closest enemy target
	float ClosestDistance = AttackRange;
	AActor* ClosestTarget = nullptr;

	// Check all actors with HealthComponent
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor == this) continue;

		UHealthComponent* OtherHealth = Actor->FindComponentByClass<UHealthComponent>();
		if (!OtherHealth || OtherHealth->bIsDead) continue;

		// Check if it's an enemy (different team)
		if (!UHealthComponent::AreEnemies(this, Actor)) continue;

		float Distance = FVector::Dist(GetActorLocation(), Actor->GetActorLocation());
		if (Distance <= ClosestDistance)
		{
			ClosestDistance = Distance;
			ClosestTarget = Actor;
		}
	}

	CurrentTarget = ClosestTarget;
}

void ATurret::RotateTowardsTarget(float DeltaTime)
{
	if (!CurrentTarget || !TurretMesh)
	{
		return;
	}

	FVector Direction = (CurrentTarget->GetActorLocation() - GetActorLocation()).GetSafeNormal();
	FRotator TargetRotation = Direction.Rotation();
	TargetRotation.Pitch = 0.0f;
	TargetRotation.Roll = 0.0f;

	FRotator NewRotation = FMath::RInterpTo(TurretMesh->GetComponentRotation(), TargetRotation, DeltaTime, 5.0f);
	TurretMesh->SetWorldRotation(NewRotation);
}

void ATurret::TryAttack()
{
	float CurrentTime = GetWorld()->GetTimeSeconds();
	
	if (CurrentTime - LastAttackTime < AttackCooldown)
	{
		return;
	}

	if (!CurrentTarget)
	{
		return;
	}

	UE_LOG(LogCoding, Verbose, TEXT("Turret: ATTACKING %s"), *CurrentTarget->GetName());

	// Shoot projectile if class is set
	if (ProjectileClass)
	{
		// Spawn projectile higher to avoid ground collision
		FVector SpawnLocation = TurretMesh->GetComponentLocation() + TurretMesh->GetForwardVector() * 100.0f + FVector(0, 0, 50.0f);
		FRotator SpawnRotation = TurretMesh->GetComponentRotation();

		AProjectile* Projectile = GetWorld()->SpawnActor<AProjectile>(ProjectileClass, SpawnLocation, SpawnRotation);
		if (Projectile)
		{
			// Use homing projectile that tracks the target
			Projectile->InitializeWithTarget(CurrentTarget, this);
			Projectile->Damage = AttackDamage;
		}
	}
	else
	{
		// Direct damage if no projectile
		UHealthComponent* TargetHealth = CurrentTarget->FindComponentByClass<UHealthComponent>();
		if (TargetHealth)
		{
			UE_LOG(LogCoding, Verbose, TEXT("Turret: Dealing %.2f dmg"), AttackDamage);
			TargetHealth->TakeDamage(AttackDamage, this);
		}
	}

	LastAttackTime = CurrentTime;
}

void ATurret::OnDeath(AActor* KilledActor, AActor* Killer)
{
	// Disable turret
	SetActorEnableCollision(false);
	
	// Destroy after 5 seconds
	SetLifeSpan(5.0f);
}

void ATurret::ApplyTeamColor()
{
	if (!HealthComponent)
	{
		return;
	}

	FLinearColor TeamColor = HealthComponent->GetTeamColor();
	
	// Apply to base mesh
	if (BaseMesh)
	{
		UMaterialInstanceDynamic* DynMaterial = BaseMesh->CreateAndSetMaterialInstanceDynamic(0);
		if (DynMaterial)
		{
			DynMaterial->SetVectorParameterValue(FName("BaseColor"), TeamColor);
		}
	}
	
	// Apply to turret head mesh
	if (TurretMesh)
	{
		UMaterialInstanceDynamic* DynMaterial = TurretMesh->CreateAndSetMaterialInstanceDynamic(0);
		if (DynMaterial)
		{
			DynMaterial->SetVectorParameterValue(FName("BaseColor"), TeamColor);
		}
	}
}
