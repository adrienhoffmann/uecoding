#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShopActor.generated.h"

class UShopComponent;

UCLASS()
class CODING_API AShopActor : public AActor
{
    GENERATED_BODY()

public:
    AShopActor();

protected:
    virtual void BeginPlay() override;

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UShopComponent* ShopComponent;
};
