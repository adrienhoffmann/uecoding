// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "HealthBarComponent.generated.h"

class UHealthComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class CODING_API UHealthBarComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UHealthBarComponent();

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Reference to the health component to display
	UPROPERTY(BlueprintReadOnly, Category = "Health Bar")
	UHealthComponent* HealthComponent;

	// Offset above the actor
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Bar")
	FVector BarOffset;

	// Should the bar always face the camera
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Bar")
	bool bAlwaysFaceCamera;

	// Hide bar when health is full
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Bar")
	bool bHideWhenFull;

	// Get health percentage (0-1)
	UFUNCTION(BlueprintPure, Category = "Health Bar")
	float GetHealthPercent() const;

private:
	void UpdateHealthBar();
};
