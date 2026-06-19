#include "GeoLLAWidgetActor.h"
#include "Blueprint/UserWidget.h"
#include "UObject/ConstructorHelpers.h"

AGeoLLAWidgetActor::AGeoLLAWidgetActor()
{
	PrimaryActorTick.bCanEverTick = false;

	static ConstructorHelpers::FClassFinder<UUserWidget> WidgetBPClassFinder(
		TEXT("/Game/UI/DarkBlue/GeoLLAWidget.GeoLLAWidget_C")
	);

	if (WidgetBPClassFinder.Succeeded())
	{
		LLAWidgetClass = WidgetBPClassFinder.Class;
		UE_LOG(LogTemp, Log, TEXT("AGeoLLAWidgetActor: Successfully loaded GeoLLAWidget blueprint"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AGeoLLAWidgetActor: Failed to load GeoLLAWidget blueprint from /Game/UI/DarkBlue/GeoLLAWidget"));
	}
}

void AGeoLLAWidgetActor::BeginPlay()
{
	Super::BeginPlay();

	if (!LLAWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("AGeoLLAWidgetActor: LLAWidgetClass is null, cannot create widget"));
		return;
	}

	UUserWidget* CreatedWidget = CreateWidget<UUserWidget>(GetWorld(), LLAWidgetClass);
	if (CreatedWidget)
	{
		LLADisplayWidget = CreatedWidget;
		CreatedWidget->AddToViewport();
		UE_LOG(LogTemp, Log, TEXT("AGeoLLAWidgetActor: Successfully created and added LLA widget to viewport"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AGeoLLAWidgetActor: Failed to create LLA widget instance"));
	}
}