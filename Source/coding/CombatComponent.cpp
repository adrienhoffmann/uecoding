// Fill out your copyright notice in the Description page of Project Settings.

#include "CombatComponent.h"
#include "CombatAnimComponent.h"
#include "HealthComponent.h"
#include "Projectile.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Logging.h"

UCombatComponent::UCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Default values - typical MOBA melee stats
	AttackDamage = 50.0f;
	AttackRange = 200.0f;
	AttackSpeed = 1.0f;
	bIsRanged = false;
	bUseAttackAnimations = true;
	bIsAttacking = false;
	CurrentTarget = nullptr;
	LastAttackTime = 0.0f;
	AttackCooldown = 1.0f;
	CombatAnimComp = nullptr;
}

void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	
	// Calculate cooldown from attack speed
	AttackCooldown = 1.0f / FMath::Max(AttackSpeed, 0.1f);

	// Find animation component if we use animations
	if (bUseAttackAnimations)
	{
		CombatAnimComp = GetOwner()->FindComponentByClass<UCombatAnimComponent>();
		if (CombatAnimComp)
		{
			// Bind to animation events
			CombatAnimComp->OnMeleeHitPoint.AddDynamic(this, &UCombatComponent::OnMeleeHitPointReached);
			CombatAnimComp->OnRangedReleasePoint.AddDynamic(this, &UCombatComponent::OnRangedReleasePointReached);
			CombatAnimComp->OnAttackEnd.AddDynamic(this, &UCombatComponent::OnAttackAnimationEnded);
		}
	}
}

void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Don't start new attack if already attacking
	if (bIsAttacking)
	{
		return;
	}

	// Auto-attack if we have a target
	if (CurrentTarget)
	{
		// Check if target is still valid
		UHealthComponent* TargetHealth = CurrentTarget->FindComponentByClass<UHealthComponent>();
		if (!TargetHealth || TargetHealth->bIsDead)
		{
			ClearTarget();
			return;
		}

		// Rotate towards target while attacking
		AActor* Owner = GetOwner();
		if (Owner && IsTargetInRange())
		{
			FVector Direction = (CurrentTarget->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal();
			Direction.Z = 0;
			if (!Direction.IsNearlyZero())
			{
				FRotator TargetRotation = Direction.Rotation();
				Owner->SetActorRotation(FMath::RInterpTo(Owner->GetActorRotation(), TargetRotation, DeltaTime, 10.0f));
			}
		}

		// Attack if in range and cooldown ready
		if (IsTargetInRange() && CanAttack())
		{
			Attack();
		}
	}
}

void UCombatComponent::SetTarget(AActor* NewTarget)
{
	if (NewTarget == GetOwner())
	{
		return; // Can't target self
	}

	// Verify target has health
	if (NewTarget)
	{
		UHealthComponent* TargetHealth = NewTarget->FindComponentByClass<UHealthComponent>();
		if (!TargetHealth || TargetHealth->bIsDead)
		{
			return;
		}

		// Check if target is an enemy (different team)
		UHealthComponent* OwnerHealth = GetOwner()->FindComponentByClass<UHealthComponent>();
		if (OwnerHealth && !UHealthComponent::AreEnemies(GetOwner(), NewTarget))
		{
			return; // Can't attack allies
		}
	}

	CurrentTarget = NewTarget;
}

void UCombatComponent::ClearTarget()
{
	CurrentTarget = nullptr;
}

bool UCombatComponent::IsTargetInRange() const
{
	if (!CurrentTarget)
	{
		return false;
	}

	return GetDistanceToTarget() <= AttackRange;
}

bool UCombatComponent::CanAttack() const
{
	if (!CurrentTarget || bIsAttacking)
	{
		return false;
	}

	float CurrentTime = GetWorld()->GetTimeSeconds();
	return (CurrentTime - LastAttackTime) >= AttackCooldown;
}

void UCombatComponent::Attack()
{
	if (!CurrentTarget || !CanAttack())
	{
		return;
	}

	LastAttackTime = GetWorld()->GetTimeSeconds();
	bIsAttacking = true;

	// Play animation if available
	if (bUseAttackAnimations && CombatAnimComp)
	{
		if (bIsRanged)
		{
			CombatAnimComp->PlayRangedAttack();
		}
		else
		{
			CombatAnimComp->PlayMeleeAttack();
		}
	}
	else
	{
		// No animation - deal damage immediately
		if (bIsRanged)
		{
			PerformRangedAttack();
		}
		else
		{
			PerformMeleeAttack();
		}
		bIsAttacking = false;
	}

	OnAttackPerformed.Broadcast(CurrentTarget);
}

float UCombatComponent::GetDistanceToTarget() const
{
	if (!CurrentTarget || !GetOwner())
	{
		return MAX_FLT;
	}

	return FVector::Dist(GetOwner()->GetActorLocation(), CurrentTarget->GetActorLocation());
}

void UCombatComponent::PerformMeleeAttack()
{
	if (!CurrentTarget)
	{
		return;
	}

	UHealthComponent* TargetHealth = CurrentTarget->FindComponentByClass<UHealthComponent>();
	if (TargetHealth)
	{
		TargetHealth->TakeDamage(AttackDamage, GetOwner());
	}
}

void UCombatComponent::PerformRangedAttack()
{
	UE_LOG(LogCoding, Verbose, TEXT("PerformRangedAttack called"));
	
	if (!CurrentTarget)
	{
		UE_LOG(LogCoding, Verbose, TEXT("PerformRangedAttack: No target"));
		return;
	}
	
	if (!ProjectileClass)
	{
		UE_LOG(LogCoding, Warning, TEXT("PerformRangedAttack: No ProjectileClass - fallback to melee"));
		PerformMeleeAttack();
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		UE_LOG(LogCoding, Warning, TEXT("PerformRangedAttack: No owner"));
		return;
	}

	// Spawn projectile in front of the character
	FVector SpawnLocation = Owner->GetActorLocation() + Owner->GetActorForwardVector() * 100.0f + FVector(0, 0, 50.0f);
	FVector Direction = (CurrentTarget->GetActorLocation() - SpawnLocation).GetSafeNormal();
	FRotator SpawnRotation = Direction.Rotation();

	UE_LOG(LogCoding, Verbose, TEXT("Spawning projectile at %s towards %s"), *SpawnLocation.ToString(), *CurrentTarget->GetName());

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Owner;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AProjectile* NewProjectile = GetWorld()->SpawnActor<AProjectile>(ProjectileClass, SpawnLocation, SpawnRotation, SpawnParams);
	if (NewProjectile)
	{
		NewProjectile->Initialize(Direction, Owner);
		NewProjectile->Damage = AttackDamage;
		UE_LOG(LogCoding, Verbose, TEXT("Projectile spawned successfully"));
	}
	else
	{
		UE_LOG(LogCoding, Error, TEXT("Failed to spawn projectile"));
	}
}

// Animation Callbacks
void UCombatComponent::OnMeleeHitPointReached()
{
	// This is called at the exact frame the melee hit should connect
	PerformMeleeAttack();
}

void UCombatComponent::OnRangedReleasePointReached()
{
	// This is called when the projectile should be released
	PerformRangedAttack();
}

void UCombatComponent::OnAttackAnimationEnded()
{
	bIsAttacking = false;
}
