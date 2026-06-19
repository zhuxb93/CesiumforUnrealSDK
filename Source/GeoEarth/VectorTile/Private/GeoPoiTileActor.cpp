#include "GeoPoiTileActor.h"
#include "../../GeoCameraController.h"
#include "CesiumRuntime/Public/CesiumGeoreference.h"
#include "CesiumRuntime/Public/CesiumWgs84Ellipsoid.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Json.h"
#include "GeoTileCalculator.h"
#include "GeoCameraFrustumCalculator.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "PoiWidgetActor.h"
#include "PoiOcclusionManager.h"

TArray<FPoiDistanceLevel>& AGeoPoiTileActor::GetDefaultDistanceLevels()
{
    static TArray<FPoiDistanceLevel> DefaultLevels;
    static bool bInitialized = false;

    if (!bInitialized)
    {
        DefaultLevels.Add(FPoiDistanceLevel(15000000.0, 1.0));
        DefaultLevels.Add(FPoiDistanceLevel(3759724.25, 1.0));
        DefaultLevels.Add(FPoiDistanceLevel(1120932.25, 1.0));
        DefaultLevels.Add(FPoiDistanceLevel(321220.0, 1.0));
        DefaultLevels.Add(FPoiDistanceLevel(92413.7578125, 1.0));
        DefaultLevels.Add(FPoiDistanceLevel(37300.9179687, 1.0));
        DefaultLevels.Add(FPoiDistanceLevel(8774.798828125, 1.0));
        DefaultLevels.Add(FPoiDistanceLevel(2757.297607421875, 1.0));
        DefaultLevels.Add(FPoiDistanceLevel(1244.6435546875, 1.0));
        DefaultLevels.Add(FPoiDistanceLevel(791.65960693359375, 1.0));
        bInitialized = true;
    }
    return DefaultLevels;
}

TMap<int32, int32>& AGeoPoiTileActor::GetCountryPoiLevelMapping()
{
    static TMap<int32, int32> Mapping;
    static bool bInitialized = false;
    
    if (!bInitialized)
    {
        Mapping.Add(0, 0);
        Mapping.Add(1, 2);
        Mapping.Add(2, 4);
        Mapping.Add(3, 5);
        Mapping.Add(4, 6);
        Mapping.Add(5, 7);
        Mapping.Add(6, 8);
        Mapping.Add(7, 9);
        bInitialized = true;
    }
    return Mapping;
}

TMap<int32, int32>& AGeoPoiTileActor::GetCityPoiLevelMapping()
{
    static TMap<int32, int32> Mapping;
    static bool bInitialized = false;
    
    if (!bInitialized)
    {
        Mapping.Add(6, 11);
        Mapping.Add(7, 13);
        Mapping.Add(8, 15);
        Mapping.Add(9, 16);
        bInitialized = true;
    }
    return Mapping;
}

AGeoPoiTileActor::AGeoPoiTileActor()
{
    PrimaryActorTick.bCanEverTick = true;
    CurrentDistanceIndex = 0;
    RefreshTimer = 0.0f;
    OcclusionTimer = 0.0f;
    CurrentlyDownloadingCount = 0;
    bEnableOcclusionCulling = true;
    OcclusionCheckInterval = 1.0f;
}

void AGeoPoiTileActor::BeginPlay()
{
    Super::BeginPlay();
    FindCameraController();
    FindGeoreference();
    InitializeOcclusionManager();
}

void AGeoPoiTileActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearAllTiles();
    ClearPoiLabels();
    ClearAllCubes();
    if (OcclusionManager)
    {
        OcclusionManager->Cleanup();
    }
    for (auto& Pair : ActiveHttpRequests)
    {
        if (Pair.Value.IsValid())
        {
            Pair.Value->CancelRequest();
        }
    }
    ActiveHttpRequests.Empty();
    Super::EndPlay(EndPlayReason);
}

void AGeoPoiTileActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!CameraController || !Georeference)
    {
        FindCameraController();
        FindGeoreference();
        return;
    }

    double Distance = CameraController->GetDistance();
    int32 DistanceIndex = FindDistanceIndex(Distance);

    if (DistanceIndex < MinDistanceIndex || DistanceIndex > MaxDistanceIndex)
    {
        if (CurrentTiles.Num() > 0)
        {
            ClearAllTiles();
        }
        UpdatePoiVisibility();
        return;
    }

    RefreshTimer += DeltaTime;
    if (RefreshTimer >= RefreshWaitTime)
    {
        RefreshTimer = 0.0f;
        UpdateTiles();
    }

    UpdatePoiVisibility();
    ProcessTileQueue();
    
    if (bEnableOcclusionCulling)
    {
        UpdateOcclusionCulling();
    }
}

void AGeoPoiTileActor::InitializeOcclusionManager()
{
    if (!OcclusionManager && bEnableOcclusionCulling)
    {
        OcclusionManager = NewObject<UPoiOcclusionManager>(this);
        OcclusionManager->Initialize(this);
        OcclusionManager->CheckInterval = OcclusionCheckInterval;
        OcclusionManager->DefaultMarkerSize = MarkerSize;
    }
}

void AGeoPoiTileActor::UpdateOcclusionCulling()
{
    if (!OcclusionManager)
    {
        return;
    }

    OcclusionTimer += GetWorld()->GetDeltaSeconds();
    if (OcclusionTimer >= OcclusionCheckInterval)
    {
        OcclusionTimer = 0.0f;
        OcclusionManager->CheckOcclusion();
    }
}

void AGeoPoiTileActor::FindCameraController()
{
    if (CameraController) return;
    
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGeoCameraController::StaticClass(), FoundActors);
    if (FoundActors.Num() > 0)
    {
        CameraController = Cast<AGeoCameraController>(FoundActors[0]);
    }
}

void AGeoPoiTileActor::FindGeoreference()
{
    if (Georeference) return;
    Georeference = ACesiumGeoreference::GetDefaultGeoreference(this);
}

void AGeoPoiTileActor::UpdateTiles()
{
    if (!CameraController) return;

    double Distance = CameraController->GetDistance();
    CurrentDistanceIndex = FindDistanceIndex(Distance);

    if (CurrentDistanceIndex < MinDistanceIndex || CurrentDistanceIndex > MaxDistanceIndex)
    {
        ClearAllTiles();
        return;
    }

    TSet<FTileID> NewTiles;

    if (CurrentDistanceIndex == 1)
    {
        NewTiles = GenerateLevel2Tiles();
    }
    else
    {
        int32 TileLevel = GetTileLevelFromDistanceIndex(CurrentDistanceIndex);
        NewTiles = GetTilesInFrustum(TileLevel);
    }

    TSet<FTileID> TilesToAdd = NewTiles.Difference(CachedTiles);
    TSet<FTileID> TilesToRemove = CachedTiles.Difference(NewTiles);
    CachedTiles = NewTiles;

    for (const FTileID& TileID : TilesToRemove)
    {
        TileLoadQueue.Remove(TileID);
        TileLoadQueueSet.Remove(TileID);
        
        if (ActiveHttpRequests.Contains(TileID))
        {
            if (ActiveHttpRequests[TileID].IsValid())
            {
                ActiveHttpRequests[TileID]->CancelRequest();
            }
            ActiveHttpRequests.Remove(TileID);
        }
        ClearPoiLabelsForTile(TileID);
        CurrentTiles.Remove(TileID);
        DestroyCubeForTile(TileID);
    }

    for (const FTileID& TileID : TilesToAdd)
    {
        CurrentTiles.Add(TileID);

        if (bShowDebugCubes)
        {
            SpawnCubeForTile(TileID);
        }

        if (!ActiveHttpRequests.Contains(TileID) && !TileLoadQueueSet.Contains(TileID))
        {
            TileLoadQueue.Add(TileID);
            TileLoadQueueSet.Add(TileID);
        }
    }
}

void AGeoPoiTileActor::ProcessTileQueue()
{
    if (TileLoadQueue.Num() == 0) return;

    int32 SlotsAvailable = MaxConcurrentDownloads - CurrentlyDownloadingCount;
    if (SlotsAvailable <= 0) return;

    int32 TilesToStart = FMath::Min(SlotsAvailable, TileLoadQueue.Num());

    for (int32 i = 0; i < TilesToStart; i++)
    {
        FTileID TileID = TileLoadQueue[0];
        TileLoadQueue.RemoveAt(0);
        TileLoadQueueSet.Remove(TileID);

        if (!ActiveHttpRequests.Contains(TileID))
        {
            StartTileDownload(TileID);
        }
    }
}

int32 AGeoPoiTileActor::FindDistanceIndex(double Distance) const
{
    for (int32 i = 0; i < GetDefaultDistanceLevels().Num(); i++)
    {
        if (Distance >= GetDefaultDistanceLevels()[i].Distance)
        {
            return i;
        }
    }
    return GetDefaultDistanceLevels().Num() - 1;
}

int32 AGeoPoiTileActor::GetTileLevelFromDistanceIndex(int32 DistanceIndex) const
{
    switch (PoiType)
    {
    case EPoiType::Country:
        return GetCountryPoiLevelMapping().Contains(DistanceIndex) ? GetCountryPoiLevelMapping()[DistanceIndex] : 0;
    case EPoiType::City:
        return GetCityPoiLevelMapping().Contains(DistanceIndex) ? GetCityPoiLevelMapping()[DistanceIndex] : 11;
    default:
        if (GetCountryPoiLevelMapping().Contains(DistanceIndex))
        {
            return GetCountryPoiLevelMapping()[DistanceIndex];
        }
        if (GetCityPoiLevelMapping().Contains(DistanceIndex))
        {
            return GetCityPoiLevelMapping()[DistanceIndex];
        }
        return DistanceIndex;
    }
}

TSet<FTileID> AGeoPoiTileActor::GenerateLevel2Tiles() const
{
    TSet<FTileID> Tiles;
    int32 Level = 2;
    int32 Cols = 8;
    int32 Rows = 4;
    
    for (int32 X = 0; X < Cols; X++)
    {
        for (int32 Y = 0; Y < Rows; Y++)
        {
            Tiles.Add(FTileID::Create(X, Y, Level));
        }
    }
    return Tiles;
}

TSet<FTileID> AGeoPoiTileActor::GetTilesInFrustum(int32 TileLevel) const
{
    if (!CameraController || !Georeference) return TSet<FTileID>();

    return GeoCameraFrustum::GetTilesInFrustum(CameraController, Georeference, TileLevel);
}

void AGeoPoiTileActor::StartTileDownload(const FTileID& TileID)
{
    FString URL = RequestURL;
    URL.ReplaceInline(TEXT("{z}"), *FString::FromInt(TileID.Z));
    URL.ReplaceInline(TEXT("{x}"), *FString::FromInt(TileID.X));
    URL.ReplaceInline(TEXT("{y}"), *FString::FromInt(TileID.InvertedY()));

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(URL);
    HttpRequest->SetVerb(TEXT("GET"));

    TWeakObjectPtr<AGeoPoiTileActor> WeakThis(this);
    HttpRequest->OnProcessRequestComplete().BindLambda([WeakThis, TileID](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
    {
        if (!WeakThis.IsValid()) return;
        
        AGeoPoiTileActor* StrongThis = WeakThis.Get();
        StrongThis->ActiveHttpRequests.Remove(TileID);
        StrongThis->CurrentlyDownloadingCount = FMath::Max(0, StrongThis->CurrentlyDownloadingCount - 1);

        if (bSuccess && Response.IsValid())
        {
            int32 StatusCode = Response->GetResponseCode();
            int32 ContentSize = Response->GetContent().Num();

            if (StatusCode == 200 && ContentSize > 0)
            {
                StrongThis->OnTileDownloadComplete(TileID, Response->GetContent());
            }
        }

        StrongThis->ProcessTileQueue();
    });

    ActiveHttpRequests.Add(TileID, HttpRequest);
    CurrentlyDownloadingCount++;
    HttpRequest->ProcessRequest();
}

void AGeoPoiTileActor::OnTileDownloadComplete(FTileID TileID, TArray<uint8> Data)
{
    TArray<FPoiData> PoiData;
    ParseGeoJsonData(Data, PoiData);
    
    if (PoiData.Num() > 0)
    {
        CreatePoiLabels(PoiData, TileID);
    }
}

void AGeoPoiTileActor::ParseGeoJsonData(const TArray<uint8>& Data, TArray<FPoiData>& OutPoiData)
{
    TArray<char> Utf8Data;
    Utf8Data.SetNum(Data.Num() + 1);
    FMemory::Memcpy(Utf8Data.GetData(), Data.GetData(), Data.Num());
    Utf8Data[Data.Num()] = '\0';
    FString JsonString = FString(UTF8_TO_TCHAR(Utf8Data.GetData()));
    
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(JsonString);
    
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        return;
    }
    
    const TArray<TSharedPtr<FJsonValue>>* Features;
    if (!JsonObject->TryGetArrayField(TEXT("features"), Features))
    {
        return;
    }
    
    for (const TSharedPtr<FJsonValue>& FeatureValue : *Features)
    {
        TSharedPtr<FJsonObject> Feature = FeatureValue->AsObject();
        if (!Feature.IsValid()) continue;
        
        FPoiData Poi;
        
        Feature->TryGetStringField(TEXT("id"), Poi.Id);
        
        const TSharedPtr<FJsonObject>* Properties;
        if (Feature->TryGetObjectField(TEXT("properties"), Properties) && Properties->IsValid())
        {
            (*Properties)->TryGetStringField(TEXT("name"), Poi.Name);
            (*Properties)->TryGetStringField(TEXT("kind"), Poi.PoiType);
            (*Properties)->TryGetNumberField(TEXT("importance"), Poi.Importance);
        }
        
        const TSharedPtr<FJsonObject>* Geometry;
        if (Feature->TryGetObjectField(TEXT("geometry"), Geometry) && Geometry->IsValid())
        {
            FString Type;
            (*Geometry)->TryGetStringField(TEXT("type"), Type);
            if (Type == TEXT("Point"))
            {
                const TArray<TSharedPtr<FJsonValue>>* Coords;
                if ((*Geometry)->TryGetArrayField(TEXT("coordinates"), Coords) && Coords->Num() >= 2)
                {
                    Poi.Longitude = (*Coords)[0]->AsNumber();
                    Poi.Latitude = (*Coords)[1]->AsNumber();
                    if (Coords->Num() >= 3)
                    {
                        Poi.Altitude = (*Coords)[2]->AsNumber();
                    }
                }
            }
        }
        
        if (!Poi.Name.IsEmpty() && FMath::Abs(Poi.Longitude) <= 180.0 && FMath::Abs(Poi.Latitude) <= 90.0)
        {
            Poi.Id = FString::Printf(TEXT("poi_lon%d_lat%d"), 
                FMath::RoundToInt(Poi.Longitude * 10000), 
                FMath::RoundToInt(Poi.Latitude * 10000));
            
            if (Georeference)
            {
                Poi.WorldPosition = Georeference->TransformLongitudeLatitudeHeightPositionToUnreal(
                    FVector(Poi.Longitude, Poi.Latitude, Poi.Altitude));
            }
            OutPoiData.Add(Poi);
        }
    }
}

void AGeoPoiTileActor::CreatePoiLabels(const TArray<FPoiData>& PoiData, const FTileID& TileID)
{
    TArray<FString> TilePoiIdList;

    for (const FPoiData& Poi : PoiData)
    {
        if (!PoiWidgetClass)
        {
            continue;
        }

        if (!PoiWidgetActors.Contains(Poi.Id))
        {
            FActorSpawnParameters SpawnParams;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            APoiWidgetActor* PoiWidgetActor = GetWorld()->SpawnActor<APoiWidgetActor>(PoiWidgetClass, Poi.WorldPosition, FRotator::ZeroRotator, SpawnParams);
            if (!PoiWidgetActor)
            {
                continue;
            }

            PoiWidgetActor->SetPoiName(Poi.Name);
            PoiWidgetActor->SetPoiData(Poi.Name, Poi.PoiType, Poi.Longitude, Poi.Latitude, Poi.Altitude);

            PoiWidgetActors.Add(Poi.Id, PoiWidgetActor);
        }

        TilePoiIdList.Add(Poi.Id);
    }

    if (TilePoiIdList.Num() > 0)
    {
        TilePoiIds.Add(TileID, TilePoiIdList);
    }
}

void AGeoPoiTileActor::UpdatePoiVisibility()
{
    if (!CameraController || !CameraController->Camera || !Georeference) return;

    FVector CameraLoc = CameraController->Camera->GetComponentLocation();

    for (auto& Pair : PoiWidgetActors)
    {
        if (Pair.Value)
        {
            UWidgetComponent* WidgetComp = Pair.Value->GetWidgetComponent();
            if (!WidgetComp) continue;

            FVector WidgetLoc = WidgetComp->GetComponentLocation();

            bool bBehindEarth = GeoCameraFrustum::IsBehindEarth(Georeference, CameraLoc, WidgetLoc);
            if (bBehindEarth)
            {
                Pair.Value->SetActorHiddenInGame(true);
                continue;
            }
            else if (!Pair.Value->IsOccluded())
            {
                Pair.Value->SetActorHiddenInGame(false);
            }

            FVector ToCamera = CameraLoc - WidgetLoc;
            double DistanceToCamera = ToCamera.Size();

            FVector CameraForward = CameraController->Camera->GetForwardVector();
            FVector CameraRight = CameraController->Camera->GetRightVector();
            FVector CameraUp = CameraController->Camera->GetUpVector();

            FVector PoiForward = -CameraForward;
            FVector PoiRight = -CameraRight;
            FVector PoiUp = CameraUp;

            FMatrix RotMatrix(
                FPlane(PoiForward.X, PoiForward.Y, PoiForward.Z, 0.f),
                FPlane(PoiRight.X, PoiRight.Y, PoiRight.Z, 0.f),
                FPlane(PoiUp.X, PoiUp.Y, PoiUp.Z, 0.f),
                FPlane(0.f, 0.f, 0.f, 1.f)
            );
            FRotator PoiRotation = RotMatrix.Rotator();
            WidgetComp->SetWorldRotation(PoiRotation);

            float DynamicScale = DistanceToCamera * AdjustScaleFactor / 100.0f;
            WidgetComp->SetWorldScale3D(FVector(DynamicScale));
        }
    }
}

void AGeoPoiTileActor::ClearPoiLabels()
{
    for (auto& Pair : PoiWidgetActors)
    {
        if (Pair.Value)
        {
            Pair.Value->Destroy();
        }
    }
    PoiWidgetActors.Empty();
    TilePoiIds.Empty();
}

void AGeoPoiTileActor::ClearPoiLabelsForTile(const FTileID& TileID)
{
    if (TArray<FString>* PoiIds = TilePoiIds.Find(TileID))
    {
        TArray<FString> IdsToCheck = *PoiIds;
        TilePoiIds.Remove(TileID);

        for (const FString& PoiId : IdsToCheck)
        {
            bool bStillReferenced = false;
            for (const auto& Pair : TilePoiIds)
            {
                if (Pair.Value.Contains(PoiId))
                {
                    bStillReferenced = true;
                    break;
                }
            }

            if (!bStillReferenced)
            {
                if (APoiWidgetActor** PoiActor = PoiWidgetActors.Find(PoiId))
                {
                    if (*PoiActor)
                    {
                        (*PoiActor)->Destroy();
                    }
                    PoiWidgetActors.Remove(PoiId);
                }
            }
        }
    }
}

void AGeoPoiTileActor::ClearAllTiles()
{
    for (auto& Pair : ActiveHttpRequests)
    {
        if (Pair.Value.IsValid())
        {
            Pair.Value->CancelRequest();
        }
    }
    ActiveHttpRequests.Empty();
    TileLoadQueue.Empty();
    TileLoadQueueSet.Empty();
    CurrentlyDownloadingCount = 0;
    ClearPoiLabels();
    ClearAllCubes();
    CurrentTiles.Empty();
    CachedTiles.Empty();
}

const TSet<FTileID>& AGeoPoiTileActor::GetCurrentTiles() const
{
    return CurrentTiles;
}

int32 AGeoPoiTileActor::GetCurrentDistanceIndex() const
{
    return CurrentDistanceIndex;
}

void AGeoPoiTileActor::SpawnCubeForTile(const FTileID& TileID)
{
    if (TileCubes.Contains(TileID))
    {
        return;
    }
    
    if (!Georeference)
    {
        return;
    }
    
    FVector WorldPos = GetTileWorldPosition(TileID);
    
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
                MeshComp->SetStaticMesh(CubeMesh);
                MeshComp->SetWorldScale3D(FVector(DebugCubeScale));
                MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                MeshComp->UpdateBounds();
            }
        }
        
        TileCubes.Add(TileID, CubeActor);
    }
}

void AGeoPoiTileActor::DestroyCubeForTile(const FTileID& TileID)
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

void AGeoPoiTileActor::ClearAllCubes()
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

FVector AGeoPoiTileActor::GetTileWorldPosition(const FTileID& TileID)
{
    FVector2D TileCenter = FGeoTileCalculator::GetTileCenter(TileID);
    
    FVector LonLatHeight(TileCenter.X, TileCenter.Y, 0.0);
    FVector ECEF = UCesiumWgs84Ellipsoid::LongitudeLatitudeHeightToEarthCenteredEarthFixed(LonLatHeight);
    
    FVector WorldPos = Georeference->TransformEarthCenteredEarthFixedPositionToUnreal(ECEF);
    
    return WorldPos;
}