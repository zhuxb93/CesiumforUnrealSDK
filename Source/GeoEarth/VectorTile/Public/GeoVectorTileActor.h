#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TileID.h"
#include "BuildingData.h"
#include "RawMeshData.h"
#include "VectorTileType.h"
#include "AsyncMeshCalculation.h"
#include "FrameRateMonitor.h"
#include "Interfaces/IHttpRequest.h"
#include "GeoVectorTileActor.generated.h"

class UProceduralMeshComponent;
class ACesiumGeoreference;
class AGeoCameraController;
class AStaticMeshActor;

USTRUCT(BlueprintType)
struct FTileDownloadInfo
{
    GENERATED_BODY()
    
    UPROPERTY()
    FTileID TileID;
    
    UPROPERTY()
    bool bIsDownloading = false;
};

UCLASS()
class GEOEARTH_API AGeoVectorTileActor : public AActor
{
    GENERATED_BODY()
    
public:
    AGeoVectorTileActor();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile Settings")
    int32 TileZoomLevel = 14;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile Settings")
    float RefreshWaitTime = 0.5f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile Settings")
    double StartDistance = 50000.0;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile Settings")
    double EndDistance = 0.0;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile Settings")
    float TileSizeScaleFactor = 1.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bEnableDebug = true;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "API Settings")
    FString RequestURL = TEXT("https://example.com/service-endpoint");
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile Settings")
    EVectorTileType VectorTileType = EVectorTileType::Buia;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
    UMaterialInterface* DefaultMaterial;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    int32 MaxTilesPerFrame = 4;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
    int32 MaxConcurrentDownloads = 8;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptive Loading")
    bool bEnableAdaptiveLoading = true;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptive Loading")
    float TargetFrameRate = 60.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptive Loading")
    float MeshCreationTimeBudgetMs = 3.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptive Loading")
    int32 MinTilesPerFrame = 1;
    
    UFUNCTION(BlueprintCallable, Category = "Tile")
    const TSet<FTileID>& GetCurrentTiles() const;
    
    UFUNCTION(BlueprintCallable, Category = "Tile")
    void LoadTilesFromSet(const TSet<FTileID>& TileIDs, const FTileID& CenterTile);
    
    UFUNCTION(BlueprintCallable, Category = "Tile")
    void ClearAllTiles();
    
    UFUNCTION(BlueprintCallable, Category = "Tile")
    void UnloadTile(const FTileID& TileID);
    
    UFUNCTION(BlueprintCallable, Category = "Tile")
    void UnloadTilesFromSet(const TSet<FTileID>& TileIDs);
    
    UFUNCTION(BlueprintCallable, Category = "Tile")
    void ClearAllTileStates();
    
    UFUNCTION(BlueprintCallable, Category = "Tile")
    void SetValidTiles(const TSet<FTileID>& ValidTiles);
    
    UFUNCTION(BlueprintCallable, Category = "Tile")
    bool IsValidTile(const FTileID& TileID) const;

    UFUNCTION(BlueprintCallable, Category = "Tile")
    void SetEnabled(bool bNewEnabled);

    UFUNCTION(BlueprintCallable, Category = "Tile")
    bool IsEnabled() const { return bEnabled; }

protected:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tile Settings")
    bool bEnabled = true;
    UPROPERTY()
    AGeoCameraController* CameraController;
    
    UPROPERTY()
    ACesiumGeoreference* Georeference;
    
    UPROPERTY()
    TSet<FTileID> CurrentTiles;
    
    UPROPERTY()
    TSet<FTileID> ValidTileIDs;
    
    UPROPERTY()
    TMap<FTileID, UProceduralMeshComponent*> TileMeshComponents;
    
    UPROPERTY()
    TMap<FTileID, FTileDownloadInfo> DownloadingTiles;
    
    UPROPERTY()
    TMap<FTileID, AStaticMeshActor*> TileCubes;
    
    TArray<FTileID> TileLoadQueue;
    TSet<FTileID> TileLoadQueueSet;
    int32 CurrentlyDownloadingCount;
    bool bAllowTileLoading;
    
    TMap<FTileID, TSharedPtr<IHttpRequest>> ActiveHttpRequests;
    
    TMap<FTileID, TSharedPtr<FAsyncMeshCalculationTask>> PendingMeshTasks;
    
    FFrameRateMonitor FrameRateMonitor;
    
    bool bWaitingForRefresh;
    float RefreshTimer;
    FVector LastCameraCenterECEF;
    double LastCameraDistance;
    bool bWasInDistanceRange;
    
    void FindCameraController();
    void FindGeoreference();
    void UpdateTiles();
    int32 CalculateLevelFromDistance(double Distance);
    void SpawnCubeForTile(const FTileID& TileID);
    void DestroyCubeForTile(const FTileID& TileID);
    void ClearAllCubes();
    void ClearAllTilesAndMeshes();
    FVector GetTileWorldPosition(const FTileID& TileID);
    bool TraceTerrainHeight(const FVector& StartPos, FVector& OutHitPos);
    
    void ProcessTileQueue();
    void ProcessCompletedMeshTasks();
    void StartTileDownload(const FTileID& TileID);
    void OnTileDownloadComplete(FTileID TileID, TArray<uint8> Data);
    void CreateMeshFromRawData(const FRawMeshData& RawData);
};