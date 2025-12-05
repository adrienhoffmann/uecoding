// Fill out your copyright notice in the Description page of Project Settings.

#include "HealthBarComponent.h"
#include "HealthComponent.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Blueprint/UserWidget.h"
#include "Components/ProgressBar.h"

UHealthBarComponent::UHealthBarComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Default settings
	BarOffset = FVector(0.0f, 0.0f, 100.0f);
	bAlwaysFaceCamera = true;
	bHideWhenFull = false;
	HealthComponent = nullptr;

	// Widget component settings
	SetWidgetSpace(EWidgetSpace::Screen);
	SetDrawSize(FVector2D(150.0f, 20.0f));
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void UHealthBarComponent::BeginPlay()
{
	Super::BeginPlay();

	// Find health component on owner
	AActor* Owner = GetOwner();
	if (Owner)
	{
		HealthComponent = Owner->FindComponentByClass<UHealthComponent>();
	}

	// Set relative location
	SetRelativeLocation(BarOffset);
}

void UHealthBarComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateHealthBar();

	// Face camera if needed
	if (bAlwaysFaceCamera)
	{
		APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0);
		if (CameraManager)
		{
			FVector CameraLocation = CameraManager->GetCameraLocation();
			FVector BarLocation = GetComponentLocation();
			FVector Direction = CameraLocation - BarLocation;
			Direction.Z = 0; // Keep upright
			if (!Direction.IsNearlyZero())
			{
				SetWorldRotation(Direction.Rotation());
			}
		}
	}
}

float UHealthBarComponent::GetHealthPercent() const
{
	if (HealthComponent)
	{
		return HealthComponent->Health / HealthComponent->MaxHealth;
	}
	return 1.0f;
}

void UHealthBarComponent::UpdateHealthBar()
{
	if (!HealthComponent)
	{
		return;
	}

	// Update the progress bar in the widget
	UUserWidget* HealthWidget = GetWidget();
	if (HealthWidget)
	{
		UProgressBar* ProgressBar = Cast<UProgressBar>(HealthWidget->GetWidgetFromName(TEXT("ProgressBar_0")));
		if (ProgressBar)
		{
			ProgressBar->SetPercent(GetHealthPercent());
		}
	}

	// Hide when full if enabled
	if (bHideWhenFull)
	{
		bool bShouldHide = (HealthComponent->Health >= HealthComponent->MaxHealth);
		SetVisibility(!bShouldHide);
	}

	// Hide when dead
	if (HealthComponent->bIsDead)
	{
		SetVisibility(false);
	}
}
