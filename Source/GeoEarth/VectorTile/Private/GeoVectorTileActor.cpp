#include "GeoVectorTileActor.h"
#include "../../GeoCameraController.h"
#include "GeoTileCalculator.h"
#include "ProceduralMeshComponent.h"
#include "CesiumRuntime/Public/CesiumGeoreference.h"
#include "CesiumRuntime/Public/CesiumWgs84Ellipsoid.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Async/Async.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"

AGeoVectorTileActor::AGeoVectorTileActor()
{
    PrimaryActorTick.bCanEverTick = true;
    
    bWaitingForRefresh = false;
    RefreshTimer = 0.0f;
    LastCameraCenterECEF = FVector::ZeroVector;
    LastCameraDistance = 0.0;
    bWasInDistanceRange = false;
    
    CurrentlyDownloadingCount = 0;
    bAllowTileLoading = true;
    
    FrameRateMonitor.Initialize(TargetFrameRate, 60);
}

void AGeoVectorTileActor::BeginPlay()
{
    Super::BeginPlay();
    
    FindCameraController();
    FindGeoreference();
    
    LastCameraCenterECEF = FVector(MAX_dbl);
    LastCameraDistance = -1.0;
}

void AGeoVectorTileActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bEnabled)
    {
        return;
    }
    
    if (!CameraController || !Georeference)
    {
        FindCameraController();
        FindGeoreference();
        return;
    }
    
    double CurrentDistance = CameraController->GetDistance();
    FVector CurrentCenterECEF = CameraController->GetCenterPointECEF();
    
    double MinDist = FMath::Min(StartDistance, EndDistance);
    double MaxDist = FMath::Max(StartDistance, EndDistance);
    bool bInDistanceRange = (CurrentDistance >= MinDist && CurrentDistance <= MaxDist);
    
    if (!bInDistanceRange)
    {
        if (bWasInDistanceRange)
        {
            ClearAllTilesAndMeshes();
            bWasInDistanceRange = false;
        }
        return;
    }
    
    if (!bWasInDistanceRange)
    {
        bWasInDistanceRange = true;
    }
    
    bool bCameraMoved = !CurrentCenterECEF.Equals(LastCameraCenterECEF, 100.0) || 
                        FMath::Abs(CurrentDistance - LastCameraDistance) > 100.0;
    
    if (bCameraMoved)
    {
        bWaitingForRefresh = true;
        RefreshTimer = 0.0f;
        LastCameraCenterECEF = CurrentCenterECEF;
        LastCameraDistance = CurrentDistance;
    }
    
    if (bWaitingForRefresh)
    {
        RefreshTimer += DeltaTime;
        if (RefreshTimer >= RefreshWaitTime)
        {
            bWaitingForRefresh = false;
            UpdateTiles();
        }
    }
    
    if (bEnableAdaptiveLoading)
    {
        FrameRateMonitor.UpdateFrameTime(DeltaTime);
    }
    
    ProcessTileQueue();
    ProcessCompletedMeshTasks();
}

void AGeoVectorTileActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearAllTilesAndMeshes();
    Super::EndPlay(EndPlayReason);
}

void AGeoVectorTileActor::FindCameraController()
{
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGeoCameraController::StaticClass(), FoundActors);
    
    if (FoundActors.Num() > 0)
    {
        CameraController = Cast<AGeoCameraController>(FoundActors[0]);
    }
}

void AGeoVectorTileActor::FindGeoreference()
{
    if (CameraController && CameraController->Georeference)
    {
        Georeference = CameraController->Georeference;
        return;
    }
    
    if (!Georeference)
    {
        Georeference = ACesiumGeoreference::GetDefaultGeoreference(this);
    }
}

void AGeoVectorTileActor::UpdateTiles()
{
    if (!CameraController || !Georeference || !CameraController->Camera)
    {
        return;
    }
    
    double Distance = CameraController->GetDistance();
    
    int32 UseLevel = TileZoomLevel;
    if (TileZoomLevel <= 0)
    {
        UseLevel = CalculateLevelFromDistance(Distance);
    }
    
    FVector CamPos = CameraController->Camera->GetComponentLocation();
    FVector CameraECEF = Georeference->TransformUnrealPositionToEarthCenteredEarthFixed(CamPos);
    FVector CameraLLA = UCesiumWgs84Ellipsoid::EarthCenteredEarthFixedToLongitudeLatitudeHeight(CameraECEF);
    
    double Longitude = CameraLLA.X;
    double Latitude = CameraLLA.Y;
    
    double VectorLoadRange = 10.0 * TileSizeScaleFactor;
    double XDistanceKm = VectorLoadRange;
    double YDistanceKm = VectorLoadRange;
    
    TSet<FTileID> NewTiles = FGeoTileCalculator::GetAreaTileIndex(Longitude, Latitude, UseLevel, XDistanceKm, YDistanceKm);
    
    SetValidTiles(NewTiles);
    
    TSet<FTileID> TilesToRemove;
    for (const FTileID& Tile : CurrentTiles)
    {
        if (!NewTiles.Contains(Tile))
        {
            TilesToRemove.Add(Tile);
        }
    }
    
    TSet<FTileID> TilesToAdd;
    for (const FTileID& Tile : NewTiles)
    {
        if (!CurrentTiles.Contains(Tile))
        {
            TilesToAdd.Add(Tile);
        }
    }
    
    for (const FTileID& Tile : TilesToRemove)
    {
        DestroyCubeForTile(Tile);
    }
    
    if (TilesToRemove.Num() > 0)
    {
        UnloadTilesFromSet(TilesToRemove);
    }
    
    if (bEnableDebug)
    {
        for (const FTileID& Tile : TilesToAdd)
        {
            SpawnCubeForTile(Tile);
        }
    }
    
    if (TilesToAdd.Num() > 0)
    {
        FTileID CenterTile = FGeoTileCalculator::LonLatToTile4326(Latitude, Longitude, UseLevel);
        LoadTilesFromSet(TilesToAdd, CenterTile);
    }
    
    CurrentTiles = NewTiles;
}

int32 AGeoVectorTileActor::CalculateLevelFromDistance(double Distance)
{
    if (Distance > 1000000) return 6;
    if (Distance > 500000) return 8;
    if (Distance > 200000) return 10;
    if (Distance > 100000) return 12;
    if (Distance > 50000) return 13;
    if (Distance > 20000) return 14;
    if (Distance > 10000) return 15;
    if (Distance > 5000) return 16;
    if (Distance > 2000) return 17;
    if (Distance > 1000) return 18;
    return 19;
}

void AGeoVectorTileActor::SpawnCubeForTile(const FTileID& TileID)
{
    if (TileCubes.Contains(TileID))
    {
        return;
    }
    
    FVector WorldPos = GetTileWorldPosition(TileID);
    
    FVector HitPos;
    if (TraceTerrainHeight(WorldPos, HitPos))
    {
        WorldPos = HitPos;
    }
    
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    
    AStaticMeshActor* CubeActor = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), WorldPos, FRotator::ZeroRotator, SpawnParams);
    
    if (CubeActor)
    {
        UStaticMeshComponent* MeshComp = CubeActor->GetStaticMeshComponent();
        if (MeshComp)
        {
            MeshComp->SetMobility(EComponentMobility::Movable);
            
            static UStaticMesh* CubeMesh = nullptr;
            if (!CubeMesh)
            {
                CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
            }
            
            if (CubeMesh)
            {
                double TileSpanDegrees = FGeoTileCalculator::GetSpan(TileID.Z);
                double TileSpanMeters = TileSpanDegrees * 111000.0;
                double DynamicScale = FMath::Clamp(TileSpanMeters * 0.1, 100.0, 1000.0);
                
                MeshComp->SetStaticMesh(CubeMesh);
                MeshComp->SetWorldScale3D(FVector(DynamicScale));
                MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                MeshComp->UpdateBounds();
            }
        }
        
        TileCubes.Add(TileID, CubeActor);
    }
}

void AGeoVectorTileActor::DestroyCubeForTile(const FTileID& TileID)
{
    if (AStaticMeshActor** CubePtr = TileCubes.Find(TileID))
    {
        if (AStaticMeshActor* Cube = *CubePtr)
        {
            Cube->Destroy();
        }
        TileCubes.Remove(TileID);
    }
}

void AGeoVectorTileActor::ClearAllCubes()
{
    for (auto& Pair : TileCubes)
    {
        if (AStaticMeshActor* Cube = Pair.Value)
        {
            Cube->Destroy();
        }
    }
    TileCubes.Empty();
}

void AGeoVectorTileActor::ClearAllTilesAndMeshes()
{
    ClearAllTileStates();
    ClearAllCubes();
    CurrentTiles.Empty();
}

FVector AGeoVectorTileActor::GetTileWorldPosition(const FTileID& TileID)
{
    FVector2D TileCenter = FGeoTileCalculator::GetTileCenter(TileID);
    
    FVector LonLatHeight(TileCenter.X, TileCenter.Y, 0.0);
    FVector ECEF = UCesiumWgs84Ellipsoid::LongitudeLatitudeHeightToEarthCenteredEarthFixed(LonLatHeight);
    
    FVector WorldPos = Georeference->TransformEarthCenteredEarthFixedPositionToUnreal(ECEF);
    
    return WorldPos;
}

bool AGeoVectorTileActor::TraceTerrainHeight(const FVector& WorldPos, FVector& OutHitPos)
{
    if (!Georeference)
    {
        return false;
    }
    
    FVector ECEFPos = Georeference->TransformUnrealPositionToEarthCenteredEarthFixed(WorldPos);
    FVector GeodeticNormal = ECEFPos.GetSafeNormal();
    
    FVector TraceDir = Georeference->TransformEarthCenteredEarthFixedDirectionToUnreal(GeodeticNormal);
    
    double TraceDistance = 500000.0;
    FVector TraceStart = WorldPos + TraceDir * TraceDistance;
    FVector TraceEnd = WorldPos - TraceDir * TraceDistance;
    
    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    
    bool bHit = GetWorld()->LineTraceSingleByChannel(
        HitResult,
        TraceStart,
        TraceEnd,
        ECC_Visibility,
        QueryParams
    );
    
    if (bHit)
    {
        OutHitPos = HitResult.Location;
        return true;
    }
    
    return false;
}

const TSet<FTileID>& AGeoVectorTileActor::GetCurrentTiles() const
{
    return CurrentTiles;
}

void AGeoVectorTileActor::ProcessCompletedMeshTasks()
{
    int32 CurrentMaxTiles = bEnableAdaptiveLoading 
        ? FrameRateMonitor.GetRecommendedTilesPerFrame(MinTilesPerFrame, MaxTilesPerFrame)
        : MaxTilesPerFrame;
    
    double FrameStartTime = FPlatformTime::Seconds();
    float TimeBudgetSeconds = MeshCreationTimeBudgetMs / 1000.0f;
    int32 MeshesCreatedThisFrame = 0;
    
    TArray<FTileID> CompletedTileIDs;
    for (auto& Pair : PendingMeshTasks)
    {
        if (!Pair.Value->IsDone())
        {
            continue;
        }
        
        CompletedTileIDs.Add(Pair.Key);
    }
    
    for (const FTileID& TileID : CompletedTileIDs)
    {
        double ElapsedTime = FPlatformTime::Seconds() - FrameStartTime;
        if (ElapsedTime >= TimeBudgetSeconds)
        {
            break;
        }
        
        if (MeshesCreatedThisFrame >= CurrentMaxTiles)
        {
            break;
        }
        
        TSharedPtr<FAsyncMeshCalculationTask> Task = PendingMeshTasks.FindRef(TileID);
        if (!Task.IsValid())
        {
            continue;
        }
        
        PendingMeshTasks.Remove(TileID);
        
        FRawMeshData RawData = Task->GetResult();
        
        if (RawData.bIsValid && IsValidTile(TileID))
        {
            CreateMeshFromRawData(RawData);
            MeshesCreatedThisFrame++;
        }
    }
}

void AGeoVectorTileActor::CreateMeshFromRawData(const FRawMeshData& RawData)
{
    if (!Georeference)
    {
        FindGeoreference();
        if (!Georeference) return;
    }

    // Convert ECEF vertices to Unreal world positions (game thread only)
    TArray<FVector> WorldVertices;
    WorldVertices.Reserve(RawData.Vertices.Num());
    for (const FVector& EcefPos : RawData.Vertices)
    {
        WorldVertices.Add(Georeference->TransformEarthCenteredEarthFixedPositionToUnreal(EcefPos));
    }

    TArray<FVector> WorldNormals;
    WorldNormals.Reserve(RawData.Normals.Num());
    for (const FVector& EcefDir : RawData.Normals)
    {
        WorldNormals.Add(Georeference->TransformEarthCenteredEarthFixedDirectionToUnreal(EcefDir).GetSafeNormal());
    }

    FString MeshName = FString::Printf(
        TEXT("TileMesh_%u_%u_%u"),
        RawData.TileID.X, RawData.TileID.Y, RawData.TileID.Z
    );

    UProceduralMeshComponent* MeshComponent = NewObject<UProceduralMeshComponent>(
        this, FName(*MeshName)
    );
    if (!MeshComponent)
    {
        return;
    }

    MeshComponent->RegisterComponent();
    MeshComponent->AttachToComponent(
        RootComponent, FAttachmentTransformRules::KeepRelativeTransform
    );

    MeshComponent->CreateMeshSection_LinearColor(
        0,
        WorldVertices,
        RawData.Triangles,
        WorldNormals,
        RawData.UVs,
        RawData.VertexColors,
        RawData.Tangents,
        true
    );

    if (DefaultMaterial)
    {
        MeshComponent->SetMaterial(0, DefaultMaterial);
    }

    TileMeshComponents.Add(RawData.TileID, MeshComponent);
}

void AGeoVectorTileActor::LoadTilesFromSet(const TSet<FTileID>& TileIDs, const FTileID& CenterTile)
{
    TArray<FTileID> SortedTiles;
    SortedTiles.Reserve(TileIDs.Num());

    for (const FTileID& TileID : TileIDs)
    {
        if (!TileMeshComponents.Contains(TileID) &&
            !DownloadingTiles.Contains(TileID) &&
            !TileLoadQueueSet.Contains(TileID))
        {
            SortedTiles.Add(TileID);
        }
    }

    int32 CX = static_cast<int32>(CenterTile.X);
    int32 CY = static_cast<int32>(CenterTile.Y);
    SortedTiles.Sort([CX, CY](const FTileID& A, const FTileID& B)
    {
        int32 DistA = FMath::Abs(static_cast<int32>(A.X) - CX) + FMath::Abs(static_cast<int32>(A.Y) - CY);
        int32 DistB = FMath::Abs(static_cast<int32>(B.X) - CX) + FMath::Abs(static_cast<int32>(B.Y) - CY);
        return DistA < DistB;
    });

    for (const FTileID& TileID : SortedTiles)
    {
        TileLoadQueue.Add(TileID);
        TileLoadQueueSet.Add(TileID);
    }
}

void AGeoVectorTileActor::ClearAllTiles()
{
    for (auto& Pair : TileMeshComponents)
    {
        if (Pair.Value)
        {
            Pair.Value->DestroyComponent();
        }
    }
    TileMeshComponents.Empty();
}

void AGeoVectorTileActor::UnloadTile(const FTileID& TileID)
{
    PendingMeshTasks.Remove(TileID);

    if (ActiveHttpRequests.Contains(TileID))
    {
        if (ActiveHttpRequests[TileID].IsValid())
        {
            ActiveHttpRequests[TileID]->CancelRequest();
        }
        ActiveHttpRequests.Remove(TileID);
    }

    if (DownloadingTiles.Contains(TileID))
    {
        DownloadingTiles.Remove(TileID);
        CurrentlyDownloadingCount = FMath::Max(0, CurrentlyDownloadingCount - 1);
    }

    TileLoadQueue.Remove(TileID);
    TileLoadQueueSet.Remove(TileID);

    if (UProceduralMeshComponent** MeshPtr = TileMeshComponents.Find(TileID))
    {
        if (UProceduralMeshComponent* Mesh = *MeshPtr)
        {
            Mesh->DestroyComponent();
        }
        TileMeshComponents.Remove(TileID);
    }
}

void AGeoVectorTileActor::UnloadTilesFromSet(const TSet<FTileID>& TileIDs)
{
    for (const FTileID& TileID : TileIDs)
    {
        UnloadTile(TileID);
    }
}

void AGeoVectorTileActor::ClearAllTileStates()
{
    TileLoadQueue.Empty();
    TileLoadQueueSet.Empty();
    
    PendingMeshTasks.Empty();
    
    for (auto& Pair : ActiveHttpRequests)
    {
        if (Pair.Value.IsValid())
        {
            Pair.Value->CancelRequest();
        }
    }
    ActiveHttpRequests.Empty();
    
    for (auto& Pair : TileMeshComponents)
    {
        if (Pair.Value)
        {
            Pair.Value->DestroyComponent();
        }
    }
    TileMeshComponents.Empty();
    
    DownloadingTiles.Empty();
    ValidTileIDs.Empty();
    CurrentlyDownloadingCount = 0;
    bAllowTileLoading = false;
}

void AGeoVectorTileActor::SetValidTiles(const TSet<FTileID>& ValidTiles)
{
    ValidTileIDs = ValidTiles;
    bAllowTileLoading = true;
}

bool AGeoVectorTileActor::IsValidTile(const FTileID& TileID) const
{
    if (!bAllowTileLoading)
    {
        return false;
    }
    return ValidTileIDs.Contains(TileID);
}

void AGeoVectorTileActor::SetEnabled(bool bNewEnabled)
{
    // Always sync visibility — this is cheap (a single flag + render invalidate),
    // and handles the case where the actor was hidden in the level/editor
    // while the cached state matches the requested value.
    SetActorHiddenInGame(!bNewEnabled);

    if (bEnabled == bNewEnabled)
    {
        return;
    }
    bEnabled = bNewEnabled;

    if (!bEnabled)
    {
        // Cancel inflight requests and free all meshes/cubes
        for (auto& Kvp : ActiveHttpRequests)
        {
            if (Kvp.Value.IsValid())
            {
                Kvp.Value->CancelRequest();
            }
        }
        ActiveHttpRequests.Empty();
        DownloadingTiles.Empty();
        PendingMeshTasks.Empty();
        TileLoadQueue.Empty();
    TileLoadQueueSet.Empty();
        CurrentlyDownloadingCount = 0;
        ClearAllTilesAndMeshes();
        bWasInDistanceRange = false;
    }
    else
    {
        // Force refresh on next tick
        LastCameraCenterECEF = FVector(MAX_dbl);
        LastCameraDistance = -1.0;
        bWaitingForRefresh = true;
        RefreshTimer = 0.0f;
    }
}

void AGeoVectorTileActor::OnTileDownloadComplete(FTileID TileID, TArray<uint8> Data)
{
    CurrentlyDownloadingCount = FMath::Max(0, CurrentlyDownloadingCount - 1);
    DownloadingTiles.Remove(TileID);
    ActiveHttpRequests.Remove(TileID);
    
    if (!IsValidTile(TileID))
    {
        return;
    }

    TSharedPtr<FAsyncMeshCalculationTask> Task = MakeShareable(
        new FAsyncMeshCalculationTask(Data, TileID, VectorTileType)
    );
    PendingMeshTasks.Add(TileID, Task);
    
    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [Task]()
    {
        Task->DoWork();
    });
}

void AGeoVectorTileActor::ProcessTileQueue()
{
    int32 CurrentMaxTiles = bEnableAdaptiveLoading 
        ? FrameRateMonitor.GetRecommendedTilesPerFrame(MinTilesPerFrame, MaxTilesPerFrame)
        : MaxTilesPerFrame;
    
    int32 TilesToStart = FMath::Min(CurrentMaxTiles, TileLoadQueue.Num());
    TilesToStart = FMath::Min(TilesToStart, MaxConcurrentDownloads - CurrentlyDownloadingCount);
    
    if (bEnableAdaptiveLoading && FrameRateMonitor.IsFrameRateCritical())
    {
        TilesToStart = 0;
    }
    
    for (int32 i = 0; i < TilesToStart && TileLoadQueue.Num() > 0; ++i)
    {
        FTileID TileID = TileLoadQueue[0];
        TileLoadQueue.RemoveAt(0);
        TileLoadQueueSet.Remove(TileID);
        
        StartTileDownload(TileID);
    }
}

void AGeoVectorTileActor::StartTileDownload(const FTileID& TileID)
{
    if (TileMeshComponents.Contains(TileID) || DownloadingTiles.Contains(TileID))
    {
        return;
    }
    
    if (!IsValidTile(TileID))
    {
        return;
    }

    FString URL = RequestURL;
    URL = URL.Replace(TEXT("{z}"), *FString::FromInt(TileID.Z));
    URL = URL.Replace(TEXT("{x}"), *FString::FromInt(TileID.X));
    URL = URL.Replace(TEXT("{y}"), *FString::FromInt(TileID.InvertedY()));
    
    FTileDownloadInfo DownloadInfo;
    DownloadInfo.TileID = TileID;
    DownloadInfo.bIsDownloading = true;
    DownloadingTiles.Add(TileID, DownloadInfo);
    CurrentlyDownloadingCount++;
    
    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(URL);
    Request->SetVerb("GET");
    
    ActiveHttpRequests.Add(TileID, Request);
    
    FTileID CapturedTileID = TileID;
    TWeakObjectPtr<AGeoVectorTileActor> WeakThis(this);
    Request->OnProcessRequestComplete().BindLambda([WeakThis, CapturedTileID](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
    {
        if (!WeakThis.IsValid()) return;
        AGeoVectorTileActor* StrongThis = WeakThis.Get();

        if (bSuccess && Response.IsValid() && Response->GetResponseCode() == 200)
        {
            TArray<uint8> Data = Response->GetContent();
            StrongThis->OnTileDownloadComplete(CapturedTileID, Data);
        }
        else
        {
            StrongThis->CurrentlyDownloadingCount = FMath::Max(0, StrongThis->CurrentlyDownloadingCount - 1);
            StrongThis->DownloadingTiles.Remove(CapturedTileID);
            StrongThis->ActiveHttpRequests.Remove(CapturedTileID);
        }
    });
    
    Request->ProcessRequest();
}