#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GeoKeyboardLocator.generated.h"

USTRUCT(BlueprintType)
struct FGeoKeyboardLocation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location")
	FKey Hotkey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location")
	FString LocationName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location", meta = (ClampMin = -180.0, ClampMax = 180.0))
	double Longitude = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location", meta = (ClampMin = -90.0, ClampMax = 90.0))
	double Latitude = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	double Distance = 1000000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = -360.0, ClampMax = 360.0))
	double Rotation = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = -89.0, ClampMax = 89.0))
	double Tilt = -45.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float FlyDuration = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location")
	bool bEnabled = true;
};

class AGeoCameraController;

UCLASS()
class GEOEARTH_API AGeoKeyboardLocator : public AActor
{
	GENERATED_BODY()

public:
	AGeoKeyboardLocator();

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	TArray<FGeoKeyboardLocation> Locations;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	AGeoCameraController* CameraController;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bAutoFindCameraController = true;

protected:
	UFUNCTION()
	void OnHotkeyPressed(int32 LocationIndex);

	void SetupInputBindings();
	void FlyToLocation(const FGeoKeyboardLocation& Location);
};