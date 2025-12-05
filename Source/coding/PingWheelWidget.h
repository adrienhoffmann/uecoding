// Simple Ping Wheel widget (C++ base) with 4 directional buttons
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PingWheelWidget.generated.h"

class UButton;

UCLASS()
class CODING_API UPingWheelWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UFUNCTION()
    void OnUpClicked();

    UFUNCTION()
    void OnRightClicked();

    UFUNCTION()
    void OnLeftClicked();

    UFUNCTION()
    void OnDownClicked();

    UFUNCTION(BlueprintCallable, Category = "PingWheel")
    void SetCenterWorldLocation(const FVector& InLoc) { CenterWorldLocation = InLoc; }

protected:
    // Buttons expected to exist in the UMG blueprint (names must match)
    UPROPERTY(meta = (BindWidget))
    UButton* Btn_Up;

    UPROPERTY(meta = (BindWidget))
    UButton* Btn_Right;

    UPROPERTY(meta = (BindWidget))
    UButton* Btn_Left;

    UPROPERTY(meta = (BindWidget))
    UButton* Btn_Down;

    UPROPERTY(meta = (BindWidget))
    UButton* Btn_Center;

    UFUNCTION()
    void OnCenterClicked();

private:
    FVector CenterWorldLocation;
};
