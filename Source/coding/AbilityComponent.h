// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AbilityComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAbilityUsed, int32, AbilityIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnManaChanged, float, Mana, float, MaxMana);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CODING_API UAbilityComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UAbilityComponent();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Mana system
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mana")
	float Mana;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mana")
	float MaxMana;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mana")
	float ManaRegenRate;

	// Ability cooldowns (4 abilities like MOBAs)
	UPROPERTY(BlueprintReadOnly, Category = "Abilities")
	TArray<float> AbilityCooldowns;

	// Events
	UPROPERTY(BlueprintAssignable, Category = "Abilities")
	FOnAbilityUsed OnAbilityUsed;

	UPROPERTY(BlueprintAssignable, Category = "Mana")
	FOnManaChanged OnManaChanged;

	// Try to use an ability
	UFUNCTION(BlueprintCallable, Category = "Abilities")
	bool TryUseAbility(int32 AbilityIndex, float ManaCost, float Cooldown);

	// Check if ability is ready
	UFUNCTION(BlueprintPure, Category = "Abilities")
	bool IsAbilityReady(int32 AbilityIndex) const;

	// Get remaining cooldown
	UFUNCTION(BlueprintPure, Category = "Abilities")
	float GetAbilityCooldown(int32 AbilityIndex) const;

	// Consume mana
	UFUNCTION(BlueprintCallable, Category = "Mana")
	bool ConsumeMana(float Amount);

	// Get mana percentage
	UFUNCTION(BlueprintPure, Category = "Mana")
	float GetManaPercent() const;

private:
	void UpdateCooldowns(float DeltaTime);
	void RegenerateMana(float DeltaTime);
};
