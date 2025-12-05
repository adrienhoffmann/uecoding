#include "MinimapComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "PlayerHUDWidget.h"
#include "TimerManager.h"
#include "Logging.h"

UMinimapComponent::UMinimapComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    IconType = EMinimapIconType::Icon_Object;
    IconColor = FLinearColor::White;
    IconSize = 8.0f;
    bShowOnMinimap = true;
}

void UMinimapComponent::BeginPlay()
{
    Super::BeginPlay();

    if (!bShowOnMinimap) return;

    // Try to register now; if no HUD exists yet, set a short timer to retry a few times
    RetryAttemptsRemaining = 6; // try for ~3 seconds if interval 0.5s
    TryRegisterWithHUD();
}

void UMinimapComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Unregister from any HUDs
    TArray<UUserWidget*> Found;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Found, UPlayerHUDWidget::StaticClass(), true);
    for (UUserWidget* W : Found)
    {
        if (UPlayerHUDWidget* HUD = Cast<UPlayerHUDWidget>(W))
        {
            HUD->UnregisterMinimapComponent(this);
        }
    }

    Super::EndPlay(EndPlayReason);
}

void UMinimapComponent::TryRegisterWithHUD()
{
    if (!GetWorld() || !bShowOnMinimap) return;

    TArray<UUserWidget*> Found;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Found, UPlayerHUDWidget::StaticClass(), true);
    if (Found.Num() > 0)
    {
        // Only register with the local player's HUD (player index 0) to avoid duplicate registrations
        APlayerController* LocalPC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
        for (UUserWidget* W : Found)
        {
            if (UPlayerHUDWidget* HUD = Cast<UPlayerHUDWidget>(W))
            {
                if (HUD->GetOwningPlayer() == LocalPC)
                {
                    bool bAdded = HUD->RegisterMinimapComponent(this);
                    if (bAdded)
                    {
                        UE_LOG(LogCoding, Log, TEXT("UMinimapComponent: Registered with local HUD for owner %s"), *GetOwner()->GetName());
                    }
                    else
                    {
                        UE_LOG(LogCoding, VeryVerbose, TEXT("UMinimapComponent: Already registered with local HUD for owner %s"), *GetOwner()->GetName());
                    }
                }
                else
                {
                    UE_LOG(LogCoding, VeryVerbose, TEXT("UMinimapComponent: Skipping registration with non-local HUD for owner %s"), *GetOwner()->GetName());
                }
            }
        }
        // Cancel timer if any
        if (RetryRegisterTimer.IsValid() && GetWorld())
        {
            GetWorld()->GetTimerManager().ClearTimer(RetryRegisterTimer);
        }
        return;
    }

    // No HUD found yet — schedule retry
    RetryAttemptsRemaining--;
    if (RetryAttemptsRemaining > 0 && GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(RetryRegisterTimer, this, &UMinimapComponent::TryRegisterWithHUD, 0.5f, false);
        UE_LOG(LogCoding, Verbose, TEXT("UMinimapComponent: HUD not found yet, will retry for owner %s (tries left=%d)"), *GetOwner()->GetName(), RetryAttemptsRemaining);
    }
    else
    {
        UE_LOG(LogCoding, Warning, TEXT("UMinimapComponent: Failed to register with HUD after retries for owner %s"), *GetOwner()->GetName());
    }
}
