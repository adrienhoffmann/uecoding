// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Projectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;

UCLASS()
class CODING_API AProjectile : public AActor
{
	GENERATED_BODY()
	
public:	
	AProjectile();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UProjectileMovementComponent* ProjectileMovement;

public:
	// Damage dealt by this projectile
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float Damage;

	// Who shot this projectile
	UPROPERTY(BlueprintReadOnly, Category = "Projectile")
	AActor* ProjectileOwner;

	// Projectile speed
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float Speed;

	// Projectile lifetime
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float Lifetime;

	// Homing (tracking) target
	UPROPERTY(BlueprintReadOnly, Category = "Projectile")
	AActor* HomingTarget;

	// Is this a homing projectile?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	bool bIsHoming;

	// How strongly it tracks the target (higher = tighter tracking)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	float HomingAccelerationMagnitude;

	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, 
	           FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
	               UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, 
	               bool bFromSweep, const FHitResult& SweepResult);

	// Initialize projectile (direction-based, no homing)
	UFUNCTION(BlueprintCallable, Category = "Projectile")
	void Initialize(FVector Direction, AActor* InProjectileOwner);

	// Initialize projectile with homing target
	UFUNCTION(BlueprintCallable, Category = "Projectile")
	void InitializeWithTarget(AActor* Target, AActor* InProjectileOwner);
};
