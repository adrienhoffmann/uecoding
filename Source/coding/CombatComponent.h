// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatComponent.generated.h"

class UHealthComponent;
class AProjectile;
class UCombatAnimComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAttackPerformed, AActor*, Target);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CODING_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UCombatComponent();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Combat Stats
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float AttackDamage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float AttackRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float AttackSpeed; // Attacks per second

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	bool bIsRanged;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	TSubclassOf<AProjectile> ProjectileClass;

	// Use animation system for attacks
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	bool bUseAttackAnimations;

	// Current target (replicated)
	UPROPERTY(BlueprintReadOnly, Category = "Combat", ReplicatedUsing=OnRep_CurrentTarget)
	AActor* CurrentTarget;

	// Is currently attacking (animation playing)
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bIsAttacking;

	// Event when attack is performed
	UPROPERTY(BlueprintAssignable, Category = "Combat")
	FOnAttackPerformed OnAttackPerformed;

	// Set attack target
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SetTarget(AActor* NewTarget);

	// Clear current target
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ClearTarget();

	// Check if target is in range
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsTargetInRange() const;

	// Check if can attack (cooldown ready)
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool CanAttack() const;

	// Perform attack on current target
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void Attack();

	// Get distance to target
	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetDistanceToTarget() const;

private:
	float LastAttackTime;
	float AttackCooldown;

	UPROPERTY()
	UCombatAnimComponent* CombatAnimComp;

	void PerformMeleeAttack();
	void PerformRangedAttack();

	// Animation callbacks
	UFUNCTION()
	void OnMeleeHitPointReached();

	UFUNCTION()
	void OnRangedReleasePointReached();

	UFUNCTION()
	void OnAttackAnimationEnded();

	// Replication
	UFUNCTION()
	void OnRep_CurrentTarget();

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
