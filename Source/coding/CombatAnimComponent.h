// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatAnimComponent.generated.h"

class UAnimMontage;
class UAnimInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAttackAnimNotify);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class CODING_API UCombatAnimComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UCombatAnimComponent();

protected:
	virtual void BeginPlay() override;

public:
	// Animation Montages
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Animations")
	UAnimMontage* MeleeAttackMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Animations")
	UAnimMontage* RangedAttackMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Animations")
	float MeleeAttackPlayRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Animations")
	float RangedAttackPlayRate;

	// Events - called from Anim Notifies
	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnAttackAnimNotify OnMeleeHitPoint;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnAttackAnimNotify OnRangedReleasePoint;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FOnAttackAnimNotify OnAttackEnd;

	// Is currently playing attack animation
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bIsAttacking;

	// Play attack animation
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void PlayMeleeAttack();

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void PlayRangedAttack();

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void StopAttackAnimation();

	// Called from Anim Notifies (Blueprint or C++)
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void NotifyMeleeHitPoint();

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void NotifyRangedReleasePoint();

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void NotifyAttackEnd();

	// Get the anim instance
	UFUNCTION(BlueprintCallable, Category = "Combat")
	UAnimInstance* GetOwnerAnimInstance() const;

private:
	void PlayMontage(UAnimMontage* Montage, float PlayRate);
};
