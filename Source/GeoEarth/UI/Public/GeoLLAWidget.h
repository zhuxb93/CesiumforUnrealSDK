#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GeoLLAWidget.generated.h"

class AGeoCameraController;
class UTextBlock;

UCLASS()
class GEOEARTH_API UGeoLLAWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	UPROPERTY(meta = (BindWidget))
	UTextBlock* LongitudeText = nullptr;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* LatitudeText = nullptr;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* DistanceText = nullptr;

	UPROPERTY()
	AGeoCameraController* CachedCameraController;

	void FindCameraController();
};