#include "GeoLLAWidget.h"
#include "Components/TextBlock.h"
#include "GeoCameraController.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetTree.h"

bool UGeoLLAWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	LongitudeText = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("LongitudeText"))));
	LatitudeText = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("LatitudeText"))));
	DistanceText = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("DistanceText"))));

	return true;
}

void UGeoLLAWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

if (!CachedCameraController)
	{
		FindCameraController();
		if (!CachedCameraController)
		{
			return;
		}
	}

	FVector LLA = CachedCameraController->GetCenterPointLLA();
	double Distance = CachedCameraController->GetDistance();

	if (LongitudeText)
	{
		LongitudeText->SetText(FText::FromString(FString::Printf(TEXT("%.6f"), LLA.X)));
	}

	if (LatitudeText)
	{
		LatitudeText->SetText(FText::FromString(FString::Printf(TEXT("%.6f"), LLA.Y)));
	}

	if (DistanceText)
	{
		DistanceText->SetText(FText::FromString(FString::Printf(TEXT("%.2f m"), Distance)));
	}
}

void UGeoLLAWidget::FindCameraController()
{
	if (CachedCameraController) return;

	if (!GetWorld())
	{
		return;
	}

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGeoCameraController::StaticClass(), FoundActors);

	if (FoundActors.Num() > 0)
	{
		CachedCameraController = Cast<AGeoCameraController>(FoundActors[0]);
	}
}