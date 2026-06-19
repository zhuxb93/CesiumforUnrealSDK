#include "EarthWidgetActor.h"
#include "EarthWidgetUI.h"
#include "Blueprint/UserWidget.h"
#include "UObject/ConstructorHelpers.h"

AEarthWidgetActor::AEarthWidgetActor()
{
	PrimaryActorTick.bCanEverTick = false;

	static ConstructorHelpers::FClassFinder<UUserWidget> WidgetBPClassFinder(
		TEXT("/Game/UI/DarkBlue/EarthWidgetUI.EarthWidgetUI_C")
	);

	if (WidgetBPClassFinder.Succeeded())
	{
		WidgetClass = WidgetBPClassFinder.Class;
		UE_LOG(LogTemp, Log, TEXT("AEarthWidgetActor: Successfully loaded EarthWidgetUI blueprint"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AEarthWidgetActor: Failed to load EarthWidgetUI blueprint from /Game/UI/DarkBlue/EarthWidgetUI"));
	}
}

void AEarthWidgetActor::BeginPlay()
{
	Super::BeginPlay();

	if (!WidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("AEarthWidgetActor: WidgetClass is null, cannot create widget"));
		return;
	}

	CreatedWidget = CreateWidget<UUserWidget>(GetWorld(), WidgetClass);

	if (CreatedWidget)
	{
		CreatedWidget->AddToViewport();
		UE_LOG(LogTemp, Log, TEXT("AEarthWidgetActor: Successfully created and added EarthWidget to viewport"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AEarthWidgetActor: Failed to create widget instance"));
	}
}