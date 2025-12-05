// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Turret.generated.h"

class UHealthComponent;
class UStaticMeshComponent;
class AEnemyCharacter;

UCLASS()
class CODING_API ATurret : public AActor
{
	GENERATED_BODY()
	
public:	
	ATurret();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* BaseMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* TurretMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UHealthComponent* HealthComponent;

	// Vision source so turret can grant vision to its team
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vision")
	class UVisionSourceComponent* VisionSource = nullptr;

public:	
	virtual void Tick(float DeltaTime) override;

	// Combat settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float AttackRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float AttackDamage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float AttackCooldown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	TSubclassOf<class AProjectile> ProjectileClass;

private:
	AActor* CurrentTarget;
	float LastAttackTime;

	void FindTarget();
	void RotateTowardsTarget(float DeltaTime);
	void TryAttack();
	void ApplyTeamColor();

	UFUNCTION()
	void OnDeath(AActor* KilledActor, AActor* Killer);
};
