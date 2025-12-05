// Fill out your copyright notice in the Description page of Project Settings.

#include "Projectile.h"
#include "HealthComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Logging.h"

AProjectile::AProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create collision component - Query only, no physics blocking
	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	CollisionComponent->InitSphereRadius(30.0f);  // Larger radius for better detection
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Overlap);  // Overlap everything, block nothing
	CollisionComponent->SetGenerateOverlapEvents(true);
	RootComponent = CollisionComponent;

	// Create mesh component
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	// Use a sphere mesh by default for visibility
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere"));
	if (SphereMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(SphereMesh.Object);
		MeshComponent->SetRelativeScale3D(FVector(0.3f, 0.3f, 0.3f));
	}

	// Create projectile movement component
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionComponent;
	ProjectileMovement->InitialSpeed = 3000.0f;
	ProjectileMovement->MaxSpeed = 5000.0f;  // Allow faster speed for homing acceleration
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->ProjectileGravityScale = 0.0f;  // No gravity by default for homing

	// Default values
	Damage = 10.0f;
	Speed = 1000.0f;
	Lifetime = 5.0f;  // Longer lifetime for slower projectiles
	InitialLifeSpan = Lifetime;

	// Homing defaults
	HomingTarget = nullptr;
	bIsHoming = true;  // Default to homing for turret/minion projectiles
	HomingAccelerationMagnitude = 50000.0f;  // Very strong tracking - impossible to dodge
}

void AProjectile::BeginPlay()
{
	Super::BeginPlay();
	
	// Use only overlap for detection - no blocking physics
	CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &AProjectile::OnOverlap);
	SetLifeSpan(Lifetime);
	
	UE_LOG(LogCoding, Verbose, TEXT("Projectile spawned at %s"), *GetActorLocation().ToString());
}

void AProjectile::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, 
                        FVector NormalImpulse, const FHitResult& Hit)
{
	// Don't hit ourselves or our owner
	if (OtherActor == this || OtherActor == ProjectileOwner)
	{
		return;
	}

	// Ignore other projectiles
	if (OtherActor && OtherActor->IsA(AProjectile::StaticClass()))
	{
		return;
	}

	// Only damage actors with HealthComponent that are enemies
	UHealthComponent* HealthComp = OtherActor ? OtherActor->FindComponentByClass<UHealthComponent>() : nullptr;
	if (HealthComp)
	{
		// Check if target is an enemy of the projectile owner
		if (UHealthComponent::AreEnemies(ProjectileOwner, OtherActor))
		{
			UE_LOG(LogCoding, Verbose, TEXT("Projectile HIT enemy %s"), *OtherActor->GetName());
			HealthComp->TakeDamage(Damage, ProjectileOwner);
			UE_LOG(LogCoding, Verbose, TEXT("Projectile dealt %.2f dmg to %s"), Damage, *OtherActor->GetName());
			Destroy();
		}
		// Else: it's an ally, ignore and keep flying
	}
	// Ignore static mesh actors (world geometry) - don't destroy, keep flying
}

void AProjectile::OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
                            UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, 
                            bool bFromSweep, const FHitResult& SweepResult)
{
	// Don't hit ourselves or our owner
	if (OtherActor == this || OtherActor == ProjectileOwner)
	{
		return;
	}

	// Ignore other projectiles
	if (OtherActor->IsA(AProjectile::StaticClass()))
	{
		return;
	}

	// Apply damage if the hit actor has a health component AND is an enemy
	UHealthComponent* HealthComp = OtherActor->FindComponentByClass<UHealthComponent>();
	if (HealthComp)
	{
		// Check if target is an enemy of the projectile owner
		if (UHealthComponent::AreEnemies(ProjectileOwner, OtherActor))
		{
			UE_LOG(LogCoding, Verbose, TEXT("Projectile overlapped enemy %s"), *OtherActor->GetName());
			HealthComp->TakeDamage(Damage, ProjectileOwner);
			UE_LOG(LogCoding, Verbose, TEXT("Projectile hit %s for %.2f dmg"), *OtherActor->GetName(), Damage);
			Destroy();
		}
		// Else: it's an ally, ignore and keep flying
	}
}

void AProjectile::Initialize(FVector Direction, AActor* InProjectileOwner)
{
	ProjectileOwner = InProjectileOwner;
	
	if (ProjectileMovement)
	{
		ProjectileMovement->Velocity = Direction * Speed;
		
		// Disable homing for direction-based projectiles
		ProjectileMovement->bIsHomingProjectile = false;
	}
}

void AProjectile::InitializeWithTarget(AActor* Target, AActor* InProjectileOwner)
{
	ProjectileOwner = InProjectileOwner;
	HomingTarget = Target;
	
	UE_LOG(LogCoding, Verbose, TEXT("Projectile init - Owner: %s, Target: %s"), 
		InProjectileOwner ? *InProjectileOwner->GetName() : TEXT("None"),
		Target ? *Target->GetName() : TEXT("None"));
	
	if (ProjectileMovement && Target)
	{
		// Use Speed from Blueprint if set, otherwise use component's InitialSpeed
		float ActualSpeed = (Speed > 0) ? Speed : ProjectileMovement->InitialSpeed;
		
		// Set initial velocity towards target
		FVector Direction = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal();
		ProjectileMovement->Velocity = Direction * ActualSpeed;
		
		// Enable homing if bIsHoming is true
		if (bIsHoming)
		{
			ProjectileMovement->bIsHomingProjectile = true;
			
			// Only override HomingAccelerationMagnitude if our variable is set
			// Otherwise use whatever is set in the Blueprint's ProjectileMovement component
			if (HomingAccelerationMagnitude > 0)
			{
				ProjectileMovement->HomingAccelerationMagnitude = HomingAccelerationMagnitude;
			}
			
			ProjectileMovement->HomingTargetComponent = Target->GetRootComponent();
			
			UE_LOG(LogCoding, Verbose, TEXT("Projectile homing accel: %.0f"), 
				ProjectileMovement->HomingAccelerationMagnitude);
		}
	}
}
