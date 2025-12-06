// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Net/UnrealNetwork.h"
#include "ItemData.h"
#include "PlayerStatsComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGoldChanged, int32, NewGold, int32, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnXPChanged, int32, NewXP, int32, NewLevel, int32, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLevelUp, int32, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStatsChanged);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CODING_API UPlayerStatsComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UPlayerStatsComponent();

	// Replication setup
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	float GoldAccumulator;
	float GameStartTime;

public:	
	// ========== GOLD ==========
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_Gold, Category = "Stats|Gold")
	int32 Gold;

	UFUNCTION()
	void OnRep_Gold();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Gold")
	int32 GoldPerSecond;

	// Delay before passive gold starts (seconds)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Gold")
	float PassiveGoldDelay;

	// ========== XP / LEVEL ==========
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_XP, Category = "Stats|XP")
	int32 CurrentXP;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_Level, Category = "Stats|XP")
	int32 CurrentLevel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Stats|XP")
	int32 XPToNextLevel;

	UFUNCTION()
	void OnRep_XP();

	UFUNCTION()
	void OnRep_Level();

	// ========== KDA ==========
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Stats|KDA")
	int32 Kills;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Stats|KDA")
	int32 Deaths;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Stats|KDA")
	int32 Assists;

	// ========== DETAILED STATS ==========
	// These are the six detailed stats exposed to Blueprints/Widget
	// Replicated from server to clients; use Server RPCs to modify
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_Stats, Category = "Stats|Detailed")
	float AttackDamage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_Stats, Category = "Stats|Detailed")
	float AbilityPower;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_Stats, Category = "Stats|Detailed")
	float Armor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_Stats, Category = "Stats|Detailed")
	float MagicResist;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_Stats, Category = "Stats|Detailed")
	float AttackSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_Stats, Category = "Stats|Detailed")
	float CritChance;

	UFUNCTION()
	void OnRep_Stats();

	// One-time test bonus applied when reaching level 2 (useful for testing 3-shot behavior)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Testing")
	float Level2DamageBonus;

	// Internal flag to avoid applying the level2 bonus multiple times
	bool bLevel2BonusApplied;

	// ====== Per-level growth (tweakable in editor) ======
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Detailed|Growth")
	float AttackDamagePerLevel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Detailed|Growth")
	float AbilityPowerPerLevel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Detailed|Growth")
	float ArmorPerLevel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Detailed|Growth")
	float MagicResistPerLevel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Detailed|Growth")
	float AttackSpeedPerLevel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Detailed|Growth")
	float CritChancePerLevel;

	// ========== EVENTS ==========
	UPROPERTY(BlueprintAssignable, Category = "Stats|Events")
	FOnGoldChanged OnGoldChanged;

	UPROPERTY(BlueprintAssignable, Category = "Stats|Events")
	FOnXPChanged OnXPChanged;

	UPROPERTY(BlueprintAssignable, Category = "Stats|Events")
	FOnLevelUp OnLevelUp;

	// Broadcast when any detailed stat changes (can be used by widgets)
	UPROPERTY(BlueprintAssignable, Category = "Stats|Events")
	FOnStatsChanged OnStatsChanged;

	// Force a server-side notification that will run on all clients.
	// Call `ForceNotifyStatsChanged()` from server-side logic to trigger the
	// `OnStatsChanged` delegate on every client in a single multicast.
	UFUNCTION(BlueprintCallable, Category = "Stats|Events")
	void ForceNotifyStatsChanged();

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_ForceNotifyStatsChanged();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ForceNotifyStatsChanged();

	// ========== FUNCTIONS ==========
	// Local wrappers that call Server RPCs if not on server
	UFUNCTION(BlueprintCallable, Category = "Stats|Gold")
	void AddGold(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Stats|Gold")
	bool SpendGold(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Stats|XP")
	void AddXP(int32 Amount);

	// Apply passive item modifiers on the server
	UFUNCTION(BlueprintCallable, Category = "Stats|Items")
	void ApplyItemModifiers(const FItemStatModifier& Modifiers);

	UFUNCTION(Server, Reliable, WithValidation, Category = "Stats|Server")
	void Server_ApplyItemModifiers(const FItemStatModifier& Modifiers);

	// ========== SERVER RPCs (authoritative) ==========
	UFUNCTION(Server, Reliable, WithValidation, Category = "Stats|Server")
	void Server_AddGold(int32 Amount);

	UFUNCTION(Server, Reliable, WithValidation, Category = "Stats|Server")
	void Server_SpendGold(int32 Amount);

	UFUNCTION(Server, Reliable, WithValidation, Category = "Stats|Server")
	void Server_AddXP(int32 Amount);

	UFUNCTION(Server, Reliable, WithValidation, Category = "Stats|Server")
	void Server_AddKill();

	UFUNCTION(Server, Reliable, WithValidation, Category = "Stats|Server")
	void Server_AddDeath();

	UFUNCTION(Server, Reliable, WithValidation, Category = "Stats|Server")
	void Server_AddAssist();

	UFUNCTION(BlueprintPure, Category = "Stats|XP")
	float GetXPPercent() const;

	// ======= Getters for Detailed Stats =======
	UFUNCTION(BlueprintCallable, Category = "Stats|Detailed")
	float GetAttackDamage() const;

	UFUNCTION(BlueprintCallable, Category = "Stats|Detailed")
	float GetAbilityPower() const;

	UFUNCTION(BlueprintCallable, Category = "Stats|Detailed")
	float GetArmor() const;

	UFUNCTION(BlueprintCallable, Category = "Stats|Detailed")
	float GetMagicResist() const;

	UFUNCTION(BlueprintCallable, Category = "Stats|Detailed")
	float GetAttackSpeed() const;

	UFUNCTION(BlueprintCallable, Category = "Stats|Detailed")
	float GetCritChance() const;

private:
	void CheckLevelUp();
	int32 CalculateXPForLevel(int32 Level) const;
};
