// Fill out your copyright notice in the Description page of Project Settings.

#include "HealthComponent.h"
#include "PlayerStatsComponent.h"
#include "Logging.h"
#include "Net/UnrealNetwork.h"

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Enable replication for the health component
	SetIsReplicatedByDefault(true);

	// Default values
	MaxHealth = 100.0f;
	Health = MaxHealth;
	HealthRegenRate = 0.0f;
	bIsDead = false;
	TeamID = -1; // Neutral by default
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	
	Health = MaxHealth;
}

void UHealthComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Health regeneration
	if (HealthRegenRate > 0.0f && Health < MaxHealth && !bIsDead)
	{
		float OldHealth = Health;
		Health = FMath::Min(Health + (HealthRegenRate * DeltaTime), MaxHealth);
		
		if (Health != OldHealth)
		{
			OnHealthChanged.Broadcast(Health, MaxHealth, 0.0f);
		}
	}
}

void UHealthComponent::TakeDamage(float Damage, AActor* DamageCauser)
{
	// Only the server should modify health
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogCoding, Warning, TEXT("HealthComponent::TakeDamage called on non-authority owner - ignoring"));
		return;
	}

	if (bIsDead || Damage <= 0.0f)
	{
		return;
	}

	// Attempt to get attacker and target stat components
	UPlayerStatsComponent* AttackerStats = nullptr;
	UPlayerStatsComponent* TargetStats = nullptr;

	if (DamageCauser)
	{
		AttackerStats = DamageCauser->FindComponentByClass<UPlayerStatsComponent>();
	}

	AActor* OwnerActor = GetOwner();
	if (OwnerActor)
	{
		TargetStats = OwnerActor->FindComponentByClass<UPlayerStatsComponent>();
	}

	// Base damage is the incoming Damage value (from ability/projectile/source)
	float BaseDamage = Damage;

	// Attacker stats (may be null)
	float AttackerAD = AttackerStats ? AttackerStats->GetAttackDamage() : 0.0f;
	float AttackerAP = AttackerStats ? AttackerStats->GetAbilityPower() : 0.0f;

	// Prevent double-counting: many attack sources already pass a Damage value
	// equal to the attacker's AD. If the incoming Damage is approximately the
	// same as the attacker's AD, assume it already includes AD and do not add it again.
	float DamageBeforeMitigation = 0.0f;
	if (AttackerStats && FMath::Abs(BaseDamage - AttackerAD) <= 1.0f)
	{
		DamageBeforeMitigation = BaseDamage; // assume Damage already accounts for AD
	}
	else
	{
		DamageBeforeMitigation = BaseDamage + AttackerAD;
	}

	// Crit calculation (simple)
	float CritChance = AttackerStats ? AttackerStats->GetCritChance() : 0.0f;
	bool bIsCrit = FMath::FRand() < CritChance;
	if (bIsCrit)
	{
		DamageBeforeMitigation *= 1.5f; // 150% crit multiplier
	}

	// Armor mitigation (physical). Use target's Armor if present, otherwise 0.
	float TargetArmor = TargetStats ? TargetStats->GetArmor() : 0.0f;
	float MitigationMultiplier = 1.0f;
	if (TargetArmor >= 0.0f)
	{
		MitigationMultiplier = 100.0f / (100.0f + TargetArmor);
	}
	else
	{
		// Negative armor increases damage (simplified)
		MitigationMultiplier = 2.0f - (100.0f / (100.0f - TargetArmor));
	}

	float FinalDamage = DamageBeforeMitigation * MitigationMultiplier;
	FinalDamage = FMath::Max(FinalDamage, 0.0f);

	Health = FMath::Max(Health - FinalDamage, 0.0f);

	UE_LOG(LogCoding, Verbose, TEXT("%s took %.2f dmg (raw %.2f, AD %.2f, crit %d, armor %.1f) H: %.2f/%.2f"),
		*GetOwner()->GetName(), FinalDamage, BaseDamage, AttackerAD, bIsCrit ? 1 : 0, TargetArmor, Health, MaxHealth);

	OnHealthChanged.Broadcast(Health, MaxHealth, FinalDamage);
	// Clients will receive OnHealthChanged via replication callback OnRep_Health

	if (Health <= 0.0f)
	{
		Die(DamageCauser);
	}
}

void UHealthComponent::Heal(float HealAmount)
{
	// Only server should modify health
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(LogCoding, Warning, TEXT("HealthComponent::Heal called on non-authority owner - ignoring"));
		return;
	}

	if (bIsDead || HealAmount <= 0.0f)
	{
		return;
	}

	float OldHealth = Health;
	Health = FMath::Min(Health + HealAmount, MaxHealth);
	
	if (Health != OldHealth)
	{
		OnHealthChanged.Broadcast(Health, MaxHealth, 0.0f);
	}
}

void UHealthComponent::OnRep_Health()
{
	OnHealthChanged.Broadcast(Health, MaxHealth, 0.0f);
}

void UHealthComponent::OnRep_IsDead()
{
	if (bIsDead)
	{
		OnDeath.Broadcast(GetOwner(), nullptr);
	}
}

void UHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UHealthComponent, Health);
	DOREPLIFETIME(UHealthComponent, bIsDead);
}

float UHealthComponent::GetHealthPercent() const
{
	return MaxHealth > 0.0f ? Health / MaxHealth : 0.0f;
}

bool UHealthComponent::IsEnemy(AActor* OtherActor) const
{
	if (!OtherActor)
	{
		return false;
	}

	int32 OtherTeamID = GetActorTeamID(OtherActor);
	
	// Neutral actors (-1) are not enemies of anyone
	if (TeamID == -1 || OtherTeamID == -1)
	{
		return false;
	}

	return TeamID != OtherTeamID;
}

bool UHealthComponent::AreEnemies(AActor* ActorA, AActor* ActorB)
{
	if (!ActorA || !ActorB)
	{
		return false;
	}

	int32 TeamA = GetActorTeamID(ActorA);
	int32 TeamB = GetActorTeamID(ActorB);

	// Neutral actors (-1) are not enemies
	if (TeamA == -1 || TeamB == -1)
	{
		return false;
	}

	return TeamA != TeamB;
}

int32 UHealthComponent::GetActorTeamID(AActor* Actor)
{
	if (!Actor)
	{
		return -1;
	}

	UHealthComponent* HealthComp = Actor->FindComponentByClass<UHealthComponent>();
	if (HealthComp)
	{
		return HealthComp->TeamID;
	}

	return -1;
}

FLinearColor UHealthComponent::GetTeamColor() const
{
	return GetTeamColorFromID(TeamID);
}

FLinearColor UHealthComponent::GetTeamColorFromID(int32 InTeamID)
{
	switch (InTeamID)
	{
		case 0: // Blue team (player)
			return FLinearColor(0.1f, 0.4f, 1.0f, 1.0f); // Bright Blue
		case 1: // Red team (enemies)
			return FLinearColor(1.0f, 0.2f, 0.2f, 1.0f); // Bright Red
		default: // Neutral
			return FLinearColor(1.0f, 1.0f, 1.0f, 1.0f); // White
	}
}

void UHealthComponent::Die(AActor* Killer)
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;
	Health = 0.0f;
	UE_LOG(LogCoding, Display, TEXT("HealthComponent::Die: %s died (Killer=%s)"), *GetOwner()->GetName(), Killer ? *Killer->GetName() : TEXT("NULL"));
	OnDeath.Broadcast(GetOwner(), Killer);
}
