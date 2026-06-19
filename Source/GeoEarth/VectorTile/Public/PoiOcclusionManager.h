#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PoiOcclusionManager.generated.h"

class AGeoPoiTileActor;
class APoiWidgetActor;
class APlayerController;
class UWidgetComponent;

UCLASS()
class GEOEARTH_API UPoiOcclusionManager : public UObject
{
    GENERATED_BODY()

public:
    UPoiOcclusionManager();

    void Initialize(AGeoPoiTileActor* InOwner);
    void CheckOcclusion();
    void Cleanup();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
    float CheckInterval = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
    int32 MaxPoiPerCheck = 1000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ClampMin = "20.0", ClampMax = "200.0"))
    FVector2D DefaultMarkerSize = FVector2D(40.0f, 40.0f);

protected:
    UPROPERTY()
    AGeoPoiTileActor* OwnerActor;

    float CheckTimer = 0.0f;

    TArray<APoiWidgetActor*> CollectVisiblePoiActors();
    void CalculateScreenRects(TArray<APoiWidgetActor*>& PoiActors, APlayerController* PlayerController);
    void ExecuteCpuOcclusion(TArray<APoiWidgetActor*>& PoiActors);
    
    FBox2D GetWidgetContentScreenBounds(UWidgetComponent* WidgetComp, APlayerController* PlayerController);
};