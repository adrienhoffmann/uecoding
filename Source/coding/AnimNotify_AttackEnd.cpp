// Fill out your copyright notice in the Description page of Project Settings.

#include "AnimNotify_AttackEnd.h"
#include "CombatAnimComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

void UAnimNotify_AttackEnd::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		return;
	}

	UCombatAnimComponent* CombatAnimComp = Owner->FindComponentByClass<UCombatAnimComponent>();
	if (CombatAnimComp)
	{
		CombatAnimComp->NotifyAttackEnd();
	}
}
