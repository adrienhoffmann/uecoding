// Fill out your copyright notice in the Description page of Project Settings.

#include "CombatAnimComponent.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Logging.h"

UCombatAnimComponent::UCombatAnimComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	MeleeAttackMontage = nullptr;
	RangedAttackMontage = nullptr;
	MeleeAttackPlayRate = 1.0f;
	RangedAttackPlayRate = 1.0f;
	bIsAttacking = false;
}

void UCombatAnimComponent::BeginPlay()
{
	Super::BeginPlay();
}

UAnimInstance* UCombatAnimComponent::GetOwnerAnimInstance() const
{
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (OwnerCharacter)
	{
		USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh();
		if (Mesh)
		{
			return Mesh->GetAnimInstance();
		}
	}
	return nullptr;
}

void UCombatAnimComponent::PlayMontage(UAnimMontage* Montage, float PlayRate)
{
	if (!Montage)
	{
		// No montage - instant attack
		return;
	}

	UAnimInstance* AnimInstance = GetOwnerAnimInstance();
	if (!AnimInstance)
	{
		return;
	}

	// Stop any current attack
	if (bIsAttacking)
	{
		StopAttackAnimation();
	}

	bIsAttacking = true;

	// Play the montage
	float Duration = AnimInstance->Montage_Play(Montage, PlayRate);
	
	if (Duration <= 0.0f)
	{
		bIsAttacking = false;
		UE_LOG(LogCoding, Warning, TEXT("CombatAnimComponent: Failed to play montage %s"), *Montage->GetName());
	}
}

void UCombatAnimComponent::PlayMeleeAttack()
{
	if (MeleeAttackMontage)
	{
		PlayMontage(MeleeAttackMontage, MeleeAttackPlayRate);
	}
	else
	{
		// No animation - trigger hit immediately
		NotifyMeleeHitPoint();
		NotifyAttackEnd();
	}
}

void UCombatAnimComponent::PlayRangedAttack()
{
	if (RangedAttackMontage)
	{
		PlayMontage(RangedAttackMontage, RangedAttackPlayRate);
	}
	else
	{
		// No animation - trigger release immediately
		NotifyRangedReleasePoint();
		NotifyAttackEnd();
	}
}

void UCombatAnimComponent::StopAttackAnimation()
{
	UAnimInstance* AnimInstance = GetOwnerAnimInstance();
	if (AnimInstance)
	{
		if (MeleeAttackMontage)
		{
			AnimInstance->Montage_Stop(0.2f, MeleeAttackMontage);
		}
		if (RangedAttackMontage)
		{
			AnimInstance->Montage_Stop(0.2f, RangedAttackMontage);
		}
	}
	bIsAttacking = false;
}

void UCombatAnimComponent::NotifyMeleeHitPoint()
{
	OnMeleeHitPoint.Broadcast();
}

void UCombatAnimComponent::NotifyRangedReleasePoint()
{
	OnRangedReleasePoint.Broadcast();
}

void UCombatAnimComponent::NotifyAttackEnd()
{
	bIsAttacking = false;
	OnAttackEnd.Broadcast();
}
