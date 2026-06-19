#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/WidgetComponent.h"
#include "PoiWidgetActor.generated.h"

UCLASS(BlueprintType)
class GEOEARTH_API APoiWidgetActor : public AActor
{
    GENERATED_BODY()

public:
    APoiWidgetActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Widget")
    UWidgetComponent* WidgetComponent;

    UFUNCTION(BlueprintCallable, Category = "POI")
    void SetPoiName(const FString& Name);

    UFUNCTION(BlueprintCallable, Category = "POI")
    void SetPoiData(const FString& Name, const FString& PoiType, double Longitude, double Latitude, double Altitude);

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "POI")
    void OnPoiDataUpdated(const FString& Name);
    virtual void OnPoiDataUpdated_Implementation(const FString& Name);

    UFUNCTION(BlueprintPure, Category = "Widget")
    UWidgetComponent* GetWidgetComponent() const;

    UFUNCTION(BlueprintCallable, Category = "POI Occlusion")
    void SetOccluded(bool bOccluded);

    UFUNCTION(BlueprintPure, Category = "POI Occlusion")
    bool IsOccluded() const { return bIsOccluded; }

    UFUNCTION(BlueprintCallable, Category = "POI Occlusion")
    void SetScreenRect(const FBox2D& InScreenRect) { ScreenRect = InScreenRect; }

    UFUNCTION(BlueprintPure, Category = "POI Occlusion")
    FBox2D GetScreenRect() const { return ScreenRect; }

    UFUNCTION(BlueprintCallable, Category = "POI Occlusion")
    void SetSortIndex(int32 InIndex) { SortIndex = InIndex; }

    UFUNCTION(BlueprintPure, Category = "POI Occlusion")
    int32 GetSortIndex() const { return SortIndex; }

    UFUNCTION(BlueprintCallable, Category = "POI Occlusion")
    void SetCameraDistance(float InDistance) { CameraDistance = InDistance; }

    UFUNCTION(BlueprintPure, Category = "POI Occlusion")
    float GetCameraDistance() const { return CameraDistance; }

    UFUNCTION(BlueprintPure, Category = "POI Data")
    FString GetCurrentPoiName() const { return CurrentPoiName; }

protected:
    virtual void BeginPlay() override;

    UPROPERTY(BlueprintReadOnly, Category = "POI Data")
    FString CurrentPoiName;

    UPROPERTY(BlueprintReadOnly, Category = "POI Data")
    FString CurrentPoiType;

    UPROPERTY(BlueprintReadOnly, Category = "POI Data")
    double CurrentLongitude;

    UPROPERTY(BlueprintReadOnly, Category = "POI Data")
    double CurrentLatitude;

    UPROPERTY(BlueprintReadOnly, Category = "POI Data")
    double CurrentAltitude;

    bool bIsOccluded = false;
    FBox2D ScreenRect;
    int32 SortIndex = 0;
    float CameraDistance = 0.0f;
};