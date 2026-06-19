#pragma once

#include "CoreMinimal.h"
#include "TileID.h"

class FGeoTileCalculator
{
public:
    static constexpr int32 EARTH_RADIUS_KM = 6378;
    
    static FTileID LonLatToTile(double Lon, double Lat, int32 Level);
    static FVector2D GetTileCenter(const FTileID& TileID);
    static TSet<FTileID> GenerateTileRange(const FTileID& MinTile, const FTileID& MaxTile);
    
    static int32 GetN(int32 Level);
    static double GetSpan(int32 Level);
    
    static TSet<FTileID> GetAreaTileIndex(double Longitude, double Latitude, int32 Level, double XDistanceKm, double YDistanceKm);
    static FTileID LonLatToTile4326(double Lat, double Lon, int32 Level);

private:
    static TMap<int32, double> LevelToSpanCache;
    static TMap<int32, int32> LevelToNCache;

    static double LongitudeOffset(double Latitude, double DistanceKm);
    static double LatitudeOffset(double DistanceKm);
    static double Rad(double Degrees);
};