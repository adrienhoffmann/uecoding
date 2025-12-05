// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EnemyCharacter.generated.h"

class UHealthComponent;
class UCombatAnimComponent;
class UAnimMontage;

UCLASS()
class CODING_API AEnemyCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AEnemyCharacter();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UHealthComponent* HealthComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCombatAnimComponent* CombatAnimComp;

public:	
	virtual void Tick(float DeltaTime) override;

	// Attack settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float AttackRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float AttackDamage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float AttackCooldown;

	// Is this a ranged minion?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	bool bIsRanged;

	// Gold reward when killed
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	int32 GoldReward;

	// XP reward when killed
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	int32 XPReward;

	// Projectile class for ranged attacks
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	TSubclassOf<class AProjectile> ProjectileClass;

	// AI settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float DetectionRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float MoveSpeed;

	// Current waypoint to follow
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	class AWaypoint* CurrentWaypoint;

	// Current target
	UPROPERTY(BlueprintReadOnly, Category = "AI")
	AActor* CurrentTarget;

	// Is currently attacking
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bIsAttacking;

protected:
	// Vision source for revealing fog around this unit (minions grant vision)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UVisionSourceComponent* VisionSource = nullptr;

private:
	float LastAttackTime;
	float SpawnTime;
	bool bInitialized;

	void FindTarget();
	void MoveTowardsTarget(float DeltaTime);
	void TryAttack();
	void PerformMeleeAttack();
	void PerformRangedAttack();
	void ApplyTeamColor();

	UFUNCTION()
	void OnDeath(AActor* KilledActor, AActor* Killer);

	UFUNCTION()
	void OnMeleeHit();

	UFUNCTION()
	void OnRangedRelease();

	UFUNCTION()
	void OnAttackEnd();
};
