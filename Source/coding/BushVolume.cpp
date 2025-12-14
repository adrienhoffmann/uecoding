// BushVolume.cpp
#include "BushVolume.h"
#include "Components/SplineComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"

ABushVolume::ABushVolume()
{
    PrimaryActorTick.bCanEverTick = false;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
    CollisionBox->SetupAttachment(RootComponent);
    CollisionBox->SetCollisionProfileName(TEXT("BlockAll"));
    CollisionBox->SetCanEverAffectNavigation(false);
    CollisionBox->SetGenerateOverlapEvents(false);

    BushSpline = CreateDefaultSubobject<USplineComponent>(TEXT("BushSpline"));
    BushSpline->SetupAttachment(RootComponent);

    // Default values
    SplineWidth = 200.f;
    CapsuleSpacing = 100.f;
    bUseSpline = true;
}

void ABushVolume::OnConstruction(const FTransform& Transform)
{
    // Destroy any previously generated capsule components
    for (UCapsuleComponent* Cap : GeneratedCapsules)
    {
        if (Cap)
        {
            Cap->DestroyComponent();
        }
    }
    GeneratedCapsules.Empty();

    if (bUseSpline && BushSpline && BushSpline->GetNumberOfSplinePoints() > 0)
    {
        float SplineLen = BushSpline->GetSplineLength();
        if (SplineLen <= 0.f) return;

        const float Spacing = FMath::Max(1.0f, CapsuleSpacing);
        int32 Steps = FMath::CeilToInt(SplineLen / Spacing);
        for (int32 i = 0; i <= Steps; ++i)
        {
            float Dist = FMath::Clamp(i * Spacing, 0.f, SplineLen);
            FVector Loc = BushSpline->GetLocationAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::Local);
            FRotator Rot = BushSpline->GetRotationAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::Local);

            UCapsuleComponent* Cap = NewObject<UCapsuleComponent>(this);
            Cap->SetupAttachment(RootComponent);
            Cap->RegisterComponent();
            Cap->SetRelativeLocation(Loc);
            Cap->SetRelativeRotation(Rot);
            Cap->SetCapsuleHalfHeight(Spacing * 0.5f);
            Cap->SetCapsuleRadius(SplineWidth * 0.5f);
            Cap->SetCollisionProfileName(TEXT("BlockAll"));
            Cap->SetCanEverAffectNavigation(false);
            Cap->SetGenerateOverlapEvents(false);
            GeneratedCapsules.Add(Cap);
        }
    }
    else if (CollisionBox)
    {
        // Ensure box also blocks visibility and uses size from designer
        CollisionBox->SetCollisionProfileName(TEXT("BlockAll"));
        CollisionBox->SetGenerateOverlapEvents(false);
    }
}

void ABushVolume::BeginPlay()
{
    Super::BeginPlay();
}

bool ABushVolume::IsPointInside(const FVector& WorldLocation) const
{
    if (bUseSpline && BushSpline && BushSpline->GetNumberOfSplinePoints() > 0)
    {
        FVector Closest = BushSpline->FindLocationClosestToWorldLocation(WorldLocation, ESplineCoordinateSpace::World);
        float DistXY = FVector::DistXY(WorldLocation, Closest);
        return DistXY <= (SplineWidth * 0.5f);
    }
    else if (CollisionBox)
    {
        // Convert to local box space
        FTransform BoxTransform = CollisionBox->GetComponentTransform();
        FVector Local = BoxTransform.InverseTransformPosition(WorldLocation);
        FVector Extent = CollisionBox->GetScaledBoxExtent();
        return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
    }
    return false;
}

bool ABushVolume::IsActorInside(AActor* Actor) const
{
    if (!Actor) return false;
    return IsPointInside(Actor->GetActorLocation());
}
