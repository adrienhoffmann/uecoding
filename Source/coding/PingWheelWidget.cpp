#include "PingWheelWidget.h"
#include "Components/Button.h"
#include "MOBAPlayerController.h"
#include "MapPing.h"
#include "Logging.h"

void UPingWheelWidget::NativeConstruct()
{
    Super::NativeConstruct();
    // Ensure we don't add the same dynamic binding twice. Remove before adding.
    if (Btn_Up)
    {
        Btn_Up->OnClicked.RemoveDynamic(this, &UPingWheelWidget::OnUpClicked);
        Btn_Up->OnClicked.AddDynamic(this, &UPingWheelWidget::OnUpClicked);
    }
    if (Btn_Right)
    {
        Btn_Right->OnClicked.RemoveDynamic(this, &UPingWheelWidget::OnRightClicked);
        Btn_Right->OnClicked.AddDynamic(this, &UPingWheelWidget::OnRightClicked);
    }
    if (Btn_Left)
    {
        Btn_Left->OnClicked.RemoveDynamic(this, &UPingWheelWidget::OnLeftClicked);
        Btn_Left->OnClicked.AddDynamic(this, &UPingWheelWidget::OnLeftClicked);
    }
    if (Btn_Down)
    {
        Btn_Down->OnClicked.RemoveDynamic(this, &UPingWheelWidget::OnDownClicked);
        Btn_Down->OnClicked.AddDynamic(this, &UPingWheelWidget::OnDownClicked);
    }
    if (Btn_Center)
    {
        Btn_Center->OnClicked.RemoveDynamic(this, &UPingWheelWidget::OnCenterClicked);
        Btn_Center->OnClicked.AddDynamic(this, &UPingWheelWidget::OnCenterClicked);
    }
}

void UPingWheelWidget::NativeDestruct()
{
    // Remove dynamic bindings to be safe and avoid duplicate bindings/error on re-create
    if (Btn_Up)
    {
        Btn_Up->OnClicked.RemoveDynamic(this, &UPingWheelWidget::OnUpClicked);
    }
    if (Btn_Right)
    {
        Btn_Right->OnClicked.RemoveDynamic(this, &UPingWheelWidget::OnRightClicked);
    }
    if (Btn_Left)
    {
        Btn_Left->OnClicked.RemoveDynamic(this, &UPingWheelWidget::OnLeftClicked);
    }
    if (Btn_Down)
    {
        Btn_Down->OnClicked.RemoveDynamic(this, &UPingWheelWidget::OnDownClicked);
    }
    if (Btn_Center)
    {
        Btn_Center->OnClicked.RemoveDynamic(this, &UPingWheelWidget::OnCenterClicked);
    }

    Super::NativeDestruct();
}

void UPingWheelWidget::OnUpClicked()
{
    if (APlayerController* PC = GetOwningPlayer())
    {
        AMOBAPlayerController* MPC = Cast<AMOBAPlayerController>(PC);
        if (MPC)
        {
            MPC->Server_RequestPing(CenterWorldLocation, EMapPingType::Ping_Help);
            UE_LOG(LogCoding, Log, TEXT("PingWheel: Up clicked -> Ping_Help at %s"), *CenterWorldLocation.ToString());
        }
    }
    RemoveFromParent();
}

void UPingWheelWidget::OnRightClicked()
{
    if (APlayerController* PC = GetOwningPlayer())
    {
        AMOBAPlayerController* MPC = Cast<AMOBAPlayerController>(PC);
        if (MPC)
        {
            MPC->Server_RequestPing(CenterWorldLocation, EMapPingType::Ping_OnMyWay);
            UE_LOG(LogCoding, Log, TEXT("PingWheel: Right clicked -> Ping_OnMyWay at %s"), *CenterWorldLocation.ToString());
        }
    }
    RemoveFromParent();
}

void UPingWheelWidget::OnLeftClicked()
{
    if (APlayerController* PC = GetOwningPlayer())
    {
        AMOBAPlayerController* MPC = Cast<AMOBAPlayerController>(PC);
        if (MPC)
        {
            MPC->Server_RequestPing(CenterWorldLocation, EMapPingType::Ping_Question);
            UE_LOG(LogCoding, Log, TEXT("PingWheel: Left clicked -> Ping_Question at %s"), *CenterWorldLocation.ToString());
        }
    }
    RemoveFromParent();
}

void UPingWheelWidget::OnDownClicked()
{
    if (APlayerController* PC = GetOwningPlayer())
    {
        AMOBAPlayerController* MPC = Cast<AMOBAPlayerController>(PC);
        if (MPC)
        {
            MPC->Server_RequestPing(CenterWorldLocation, EMapPingType::Ping_Simple);
            UE_LOG(LogCoding, Log, TEXT("PingWheel: Down clicked -> Ping_Simple at %s"), *CenterWorldLocation.ToString());
        }
    }
    RemoveFromParent();
}

void UPingWheelWidget::OnCenterClicked()
{
    if (APlayerController* PC = GetOwningPlayer())
    {
        AMOBAPlayerController* MPC = Cast<AMOBAPlayerController>(PC);
        if (MPC)
        {
            MPC->Server_RequestPing(CenterWorldLocation, EMapPingType::Ping_Simple);
            UE_LOG(LogCoding, Log, TEXT("PingWheel: Center clicked -> Ping_Simple at %s"), *CenterWorldLocation.ToString());
        }
    }
    RemoveFromParent();
}
