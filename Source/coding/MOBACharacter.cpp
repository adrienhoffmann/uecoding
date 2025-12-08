// Fill out your copyright notice in the Description page of Project Settings.

#include "MOBACharacter.h"
#include "HealthComponent.h"
#include "PlayerStatsComponent.h"
#include "CombatComponent.h"
#include "CombatAnimComponent.h"
#include "FogOfWarPostProcess.h"
#include "VisionSourceComponent.h"
#include "InventoryComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Logging.h"

AMOBACharacter::AMOBACharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create components natively (stable across recompiles, like EnemyCharacter does)
	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	PlayerStatsComponent = CreateDefaultSubobject<UPlayerStatsComponent>(TEXT("PlayerStatsComponent"));
	InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));
	CombatComponent = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	CombatAnimComponent = CreateDefaultSubobject<UCombatAnimComponent>(TEXT("CombatAnimComponent"));

	// Create Fog of War post-process component for 3D visualization (only relevant on local client)
	FogOfWarPostProcess = CreateDefaultSubobject<UFogOfWarPostProcess>(TEXT("FogOfWarPostProcess"));

	// Create Vision Source component - reveals fog around this character
	VisionSource = CreateDefaultSubobject<UVisionSourceComponent>(TEXT("VisionSource"));
	VisionSource->VisionRadius = 2000.f; // Default vision radius
	VisionSource->TeamID = 0; // Default team, should be set by GameMode

	// Set this character to be controlled by player
	AutoPossessPlayer = EAutoReceiveInput::Disabled; // Let GameMode handle possession

	// Camera setup (top-down style)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritYaw = false;
	CameraBoom->bInheritRoll = false;
	// Ensure spring arm keeps a fixed rotation independent from pawn orientation
	CameraBoom->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f));
	CameraBoom->TargetArmLength = 1200.f;

	TopDownCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCamera->bUsePawnControlRotation = false;

	MinZoomDistance = 400.f;
	MaxZoomDistance = 3000.f;
	ZoomSpeed = 600.f; // units per scroll tick
	DesiredArmLength = CameraBoom->TargetArmLength;
}

void AMOBACharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (CameraBoom)
	{
		float Current = CameraBoom->TargetArmLength;
		float NewLength = FMath::FInterpTo(Current, DesiredArmLength, DeltaSeconds, 10.f);
		CameraBoom->TargetArmLength = NewLength;
	}
}

void AMOBACharacter::AdjustZoom(float AxisValue)
{
	if (FMath::IsNearlyZero(AxisValue)) return;

	// AxisValue will be positive when scrolling up in many setups; invert if needed
	DesiredArmLength = FMath::Clamp(DesiredArmLength - AxisValue * ZoomSpeed, MinZoomDistance, MaxZoomDistance);
}

void AMOBACharacter::BeginPlay()
{
	Super::BeginPlay();

	// Components are already created and attached at this point
	UE_LOG(LogCoding, Log, TEXT("MOBACharacter BeginPlay - PlayerStatsComponent: %s"), 
		PlayerStatsComponent ? TEXT("VALID") : TEXT("NULL"));

	// Ensure the HealthComponent has a valid TeamID. If it's neutral (-1), assign it to team 0
	if (HealthComponent && HealthComponent->TeamID == -1)
	{
		HealthComponent->TeamID = 0;
		UE_LOG(LogCoding, Log, TEXT("MOBACharacter BeginPlay: HealthComponent TeamID was neutral. Assigned default TeamID=0 to %s"), *GetName());
	}

	// Ensure VisionSource follows the HealthComponent team
	if (VisionSource && HealthComponent)
	{
		VisionSource->SetTeamID(HealthComponent->TeamID);
	}
}
