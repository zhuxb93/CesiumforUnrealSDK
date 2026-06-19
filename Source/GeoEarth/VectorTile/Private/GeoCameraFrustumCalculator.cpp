#include "GeoCameraFrustumCalculator.h"
#include "../../GeoCameraController.h"
#include "CesiumRuntime/Public/CesiumGeoreference.h"
#include "GeoTileCalculator.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/GameStateBase.h"

namespace GeoCameraFrustum
{
    static TPair<FTileID, FTileID> CalculateFrustumTileRange(
        AGeoCameraController* CameraController,
        ACesiumGeoreference* Georeference,
        int32 Level)
    {
        if (!CameraController || !Georeference)
        {
            return TPair<FTileID, FTileID>(FTileID(), FTileID());
        }

        double Distance = CameraController->GetDistance();
        FVector CenterLLA = CameraController->GetCenterPointLLA();
        double CenterLon = CenterLLA.X;
        double CenterLat = CenterLLA.Y;

        float FOV = 90.0f;
        if (UCameraComponent* Camera = CameraController->Camera)
        {
            FOV = Camera->FieldOfView;
        }

        constexpr double EarthRadius = 6371000.0;
        double HalfFOVRad = FMath::DegreesToRadians(FOV * 0.5f);

        double GroundRadiusMeters = Distance * FMath::Tan(HalfFOVRad);
        
        double HorizonAngle = FMath::Acos(EarthRadius / (EarthRadius + Distance));
        double MaxGroundRadius = EarthRadius * FMath::Tan(HorizonAngle);
        
        if (GroundRadiusMeters > MaxGroundRadius)
        {
            GroundRadiusMeters = MaxGroundRadius;
        }

        double GroundRadiusKm = GroundRadiusMeters / 1000.0;

        double LatDegPerKm = 1.0 / 111.32;
        double ClampedLat = FMath::Clamp(CenterLat, -85.0, 85.0);
        double LonDegPerKm = 1.0 / (111.32 * FMath::Cos(FMath::DegreesToRadians(ClampedLat)));

        double HalfLatRange = GroundRadiusKm * LatDegPerKm;
        double HalfLonRange = GroundRadiusKm * LonDegPerKm;

        double MinLon = CenterLon - HalfLonRange;
        double MaxLon = CenterLon + HalfLonRange;
        double MinLat = CenterLat - HalfLatRange;
        double MaxLat = CenterLat + HalfLatRange;

        MinLon = FMath::Clamp(MinLon, -180.0, 180.0);
        MaxLon = FMath::Clamp(MaxLon, -180.0, 180.0);
        MinLat = FMath::Clamp(MinLat, -85.0, 85.0);
        MaxLat = FMath::Clamp(MaxLat, -85.0, 85.0);

        FTileID MinTile = FGeoTileCalculator::LonLatToTile(MinLon, MinLat, Level);
        FTileID MaxTile = FGeoTileCalculator::LonLatToTile(MaxLon, MaxLat, Level);

        return TPair<FTileID, FTileID>(MinTile, MaxTile);
    }

    TSet<FTileID> GetTilesInFrustum(
        AGeoCameraController* CameraController,
        ACesiumGeoreference* Georeference,
        int32 Level)
    {
        if (!CameraController || !Georeference)
        {
            return TSet<FTileID>();
        }

        TPair<FTileID, FTileID> TileRange = CalculateFrustumTileRange(CameraController, Georeference, Level);
        
        if (TileRange.Key.Z > 0 && TileRange.Value.Z > 0)
        {
            return FGeoTileCalculator::GenerateTileRange(TileRange.Key, TileRange.Value);
        }

        return TSet<FTileID>();
    }

    bool IsBehindEarth(
        ACesiumGeoreference* Georeference,
        const FVector& CameraWorldPos,
        const FVector& PoiWorldPos)
    {
        if (!Georeference)
        {
            return false;
        }

        FVector CameraECEF = Georeference->TransformUnrealPositionToEarthCenteredEarthFixed(CameraWorldPos);
        FVector PoiECEF = Georeference->TransformUnrealPositionToEarthCenteredEarthFixed(PoiWorldPos);

        if (CameraECEF.Size() < 100.0 || PoiECEF.Size() < 100.0)
        {
            return false;
        }

        FVector CameraDir = CameraECEF.GetSafeNormal();
        FVector PoiDir = PoiECEF.GetSafeNormal();

        double DotProduct = FVector::DotProduct(CameraDir, PoiDir);
        double AngleRadians = FMath::Acos(FMath::Clamp(DotProduct, -1.0, 1.0));
        double AngleDegrees = FMath::RadiansToDegrees(AngleRadians);

        return AngleDegrees > 90.0;
    }
}