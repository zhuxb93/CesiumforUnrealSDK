#pragma once

#include "CoreMinimal.h"
#include "TileID.h"
#include "BuildingData.h"
#include "RoadData.h"

class FVectorTileParser
{
public:
    static bool DecompressLZ4(const TArray<uint8>& CompressedData, TArray<uint8>& OutDecompressed);
    
    static bool ParseVectorTile(const TArray<uint8>& TileData, FBuildingTile& OutTile);
    
    static bool ParseRoadTile(const TArray<uint8>& TileData, FRoadTile& OutTile);
};