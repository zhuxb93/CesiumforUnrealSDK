#include "GeoTileCalculator.h"

TMap<int32, double> FGeoTileCalculator::LevelToSpanCache;
TMap<int32, int32> FGeoTileCalculator::LevelToNCache;

FTileID FGeoTileCalculator::LonLatToTile(double Lon, double Lat, int32 Level)
{
    int32 N = GetN(Level);
    double Interval = 180.0 / N;
    
    double TileX = (Lon + 180.0) / Interval;
    double TileY = (Lat + 90.0) / Interval;
    
    return FTileID::Create(
        static_cast<uint32>(FMath::FloorToInt(TileX)),
        static_cast<uint32>(FMath::FloorToInt(TileY)),
        static_cast<uint32>(Level)
    );
}

FVector2D FGeoTileCalculator::GetTileCenter(const FTileID& TileID)
{
    double Span = GetSpan(static_cast<int32>(TileID.Z));
    double Lon = TileID.X * Span - 180.0 + Span * 0.5;
    double Lat = TileID.Y * Span - 90.0 + Span * 0.5;
    return FVector2D(Lon, Lat);
}

TSet<FTileID> FGeoTileCalculator::GenerateTileRange(const FTileID& MinTile, const FTileID& MaxTile)
{
    TSet<FTileID> Result;
    
    uint32 MinX = FMath::Min(MinTile.X, MaxTile.X);
    uint32 MaxX = FMath::Max(MinTile.X, MaxTile.X);
    uint32 MinY = FMath::Min(MinTile.Y, MaxTile.Y);
    uint32 MaxY = FMath::Max(MinTile.Y, MaxTile.Y);
    
    int32 CountX = static_cast<int32>(MaxX - MinX + 1);
    int32 CountY = static_cast<int32>(MaxY - MinY + 1);
    Result.Reserve(CountX * CountY);
    
    for (uint32 X = MinX; X <= MaxX; ++X)
    {
        for (uint32 Y = MinY; Y <= MaxY; ++Y)
        {
            Result.Add(FTileID::Create(X, Y, MinTile.Z));
        }
    }
    
    return Result;
}

int32 FGeoTileCalculator::GetN(int32 Level)
{
    if (Level < 0 || Level >= 31)
    {
        return 1;
    }
    if (!LevelToNCache.Contains(Level))
    {
        LevelToNCache.Add(Level, 1 << Level);
    }
    return LevelToNCache[Level];
}

double FGeoTileCalculator::GetSpan(int32 Level)
{
    if (!LevelToSpanCache.Contains(Level))
    {
        LevelToSpanCache.Add(Level, 180.0 / GetN(Level));
    }
    return LevelToSpanCache[Level];
}

double FGeoTileCalculator::Rad(double Degrees)
{
    return Degrees * PI / 180.0;
}

double FGeoTileCalculator::LongitudeOffset(double Latitude, double DistanceKm)
{
    double R = EARTH_RADIUS_KM;
    double D = DistanceKm / R;
    double CosLat = FMath::Cos(Rad(Latitude));
    if (FMath::Abs(CosLat) < 1e-10)
    {
        return 180.0;
    }
    double C = D / CosLat * 180.0 / PI;
    return C;
}

double FGeoTileCalculator::LatitudeOffset(double DistanceKm)
{
    double R = EARTH_RADIUS_KM;
    double D = DistanceKm / R;
    double C = D / PI * 180.0;
    return C;
}

FTileID FGeoTileCalculator::LonLatToTile4326(double Lat, double Lon, int32 Level)
{
    int32 N = GetN(Level);
    double Interval = 180.0 / N;
    
    double MX = Lon + 180.0;
    double MY = Lat + 90.0;
    
    MX /= Interval;
    MY /= Interval;
    
    return FTileID::Create(
        static_cast<uint32>(FMath::FloorToInt(MX)),
        static_cast<uint32>(FMath::FloorToInt(MY)),
        static_cast<uint32>(Level)
    );
}

TSet<FTileID> FGeoTileCalculator::GetAreaTileIndex(double Longitude, double Latitude, int32 Level, double XDistanceKm, double YDistanceKm)
{
    double LeftLon = Longitude - LongitudeOffset(Latitude, XDistanceKm);
    double TopLat = Latitude + LatitudeOffset(YDistanceKm);

    double RightLon = Longitude + LongitudeOffset(Latitude, XDistanceKm);
    double BottomLat = Latitude - LatitudeOffset(YDistanceKm);

    FTileID TopLeftTile = LonLatToTile4326(TopLat, LeftLon, Level);
    FTileID BottomRightTile = LonLatToTile4326(BottomLat, RightLon, Level);

    return GenerateTileRange(TopLeftTile, BottomRightTile);
}