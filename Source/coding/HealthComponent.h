// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnHealthChanged, float, Health, float, MaxHealth, float, DamageTaken);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDeath, AActor*, KilledActor, AActor*, Killer);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CODING_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UHealthComponent();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Current health (replicated)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health", ReplicatedUsing=OnRep_Health)
	float Health;

	// Maximum health
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float MaxHealth;

	// Health regeneration per second
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float HealthRegenRate;

	// Is this actor dead? (replicated)
	UPROPERTY(BlueprintReadOnly, Category = "Health", ReplicatedUsing=OnRep_IsDead)
	bool bIsDead;

	// Team ID (0 = Blue, 1 = Red, -1 = Neutral)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team")
	int32 TeamID;

	// Events
	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnDeath OnDeath;

	// Apply damage to this component
	UFUNCTION(BlueprintCallable, Category = "Health")
	void TakeDamage(float Damage, AActor* DamageCauser);

	// Heal this component
	UFUNCTION(BlueprintCallable, Category = "Health")
	void Heal(float HealAmount);

	// Get health percentage (0-1)
	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealthPercent() const;

	// Check if another actor is an enemy (different team)
	UFUNCTION(BlueprintPure, Category = "Team")
	bool IsEnemy(AActor* OtherActor) const;

	// Static helper to check if two actors are enemies
	UFUNCTION(BlueprintPure, Category = "Team")
	static bool AreEnemies(AActor* ActorA, AActor* ActorB);

	// Get TeamID from any actor (returns -1 if no HealthComponent)
	UFUNCTION(BlueprintPure, Category = "Team")
	static int32 GetActorTeamID(AActor* Actor);

	// Get team color (Blue for team 0, Red for team 1, White for neutral)
	UFUNCTION(BlueprintPure, Category = "Team")
	FLinearColor GetTeamColor() const;

	// Static helper to get team color from TeamID
	UFUNCTION(BlueprintPure, Category = "Team")
	static FLinearColor GetTeamColorFromID(int32 InTeamID);

private:
	void Die(AActor* Killer);

	// Replication callbacks for clients
	UFUNCTION()
	void OnRep_Health();

	UFUNCTION()
	void OnRep_IsDead();

public:
	// Replication
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
