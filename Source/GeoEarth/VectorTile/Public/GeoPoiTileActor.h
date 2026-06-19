#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TileID.h"
#include "Interfaces/IHttpRequest.h"
#include "Blueprint/UserWidget.h"
#include "Components/WidgetComponent.h"
#include "GeoPoiTileActor.generated.h"

class AGeoCameraController;
class ACesiumGeoreference;
class UWidgetComponent;
class AStaticMeshActor;
class APoiWidgetActor;
class UPoiOcclusionManager;

UENUM(BlueprintType)
enum class EPoiType : uint8
{
    Country UMETA(DisplayName = "Country POI"),
    City UMETA(DisplayName = "City POI"),
    All UMETA(DisplayName = "All POI")
};

USTRUCT(BlueprintType)
struct FPoiDistanceLevel
{
    GENERATED_BODY()

    UPROPERTY()
    double Distance = 0.0;

    UPROPERTY()
    double RangeFactor = 1.0;

    FPoiDistanceLevel() {}

    FPoiDistanceLevel(double InDistance, double InRangeFactor)
        : Distance(InDistance), RangeFactor(InRangeFactor) {}
};

USTRUCT(BlueprintType)
struct FPoiData
{
    GENERATED_BODY()
    
    UPROPERTY()
    FString Name;
    
    UPROPERTY()
    FString PoiType;
    
    UPROPERTY()
    double Longitude = 0.0;
    
    UPROPERTY()
    double Latitude = 0.0;
    
    UPROPERTY()
    double Altitude = 0.0;
    
    UPROPERTY()
    int32 Importance = 0;
    
    UPROPERTY()
    FString Id;

    UPROPERTY()
    FVector WorldPosition;
    
    FPoiData() {}
};

UCLASS()
class GEOEARTH_API AGeoPoiTileActor : public AActor
{
    GENERATED_BODY()
    
public:
    AGeoPoiTileActor();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "POI Settings")
    EPoiType PoiType = EPoiType::All;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Distance Index")
    int32 MinDistanceIndex = 0;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Distance Index")
    int32 MaxDistanceIndex = 9;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile Settings")
    float RefreshWaitTime = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile Settings")
    int32 MaxConcurrentDownloads = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "API Settings")
    FString RequestURL = TEXT("https://example.com/service-endpoint");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bShowDebugCubes = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    float DebugCubeScale = 10000000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
    TSubclassOf<APoiWidgetActor> PoiWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
    float AdjustScaleFactor = 0.03f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion")
    bool bEnableOcclusionCulling = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion")
    float OcclusionCheckInterval = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion", meta = (ClampMin = "20.0", ClampMax = "200.0"))
    FVector2D MarkerSize = FVector2D(40.0f, 40.0f);
    
    UFUNCTION(BlueprintCallable, Category = "POI")
    const TSet<FTileID>& GetCurrentTiles() const;
    
    UFUNCTION(BlueprintCallable, Category = "POI")
    void ClearAllTiles();
    
    UFUNCTION(BlueprintCallable, Category = "POI")
    int32 GetCurrentDistanceIndex() const;
    
protected:
    UPROPERTY()
    AGeoCameraController* CameraController;
    
    UPROPERTY()
    ACesiumGeoreference* Georeference;
    
    UPROPERTY()
    UPoiOcclusionManager* OcclusionManager;
    
    TSet<FTileID> CurrentTiles;
    TSet<FTileID> CachedTiles;
    
    TMap<FTileID, TArray<FString>> TilePoiIds;
    
    TMap<FTileID, TSharedPtr<IHttpRequest>> ActiveHttpRequests;
    
    TArray<FTileID> TileLoadQueue;
    TSet<FTileID> TileLoadQueueSet;
    int32 CurrentlyDownloadingCount = 0;
    
public:
    UPROPERTY()
    TMap<FString, APoiWidgetActor*> PoiWidgetActors;
    
protected:
    UPROPERTY()
    TMap<FTileID, AStaticMeshActor*> TileCubes;

    int32 CurrentDistanceIndex;
    float RefreshTimer;
    float OcclusionTimer;
    
    void FindCameraController();
    void FindGeoreference();
    void UpdateTiles();
    void ProcessTileQueue();
    
    int32 FindDistanceIndex(double Distance) const;
    int32 GetTileLevelFromDistanceIndex(int32 DistanceIndex) const;
    
    TSet<FTileID> GenerateLevel2Tiles() const;
    TSet<FTileID> GetTilesInFrustum(int32 TileLevel) const;
    
    void StartTileDownload(const FTileID& TileID);
    void OnTileDownloadComplete(FTileID TileID, TArray<uint8> Data);
    void ParseGeoJsonData(const TArray<uint8>& Data, TArray<FPoiData>& OutPoiData);
    
    void CreatePoiLabels(const TArray<FPoiData>& PoiData, const FTileID& TileID);
    void UpdatePoiVisibility();
    void ClearPoiLabels();
    void ClearPoiLabelsForTile(const FTileID& TileID);
    
    void SpawnCubeForTile(const FTileID& TileID);
    void DestroyCubeForTile(const FTileID& TileID);
    void ClearAllCubes();
    FVector GetTileWorldPosition(const FTileID& TileID);
    
    void InitializeOcclusionManager();
    void UpdateOcclusionCulling();
    
    static TArray<FPoiDistanceLevel>& GetDefaultDistanceLevels();
    static TMap<int32, int32>& GetCountryPoiLevelMapping();
    static TMap<int32, int32>& GetCityPoiLevelMapping();
};