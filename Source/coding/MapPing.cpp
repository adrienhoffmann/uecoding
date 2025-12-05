// Implementation for MapPing
#include "MapPing.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Logging.h"
#include "Components/BillboardComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

AMapPing::AMapPing()
{
    PrimaryActorTick.bCanEverTick = false;
    // Set replication flag directly in constructor to avoid warning about pre-init actors
    bReplicates = true;
    LifeTime = 6.0f;
    PingType = EMapPingType::Ping_Simple;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Icon = CreateDefaultSubobject<UBillboardComponent>(TEXT("PingIcon"));
    Icon->SetupAttachment(RootComponent);
    Icon->SetRelativeLocation(FVector(0,0,50.0f));

    // Note: Billboard sprite won't show in-game by default. Use a Blueprint subclass
    // with a visible mesh/particle, or set bIsScreenSizeScaled = false and use a real texture.
    static ConstructorHelpers::FObjectFinder<UTexture2D> IconTex(TEXT("/Engine/EditorResources/BadIcon"));
    if (IconTex.Succeeded())
    {
        Icon->SetSprite(IconTex.Object);
    }
}

void AMapPing::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AMapPing, PingType);
    DOREPLIFETIME(AMapPing, PingLocation);
}

void AMapPing::BeginPlay()
{
    Super::BeginPlay();
    // store location
    PingLocation = GetActorLocation();
    if (HasAuthority())
    {
        // start lifetime timer
        static FTimerHandle LifeTimerHandle;
        GetWorldTimerManager().SetTimer(LifeTimerHandle, this, &AMapPing::OnLifeExpired, LifeTime, false);
    }
    UE_LOG(LogCoding, Verbose, TEXT("MapPing spawned at %s type %d"), *PingLocation.ToString(), (int32)PingType);

    // Play ping sound locally on begin play (clients + server). Use different sound for simple ping vs others.
    USoundBase* ToPlay = nullptr;
    if (PingType == EMapPingType::Ping_Simple)
    {
        ToPlay = Sound_SimplePing;
    }
    else
    {
        ToPlay = Sound_OtherPing;
    }

    if (ToPlay)
    {
        // Play locally and request clients to play via multicast to ensure audibility
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), ToPlay, GetActorLocation());
        if (HasAuthority())
        {
            Multicast_PlayPingSound(ToPlay);
        }
    }
}

void AMapPing::Multicast_PlayPingSound_Implementation(USoundBase* SoundToPlay)
{
    if (!SoundToPlay) return;
    // Play on this instance (clients and server)
    UGameplayStatics::PlaySoundAtLocation(GetWorld(), SoundToPlay, GetActorLocation());
}

void AMapPing::OnLifeExpired()
{
    Destroy();
}
