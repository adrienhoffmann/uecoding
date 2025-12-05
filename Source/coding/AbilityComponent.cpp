// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilityComponent.h"

UAbilityComponent::UAbilityComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	MaxMana = 100.0f;
	Mana = MaxMana;
	ManaRegenRate = 5.0f;

	// Initialize 4 ability slots (Q, W, E, R)
	AbilityCooldowns.Init(0.0f, 4);
}

void UAbilityComponent::BeginPlay()
{
	Super::BeginPlay();
	
	Mana = MaxMana;
}

void UAbilityComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateCooldowns(DeltaTime);
	RegenerateMana(DeltaTime);
}

bool UAbilityComponent::TryUseAbility(int32 AbilityIndex, float ManaCost, float Cooldown)
{
	if (!AbilityCooldowns.IsValidIndex(AbilityIndex))
	{
		return false;
	}

	// Check if ability is on cooldown
	if (AbilityCooldowns[AbilityIndex] > 0.0f)
	{
		return false;
	}

	// Check if enough mana
	if (Mana < ManaCost)
	{
		return false;
	}

	// Use ability
	ConsumeMana(ManaCost);
	AbilityCooldowns[AbilityIndex] = Cooldown;
	OnAbilityUsed.Broadcast(AbilityIndex);

	return true;
}

bool UAbilityComponent::IsAbilityReady(int32 AbilityIndex) const
{
	if (!AbilityCooldowns.IsValidIndex(AbilityIndex))
	{
		return false;
	}

	return AbilityCooldowns[AbilityIndex] <= 0.0f;
}

float UAbilityComponent::GetAbilityCooldown(int32 AbilityIndex) const
{
	if (!AbilityCooldowns.IsValidIndex(AbilityIndex))
	{
		return 0.0f;
	}

	return AbilityCooldowns[AbilityIndex];
}

bool UAbilityComponent::ConsumeMana(float Amount)
{
	if (Mana < Amount)
	{
		return false;
	}

	float OldMana = Mana;
	Mana = FMath::Max(Mana - Amount, 0.0f);

	if (Mana != OldMana)
	{
		OnManaChanged.Broadcast(Mana, MaxMana);
	}

	return true;
}

float UAbilityComponent::GetManaPercent() const
{
	return MaxMana > 0.0f ? Mana / MaxMana : 0.0f;
}

void UAbilityComponent::UpdateCooldowns(float DeltaTime)
{
	for (int32 i = 0; i < AbilityCooldowns.Num(); i++)
	{
		if (AbilityCooldowns[i] > 0.0f)
		{
			AbilityCooldowns[i] = FMath::Max(AbilityCooldowns[i] - DeltaTime, 0.0f);
		}
	}
}

void UAbilityComponent::RegenerateMana(float DeltaTime)
{
	if (ManaRegenRate > 0.0f && Mana < MaxMana)
	{
		float OldMana = Mana;
		Mana = FMath::Min(Mana + (ManaRegenRate * DeltaTime), MaxMana);

		if (Mana != OldMana)
		{
			OnManaChanged.Broadcast(Mana, MaxMana);
		}
	}
}
