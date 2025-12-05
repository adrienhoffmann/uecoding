// Simple component to mark actors for the minimap
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MinimapComponent.generated.h"

UENUM(BlueprintType)
enum class EMinimapIconType : uint8
{
    Icon_Hero UMETA(DisplayName = "Hero"),
    Icon_Minion UMETA(DisplayName = "Minion"),
    Icon_Tower UMETA(DisplayName = "Tower"),
    Icon_Object UMETA(DisplayName = "Object")
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class CODING_API UMinimapComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMinimapComponent();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    // Attempt to register with HUD until success (in case HUD is created after actors)
    void TryRegisterWithHUD();

    // Type shown on the minimap
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
    EMinimapIconType IconType;

    // Color for the icon
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
    FLinearColor IconColor;

    // Size in pixels on the minimap (optional)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
    float IconSize;

    // Team ID for fog of war visibility (0 = blue, 1 = red, etc.)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
    int32 TeamID = 0;

    // Only show for owning team or always
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
    bool bShowOnMinimap;

protected:
    // Timer used to retry registration
    FTimerHandle RetryRegisterTimer;
    int32 RetryAttemptsRemaining;

    
};
