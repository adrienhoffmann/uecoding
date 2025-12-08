// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MOBACharacter.generated.h"

class UHealthComponent;
class UPlayerStatsComponent;
class UInventoryComponent;
class UCombatComponent;
class UCombatAnimComponent;
class USpringArmComponent;
class UCameraComponent;
class UFogOfWarPostProcess;
class UVisionSourceComponent;

/**
 * Base Character class for player-controlled MOBA characters.
 * Contains PlayerStatsComponent created natively (stable across recompiles).
 */
UCLASS()
class CODING_API AMOBACharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AMOBACharacter();
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

public:
	// ========== COMPONENTS (Native, stable) ==========
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UHealthComponent* HealthComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UPlayerStatsComponent* PlayerStatsComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UInventoryComponent* InventoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCombatComponent* CombatComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UCombatAnimComponent* CombatAnimComponent;

	// Fog of War post-process component for 3D world visualization (client-side)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FogOfWar")
	UFogOfWarPostProcess* FogOfWarPostProcess;

	// Vision source for fog of war - reveals fog around this character
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FogOfWar")
	UVisionSourceComponent* VisionSource;

	// ========== GETTERS (BlueprintCallable for convenience) ==========

	UFUNCTION(BlueprintPure, Category = "Stats")
	UPlayerStatsComponent* GetPlayerStatsComponent() const { return PlayerStatsComponent; }

	UFUNCTION(BlueprintPure, Category = "Health")
	UHealthComponent* GetHealthComponent() const { return HealthComponent; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	UInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	UCombatComponent* GetCombatComponent() const { return CombatComponent; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	UCombatAnimComponent* GetCombatAnimComponent() const { return CombatAnimComponent; }

	// ========== CAMERA / ZOOM ==========

	/** Spring arm for top-down camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	USpringArmComponent* CameraBoom;

	/** Camera used for top-down view */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	UCameraComponent* TopDownCamera;

	/** Minimum zoom (arm length) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float MinZoomDistance;

	/** Maximum zoom (arm length) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float MaxZoomDistance;

	/** How fast scroll changes the desired zoom */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float ZoomSpeed;

	/** Call to adjust desired zoom amount (bind this to input) */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void AdjustZoom(float AxisValue);

protected:
	/** Internal desired arm length used for smooth interpolation */
	float DesiredArmLength;
};
