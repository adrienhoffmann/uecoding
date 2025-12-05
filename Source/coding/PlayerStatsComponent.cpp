// Fill out your copyright notice in the Description page of Project Settings.

#include "PlayerStatsComponent.h"
#include "Logging.h"

UPlayerStatsComponent::UPlayerStatsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Enable replication
	SetIsReplicatedByDefault(true);

	// Starting values
	Gold = 500;  // Starting gold like in LoL
	GoldPerSecond = 1;  // 1 gold per second passive
	GoldAccumulator = 0.0f;
	PassiveGoldDelay = 50.0f;  // 50 seconds before passive gold starts
	GameStartTime = 0.0f;

	CurrentXP = 0;
	CurrentLevel = 1;
	XPToNextLevel = 280;  // XP needed for level 2

	Kills = 0;
	Deaths = 0;
	Assists = 0;

	// Default detailed stats
	AttackDamage = 40.f;
	AbilityPower = 10.f;
	Armor = 30.f;
	MagicResist = 30.f;
	AttackSpeed = 0.7f; // attacks per second baseline
	CritChance = 0.5f; // 50% base crit for testing

	// Level 2 test bonus (tweakable) and init flag
	Level2DamageBonus = 30.0f;
	bLevel2BonusApplied = false;

	// Default per-level growth (tweak in editor if desired)
	AttackDamagePerLevel = 2.0f;
	AbilityPowerPerLevel = 2.0f;
	ArmorPerLevel = 2.5f;
	MagicResistPerLevel = 1.5f;
	AttackSpeedPerLevel = 0.02f;
	CritChancePerLevel = 0.005f; // 0.5%
}

void UPlayerStatsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Gold & economy
	DOREPLIFETIME(UPlayerStatsComponent, Gold);

	// XP & Level
	DOREPLIFETIME(UPlayerStatsComponent, CurrentXP);
	DOREPLIFETIME(UPlayerStatsComponent, CurrentLevel);
	DOREPLIFETIME(UPlayerStatsComponent, XPToNextLevel);

	// KDA
	DOREPLIFETIME(UPlayerStatsComponent, Kills);
	DOREPLIFETIME(UPlayerStatsComponent, Deaths);
	DOREPLIFETIME(UPlayerStatsComponent, Assists);

	// Detailed stats
	DOREPLIFETIME(UPlayerStatsComponent, AttackDamage);
	DOREPLIFETIME(UPlayerStatsComponent, AbilityPower);
	DOREPLIFETIME(UPlayerStatsComponent, Armor);
	DOREPLIFETIME(UPlayerStatsComponent, MagicResist);
	DOREPLIFETIME(UPlayerStatsComponent, AttackSpeed);
	DOREPLIFETIME(UPlayerStatsComponent, CritChance);
}

// ========== OnRep callbacks ==========
void UPlayerStatsComponent::OnRep_Gold()
{
	// Keep replication callbacks light. Use multicast/ForceNotifyStatsChanged
	// from server-side logic to trigger a single client-side broadcast.
	OnGoldChanged.Broadcast(Gold, 0); // Delta unknown on client side
}

void UPlayerStatsComponent::OnRep_XP()
{
	OnXPChanged.Broadcast(CurrentXP, CurrentLevel, 0);
}

void UPlayerStatsComponent::OnRep_Level()
{
	OnLevelUp.Broadcast(CurrentLevel);
	// Do not double-broadcast OnStatsChanged here; prefer server multicast.
}

void UPlayerStatsComponent::OnRep_Stats()
{
	// OnRep for detailed stats kept for backward compatibility; prefer
	// server-side multicast to trigger `OnStatsChanged` in a single place.
}

void UPlayerStatsComponent::ForceNotifyStatsChanged()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		// Server: directly multicast to all clients
		Multicast_ForceNotifyStatsChanged();
	}
	else
	{
		// Client: request the server to multicast
		Server_ForceNotifyStatsChanged();
	}
}

bool UPlayerStatsComponent::Server_ForceNotifyStatsChanged_Validate()
{
	return true;
}

void UPlayerStatsComponent::Server_ForceNotifyStatsChanged_Implementation()
{
	Multicast_ForceNotifyStatsChanged();
}

void UPlayerStatsComponent::Multicast_ForceNotifyStatsChanged_Implementation()
{
	OnStatsChanged.Broadcast();
}

void UPlayerStatsComponent::BeginPlay()
{
	Super::BeginPlay();
	GameStartTime = GetWorld()->GetTimeSeconds();
}

void UPlayerStatsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Passive gold income (only after delay)
	float TimeSinceStart = GetWorld()->GetTimeSeconds() - GameStartTime;
	if (GoldPerSecond > 0 && TimeSinceStart >= PassiveGoldDelay)
	{
		GoldAccumulator += DeltaTime * GoldPerSecond;
		if (GoldAccumulator >= 1.0f)
		{
			int32 GoldToAdd = FMath::FloorToInt(GoldAccumulator);
			GoldAccumulator -= GoldToAdd;
			Gold += GoldToAdd;
			// Silent add - no broadcast for passive gold to avoid spam
		}
	}
}

void UPlayerStatsComponent::AddGold(int32 Amount)
{
	if (Amount <= 0) return;

	// Route to server if we're a client
	if (!GetOwner()->HasAuthority())
	{
		Server_AddGold(Amount);
		return;
	}

	// Server-side: apply the change
	Gold += Amount;
	OnGoldChanged.Broadcast(Gold, Amount);
	// Notify all clients for UI refresh in one place
	Multicast_ForceNotifyStatsChanged();
    
	UE_LOG(LogCoding, Log, TEXT("Gold +%d (Total: %d)"), Amount, Gold);
}

bool UPlayerStatsComponent::SpendGold(int32 Amount)
{
	if (Amount <= 0 || Gold < Amount)
	{
		return false;
	}

	// Route to server if we're a client
	if (!GetOwner()->HasAuthority())
	{
		Server_SpendGold(Amount);
		// Optimistic: assume it will succeed (server will correct if not)
		return true;
	}

	// Server-side: apply the change
	Gold -= Amount;
	OnGoldChanged.Broadcast(Gold, -Amount);
	// Notify all clients for UI refresh
	Multicast_ForceNotifyStatsChanged();
    
	UE_LOG(LogCoding, Log, TEXT("Gold -%d (Total: %d)"), Amount, Gold);
	return true;
}

void UPlayerStatsComponent::AddXP(int32 Amount)
{
	if (Amount <= 0) return;

	// Route to server if we're a client
	if (!GetOwner()->HasAuthority())
	{
		Server_AddXP(Amount);
		return;
	}

	// Server-side: apply the change
	CurrentXP += Amount;
	OnXPChanged.Broadcast(CurrentXP, CurrentLevel, Amount);
    
	UE_LOG(LogCoding, Log, TEXT("XP +%d (Total: %d/%d)"), Amount, CurrentXP, XPToNextLevel);

	// Notify clients for UI refresh
	Multicast_ForceNotifyStatsChanged();

	CheckLevelUp();
}

float UPlayerStatsComponent::GetXPPercent() const
{
	if (XPToNextLevel <= 0) return 1.0f;
	return (float)CurrentXP / (float)XPToNextLevel;
}

void UPlayerStatsComponent::CheckLevelUp()
{
	while (CurrentXP >= XPToNextLevel && CurrentLevel < 18)  // Max level 18
	{
		CurrentXP -= XPToNextLevel;
		CurrentLevel++;
		XPToNextLevel = CalculateXPForLevel(CurrentLevel);

		// Apply per-level stat growth (tweakable via properties)
		AttackDamage += AttackDamagePerLevel;
		AbilityPower += AbilityPowerPerLevel;
		Armor += ArmorPerLevel;
		MagicResist += MagicResistPerLevel;
		AttackSpeed += AttackSpeedPerLevel;
		CritChance += CritChancePerLevel;

		// Special test: apply a one-time large bonus when reaching level 2 (useful for quick testing)
		if (!bLevel2BonusApplied && CurrentLevel == 2)
		{
			AttackDamage += Level2DamageBonus;
			bLevel2BonusApplied = true;
			UE_LOG(LogCoding, Log, TEXT("Level2 bonus: +%.1f AD (total %.1f)"), Level2DamageBonus, AttackDamage);
		}

		// Notify listeners
		OnLevelUp.Broadcast(CurrentLevel);
		// Use multicast to notify all clients in one place
		Multicast_ForceNotifyStatsChanged();

		UE_LOG(LogCoding, Display, TEXT("LEVEL UP %d: AD %.1f AP %.1f Armor %.1f MR %.1f AS %.2f Crit %.3f"),
			CurrentLevel, AttackDamage, AbilityPower, Armor, MagicResist, AttackSpeed, CritChance);
	}
}

int32 UPlayerStatsComponent::CalculateXPForLevel(int32 Level) const
{
	// LoL XP table - XP needed to reach the NEXT level
	// Level 1->2: 280, 2->3: 380, etc.
	switch (Level)
	{
		case 1:  return 280;
		case 2:  return 380;
		case 3:  return 480;
		case 4:  return 580;
		case 5:  return 680;
		case 6:  return 780;
		case 7:  return 880;
		case 8:  return 980;
		case 9:  return 1080;
		case 10: return 1180;
		case 11: return 1280;
		case 12: return 1380;
		case 13: return 1480;
		case 14: return 1580;
		case 15: return 1680;
		case 16: return 1780;
		case 17: return 1880;
		default: return 9999; // Max level
	}
}

// ======= Detailed stat getters =======
float UPlayerStatsComponent::GetAttackDamage() const
{
	return AttackDamage;
}

float UPlayerStatsComponent::GetAbilityPower() const
{
	return AbilityPower;
}

float UPlayerStatsComponent::GetArmor() const
{
	return Armor;
}

float UPlayerStatsComponent::GetMagicResist() const
{
	return MagicResist;
}

float UPlayerStatsComponent::GetAttackSpeed() const
{
	return AttackSpeed;
}

float UPlayerStatsComponent::GetCritChance() const
{
	return CritChance;
}

// ========== SERVER RPCs IMPLEMENTATION ==========

bool UPlayerStatsComponent::Server_AddGold_Validate(int32 Amount)
{
	// Basic validation: amount must be reasonable (anti-cheat)
	return Amount > 0 && Amount < 100000;
}

void UPlayerStatsComponent::Server_AddGold_Implementation(int32 Amount)
{
	// Server authoritative: apply gold
	Gold += Amount;
	OnGoldChanged.Broadcast(Gold, Amount);
	Multicast_ForceNotifyStatsChanged();
	UE_LOG(LogCoding, Display, TEXT("[Server] Gold +%d (Total: %d)"), Amount, Gold);
}

bool UPlayerStatsComponent::Server_SpendGold_Validate(int32 Amount)
{
	// Basic validation
	return Amount > 0 && Amount < 100000;
}

void UPlayerStatsComponent::Server_SpendGold_Implementation(int32 Amount)
{
	if (Gold >= Amount)
	{
		Gold -= Amount;
		OnGoldChanged.Broadcast(Gold, -Amount);
		Multicast_ForceNotifyStatsChanged();
		UE_LOG(LogCoding, Display, TEXT("[Server] Gold -%d (Total: %d)"), Amount, Gold);
	}
	else
	{
		UE_LOG(LogCoding, Warning, TEXT("[Server] Gold DENIED: have %d need %d"), Gold, Amount);
	}
}

bool UPlayerStatsComponent::Server_AddXP_Validate(int32 Amount)
{
	return Amount > 0 && Amount < 100000;
}

void UPlayerStatsComponent::Server_AddXP_Implementation(int32 Amount)
{
	CurrentXP += Amount;
	OnXPChanged.Broadcast(CurrentXP, CurrentLevel, Amount);
	Multicast_ForceNotifyStatsChanged();
	UE_LOG(LogCoding, Display, TEXT("[Server] XP +%d (Total: %d/%d)"), Amount, CurrentXP, XPToNextLevel);
	CheckLevelUp();
}

bool UPlayerStatsComponent::Server_AddKill_Validate()
{
	return true; // Server validates kill event via game logic elsewhere
}

void UPlayerStatsComponent::Server_AddKill_Implementation()
{
	Kills++;
	UE_LOG(LogCoding, Log, TEXT("[Server] Kill (Total: %d)"), Kills);
}

bool UPlayerStatsComponent::Server_AddDeath_Validate()
{
	return true;
}

void UPlayerStatsComponent::Server_AddDeath_Implementation()
{
	Deaths++;
	UE_LOG(LogCoding, Log, TEXT("[Server] Death (Total: %d)"), Deaths);
}

bool UPlayerStatsComponent::Server_AddAssist_Validate()
{
	return true;
}

void UPlayerStatsComponent::Server_AddAssist_Implementation()
{
	Assists++;
	UE_LOG(LogCoding, Log, TEXT("[Server] Assist (Total: %d)"), Assists);
}
