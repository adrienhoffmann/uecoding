// Fill out your copyright notice in the Description page of Project Settings.

#include "EnemyCharacter.h"
#include "HealthComponent.h"
#include "CombatAnimComponent.h"
#include "VisionSourceComponent.h"
#include "PlayerStatsComponent.h"
#include "Projectile.h"
#include "Waypoint.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Logging.h"
#include "MOBACharacter.h"

AEnemyCharacter::AEnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create health component
	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	HealthComponent->MaxHealth = 100.0f;
	HealthComponent->HealthRegenRate = 0.5f;

	// Create combat animation component
	CombatAnimComp = CreateDefaultSubobject<UCombatAnimComponent>(TEXT("CombatAnimComponent"));

	// Use the default skeletal mesh from ACharacter (will be set in Blueprint)
	// No cube mesh - minions will use the mannequin set in BP_Minion
	
	// Ensure capsule responds to visibility traces (for targeting)
	if (GetCapsuleComponent())
	{
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}

	// Ensure mesh responds to visibility for targeting
	if (GetMesh())
	{
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}

	// Combat settings
	AttackRange = 150.0f;
	AttackDamage = 10.0f;
	AttackCooldown = 1.5f;
	LastAttackTime = 0.0f;
	bIsAttacking = false;
	bIsRanged = false;
	GoldReward = 17;  // Default melee gold (17 for melee, 20 for ranged - set in BP)
	XPReward = 60;    // Default XP reward
	ProjectileClass = nullptr;

	// AI settings
	DetectionRange = 1000.0f;
	MoveSpeed = 300.0f;
	CurrentTarget = nullptr;
	CurrentWaypoint = nullptr;

	// Auto possess AI - ensures spawned minions get a controller
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// Setup movement
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;
	}

	// Create a VisionSource so minions/turrets can grant vision
	VisionSource = CreateDefaultSubobject<UVisionSourceComponent>(TEXT("VisionSource"));
	// Set a default radius for minions; ranged minions will override in BeginPlay
	VisionSource->VisionRadius = 800.f;
	VisionSource->TeamID = 0;
}

void AEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	if (HealthComponent)
	{
		HealthComponent->OnDeath.AddDynamic(this, &AEnemyCharacter::OnDeath);
	}

	// Bind to combat animation events
	if (CombatAnimComp)
	{
		CombatAnimComp->OnMeleeHitPoint.AddDynamic(this, &AEnemyCharacter::OnMeleeHit);
		CombatAnimComp->OnRangedReleasePoint.AddDynamic(this, &AEnemyCharacter::OnRangedRelease);
		CombatAnimComp->OnAttackEnd.AddDynamic(this, &AEnemyCharacter::OnAttackEnd);
	}

	// Force collision for targeting - ensures minions can be clicked
	if (GetCapsuleComponent())
	{
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}
	
	// Ensure CharacterMovement is properly set up for spawned minions
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;
		GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);
		GetCharacterMovement()->bOrientRotationToMovement = false; // We handle rotation manually
	}
	
	// Spawn default controller if we don't have one (needed for AddMovementInput)
	if (!GetController())
	{
		SpawnDefaultController();
	}
	
	// Apply team color to mesh
	ApplyTeamColor();

	// Configure vision source based on team and minion type
	if (VisionSource)
	{
		// If we have a HealthComponent, use its TeamID
		if (HealthComponent)
		{
			VisionSource->SetTeamID(HealthComponent->TeamID);
		}

		// Use a different radius for ranged minions
		if (bIsRanged)
		{
			VisionSource->SetVisionRadius(1000.f);
			VisionSource->SetGrantsVision(true);
		}
		else
		{
			VisionSource->SetVisionRadius(700.f);
			VisionSource->SetGrantsVision(true);
		}
	}
	
	UE_LOG(LogCoding, Verbose, TEXT("Minion %s BeginPlay: Waypoint=%s, Team=%d, Speed=%.0f, Ctrl=%d"),
		*GetName(),
		CurrentWaypoint ? *CurrentWaypoint->GetName() : TEXT("NULL"),
		HealthComponent ? HealthComponent->TeamID : -999,
		MoveSpeed,
		GetController() != nullptr);
}

void AEnemyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (HealthComponent && HealthComponent->bIsDead)
	{
		return;
	}

	// Ensure movement mode is Walking (may get reset after spawn)
	if (GetCharacterMovement() && GetCharacterMovement()->MovementMode != EMovementMode::MOVE_Walking)
	{
		if (GetCharacterMovement()->MovementMode == EMovementMode::MOVE_Falling)
		{
			// Wait until we land
		}
		else
		{
			GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);
		}
	}

	// Don't do anything while attacking
	if (bIsAttacking)
	{
		return;
	}

	FindTarget();

	if (CurrentTarget)
	{
		float DistanceToTarget = FVector::Dist(GetActorLocation(), CurrentTarget->GetActorLocation());

		// Debug log
		static float LastTargetLogTime = 0;
		if (GetWorld()->GetTimeSeconds() - LastTargetLogTime > 2.0f)
		{
			UE_LOG(LogCoding, Verbose, TEXT("Minion %s: target %s dist=%.0f"), *GetName(), *CurrentTarget->GetName(), DistanceToTarget);
			LastTargetLogTime = GetWorld()->GetTimeSeconds();
		}

		if (DistanceToTarget <= AttackRange)
		{
			TryAttack();
		}
		else if (DistanceToTarget <= DetectionRange)
		{
			MoveTowardsTarget(DeltaTime);
		}
		else
		{
			CurrentTarget = nullptr;
		}
	}
	else if (CurrentWaypoint)
	{
		// Move towards waypoint if no target
		float DistanceToWaypoint = FVector::Dist(GetActorLocation(), CurrentWaypoint->GetActorLocation());
		
		if (DistanceToWaypoint <= 100.0f)
		{
			// Reached waypoint, move to next
			UE_LOG(LogCoding, Verbose, TEXT("Minion %s: Reached waypoint %s"), *GetName(), *CurrentWaypoint->GetName());
			CurrentWaypoint = CurrentWaypoint->NextWaypoint;
		}
		else
		{
			// Move towards waypoint
			FVector Direction = (CurrentWaypoint->GetActorLocation() - GetActorLocation()).GetSafeNormal();
			Direction.Z = 0;
			
			// Debug log every 2 seconds
			static float LastMoveLogTime = 0;
			if (GetWorld()->GetTimeSeconds() - LastMoveLogTime > 2.0f)
			{
				UCharacterMovementComponent* MoveComp = GetCharacterMovement();
				UE_LOG(LogCoding, Verbose, TEXT("Minion %s: Moving. Mode=%d Ground=%d Speed=%.0f"),
					*GetName(),
					MoveComp ? (int32)MoveComp->MovementMode.GetValue() : -1,
					MoveComp ? MoveComp->IsMovingOnGround() : false,
					MoveComp ? MoveComp->MaxWalkSpeed : 0.0f);
				LastMoveLogTime = GetWorld()->GetTimeSeconds();
			}
			
			// Use AddMovementInput - this triggers walk animation properly
			AddMovementInput(Direction, 1.0f);
			
			// Rotate towards direction
			FRotator NewRotation = Direction.Rotation();
			NewRotation.Pitch = 0.0f;
			NewRotation.Roll = 0.0f;
			SetActorRotation(FMath::RInterpTo(GetActorRotation(), NewRotation, DeltaTime, 10.0f));
		}
	}
	else
	{
		// No waypoint, no target - log once
		static bool bLoggedNoWaypoint = false;
		if (!bLoggedNoWaypoint)
		{
			UE_LOG(LogCoding, Error, TEXT("Minion %s: No waypoint and no target!"), *GetName());
			bLoggedNoWaypoint = true;
		}
	}
}

void AEnemyCharacter::FindTarget()
{
	// Check if current target is still valid
	if (CurrentTarget)
	{
		UHealthComponent* TargetHealth = CurrentTarget->FindComponentByClass<UHealthComponent>();
		float Distance = FVector::Dist(GetActorLocation(), CurrentTarget->GetActorLocation());
		
		// Check if target is dead or out of range
		if ((TargetHealth && TargetHealth->bIsDead) || Distance > DetectionRange)
		{
			CurrentTarget = nullptr;
		}
		else
		{
			return; // Keep current target
		}
	}

	// Find closest enemy target
	float ClosestDistance = DetectionRange;
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

void AEnemyCharacter::MoveTowardsTarget(float DeltaTime)
{
	if (!CurrentTarget)
	{
		return;
	}

	FVector Direction = (CurrentTarget->GetActorLocation() - GetActorLocation()).GetSafeNormal();
	Direction.Z = 0; // Keep movement horizontal
	
	// Use AddMovementInput - this triggers walk animation properly (same as manually placed minions)
	AddMovementInput(Direction, 1.0f);

	// Rotate towards target
	if (!Direction.IsNearlyZero())
	{
		FRotator TargetRotation = Direction.Rotation();
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), TargetRotation, DeltaTime, 10.0f));
	}
}

void AEnemyCharacter::TryAttack()
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

	LastAttackTime = CurrentTime;
	bIsAttacking = true;

	// Play attack animation based on type
	if (CombatAnimComp)
	{
		if (bIsRanged && CombatAnimComp->RangedAttackMontage)
		{
			CombatAnimComp->PlayRangedAttack();
		}
		else if (!bIsRanged && CombatAnimComp->MeleeAttackMontage)
		{
			CombatAnimComp->PlayMeleeAttack();
		}
		else
		{
			// No animation - do damage immediately
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
	}
	else
	{
		// No anim component - do damage immediately
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
}

void AEnemyCharacter::PerformMeleeAttack()
{
	if (!CurrentTarget)
	{
		return;
	}

	UHealthComponent* TargetHealth = CurrentTarget->FindComponentByClass<UHealthComponent>();
	if (TargetHealth)
	{
		TargetHealth->TakeDamage(AttackDamage, this);
	}
}

void AEnemyCharacter::PerformRangedAttack()
{
	if (!CurrentTarget)
	{
		return;
	}

	if (!ProjectileClass)
	{
		// No projectile - fall back to instant damage
		PerformMeleeAttack();
		return;
	}

	// Spawn projectile
	FVector SpawnLocation = GetActorLocation() + GetActorForwardVector() * 50.0f + FVector(0, 0, 50.0f);
	FVector Direction = (CurrentTarget->GetActorLocation() - SpawnLocation).GetSafeNormal();
	FRotator SpawnRotation = Direction.Rotation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AProjectile* NewProjectile = GetWorld()->SpawnActor<AProjectile>(ProjectileClass, SpawnLocation, SpawnRotation, SpawnParams);
	if (NewProjectile)
	{
		// Use homing projectile that tracks the target
		NewProjectile->InitializeWithTarget(CurrentTarget, this);
		NewProjectile->Damage = AttackDamage;
	}
}

void AEnemyCharacter::OnMeleeHit()
{
	// Called from animation notify - deal melee damage now
	PerformMeleeAttack();
}

void AEnemyCharacter::OnRangedRelease()
{
	// Called from animation notify - spawn projectile now
	PerformRangedAttack();
}

void AEnemyCharacter::OnAttackEnd()
{
	// Called from animation notify - attack finished
	bIsAttacking = false;
}

void AEnemyCharacter::OnDeath(AActor* KilledActor, AActor* Killer)
{
	// Give gold to killer (last-hit)
	if (Killer)
	{
		UPlayerStatsComponent* KillerStats = Killer->FindComponentByClass<UPlayerStatsComponent>();
		if (KillerStats)
		{
			KillerStats->AddGold(GoldReward);
			UE_LOG(LogCoding, Log, TEXT("%s killed %s: +%dG"), *Killer->GetName(), *GetName(), GoldReward);
		}

		// Share XP among nearby allied players (including the killer if present)
		float XPShareRadius = 1200.0f; // tunable radius
		TArray<AActor*> Found;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AMOBACharacter::StaticClass(), Found);
		TArray<UPlayerStatsComponent*> Eligible;
		// Determine killer team id if possible
		int32 KillerTeam = -1;
		if (AActor* K = Killer)
		{
			if (UHealthComponent* KH = K->FindComponentByClass<UHealthComponent>())
			{
				KillerTeam = KH->TeamID;
			}
		}

		for (AActor* A : Found)
		{
			if (!A) continue;
			float Dist = FVector::Dist(A->GetActorLocation(), GetActorLocation());
			if (Dist <= XPShareRadius)
			{
				UPlayerStatsComponent* PS = A->FindComponentByClass<UPlayerStatsComponent>();
				UHealthComponent* HC = A->FindComponentByClass<UHealthComponent>();
				if (PS && HC)
				{
					// Only award to allied players of the killer (if killer known), otherwise award to all nearby players
					if (KillerTeam == -1 || HC->TeamID == KillerTeam)
					{
						Eligible.AddUnique(PS);
					}
				}
			}
		}

		if (Eligible.Num() > 0)
		{
			int32 Share = XPReward / Eligible.Num();
			for (UPlayerStatsComponent* PS : Eligible)
			{
				if (PS)
				{
					PS->AddXP(Share);
				}
			}
			UE_LOG(LogCoding, Log, TEXT("%s died: %d XP shared among %d players"), *GetName(), XPReward, Eligible.Num());
		}
		else
		{
			// Fallback: give XP to killer only
			if (Killer)
			{
				UPlayerStatsComponent* KS = Killer->FindComponentByClass<UPlayerStatsComponent>();
				if (KS) KS->AddXP(XPReward);
			}
		}
	}

	// Disable collision and movement
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->DisableMovement();

	// Destroy after 3 seconds
	SetLifeSpan(3.0f);
}

void AEnemyCharacter::ApplyTeamColor()
{
	if (!HealthComponent || !GetMesh())
	{
		return;
	}

	FLinearColor TeamColor = HealthComponent->GetTeamColor();
	
	// Create dynamic material instance and apply team color
	UMaterialInstanceDynamic* DynMaterial = GetMesh()->CreateAndSetMaterialInstanceDynamic(0);
	if (DynMaterial)
	{
		// Try common parameter names for color tinting
		DynMaterial->SetVectorParameterValue(FName("TeamColor"), TeamColor);
		DynMaterial->SetVectorParameterValue(FName("BodyColor"), TeamColor);
		DynMaterial->SetVectorParameterValue(FName("Tint"), TeamColor);
		DynMaterial->SetVectorParameterValue(FName("BaseColor"), TeamColor);
	}
}
