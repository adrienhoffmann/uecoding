// BushVolume.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BushVolume.generated.h"

class USplineComponent;
class UBoxComponent;
class UCapsuleComponent;

UCLASS()
class CODING_API ABushVolume : public AActor
{
    GENERATED_BODY()

public:
    ABushVolume();

    // Use spline to generate capsules along the spline, or fallback to a simple box
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bush")
    bool bUseSpline = true;

    // The spline defines the centerline for the bush
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bush")
    USplineComponent* BushSpline;

    // Fallback box collision
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bush")
    UBoxComponent* CollisionBox;

    // Width of spline bush (diameter)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bush", meta = (ClampMin = "1.0"))
    float SplineWidth = 200.f;

    // Distance between generated capsules along spline
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bush", meta = (ClampMin = "1.0"))
    float CapsuleSpacing = 100.f;

    // Generated capsule components for spline shape
    UPROPERTY(VisibleInstanceOnly, Transient)
    TArray<UCapsuleComponent*> GeneratedCapsules;

    // Check if a point is inside the bush volume (spline or box)
    UFUNCTION(BlueprintCallable, Category = "Bush")
    bool IsPointInside(const FVector& WorldLocation) const;

    // Check if an actor is inside the bush (wrapper around point test)
    UFUNCTION(BlueprintCallable, Category = "Bush")
    bool IsActorInside(AActor* Actor) const;

protected:
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
};
