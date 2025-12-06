#include "ShopActor.h"
#include "ShopComponent.h"

AShopActor::AShopActor()
{
    PrimaryActorTick.bCanEverTick = false;
    ShopComponent = CreateDefaultSubobject<UShopComponent>(TEXT("ShopComponent"));
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AShopActor::BeginPlay()
{
    Super::BeginPlay();
}
