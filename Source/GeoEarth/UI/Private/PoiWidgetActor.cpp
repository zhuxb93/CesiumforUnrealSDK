#include "PoiWidgetActor.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"

APoiWidgetActor::APoiWidgetActor()
{
    PrimaryActorTick.bCanEverTick = false;

    WidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("WidgetComponent"));
    RootComponent = WidgetComponent;

    if (WidgetComponent)
    {
        WidgetComponent->SetWidgetSpace(EWidgetSpace::World);
        WidgetComponent->SetPivot(FVector2D(0.5f, 0.5f));
    }

    CurrentLongitude = 0.0;
    CurrentLatitude = 0.0;
    CurrentAltitude = 0.0;
    bIsOccluded = false;
    SortIndex = 0;
    CameraDistance = 0.0f;
}

void APoiWidgetActor::BeginPlay()
{
    Super::BeginPlay();
}

void APoiWidgetActor::SetPoiName(const FString& Name)
{
    CurrentPoiName = Name;
    OnPoiDataUpdated(Name);
}

void APoiWidgetActor::SetPoiData(const FString& Name, const FString& PoiType, double Longitude, double Latitude, double Altitude)
{
    CurrentPoiName = Name;
    CurrentPoiType = PoiType;
    CurrentLongitude = Longitude;
    CurrentLatitude = Latitude;
    CurrentAltitude = Altitude;
    OnPoiDataUpdated(Name);
}

void APoiWidgetActor::OnPoiDataUpdated_Implementation(const FString& Name)
{
    if (!WidgetComponent)
    {
        return;
    }

    UUserWidget* MainWidget = WidgetComponent->GetUserWidgetObject();
    if (!MainWidget)
    {
        return;
    }

    UTextBlock* PoiNameText = Cast<UTextBlock>(MainWidget->GetWidgetFromName(TEXT("PoiName")));
    if (PoiNameText)
    {
        PoiNameText->SetText(FText::FromString(Name));
    }

    if (MainWidget->WidgetTree)
    {
        MainWidget->WidgetTree->ForEachWidget([&Name](UWidget* Widget)
        {
            if (UUserWidget* ChildUserWidget = Cast<UUserWidget>(Widget))
            {
                UTextBlock* ChildPoiName = Cast<UTextBlock>(ChildUserWidget->GetWidgetFromName(TEXT("PoiName")));
                if (ChildPoiName)
                {
                    ChildPoiName->SetText(FText::FromString(Name));
                }
            }
        });
    }
}

void APoiWidgetActor::SetOccluded(bool bOccluded)
{
    if (bIsOccluded != bOccluded)
    {
        bIsOccluded = bOccluded;
        SetActorHiddenInGame(bOccluded);
    }
}

UWidgetComponent* APoiWidgetActor::GetWidgetComponent() const
{
    return WidgetComponent;
}