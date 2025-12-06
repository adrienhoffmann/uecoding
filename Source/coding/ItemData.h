#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"
#include "ItemData.generated.h"

USTRUCT(BlueprintType)
struct FItemStatModifier
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AttackDamage = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AbilityPower = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Armor = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MagicResist = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AttackSpeed = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CritChance = 0.0f;
};

UCLASS(BlueprintType)
class CODING_API UItemData : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
    int32 Cost = 100;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
    UTexture2D* Icon;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
    FItemStatModifier Modifier;
};
