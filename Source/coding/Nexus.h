// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Nexus.generated.h"

class UHealthComponent;

UCLASS()
class CODING_API ANexus : public AActor
{
	GENERATED_BODY()
	
public:	
	ANexus();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* NexusMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UHealthComponent* HealthComponent;

	// Vision source so nexus grants vision for its team
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vision")
	class UVisionSourceComponent* VisionSource = nullptr;

private:
	UFUNCTION()
	void OnNexusDestroyed(AActor* KilledActor, AActor* Killer);
};
