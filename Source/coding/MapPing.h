// Simple replicated ping actor used for minimap pings
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MapPing.generated.h"

UENUM(BlueprintType)
enum class EMapPingType : uint8
{
    Ping_Simple UMETA(DisplayName = "Simple"),
    Ping_OnMyWay UMETA(DisplayName = "OnMyWay"),
    Ping_Help UMETA(DisplayName = "NeedHelp"),
    Ping_Question UMETA(DisplayName = "Question")
};

UCLASS()
class CODING_API AMapPing : public AActor
{
    GENERATED_BODY()

public:
    AMapPing();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(Replicated, BlueprintReadOnly)
    EMapPingType PingType;

    UPROPERTY(Replicated, BlueprintReadOnly)
    FVector PingLocation;

    UPROPERTY(EditDefaultsOnly, Category = "Ping")
    float LifeTime;

    // Visual component for the ping (simple billboard)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ping")
    class UBillboardComponent* Icon;

    // Sounds to play when ping spawns (set in Blueprint or defaults)
    UPROPERTY(EditDefaultsOnly, Category = "Ping|Sound")
    class USoundBase* Sound_SimplePing;

    UPROPERTY(EditDefaultsOnly, Category = "Ping|Sound")
    class USoundBase* Sound_OtherPing;
protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void OnLifeExpired();

    // Play a sound on all clients (server calls this to ensure clients hear ping)
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlayPingSound(USoundBase* SoundToPlay);
    void Multicast_PlayPingSound_Implementation(USoundBase* SoundToPlay);
};
