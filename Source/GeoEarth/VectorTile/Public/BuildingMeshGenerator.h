#pragma once

#include "CoreMinimal.h"
#include "BuildingData.h"

class FBuildingMeshGenerator
{
public:
    static void GenerateBuildingMesh(
        const FBuildingData& Building,
        TArray<FVector>& OutVertices,
        TArray<int32>& OutTriangles,
        TArray<FVector2D>& OutUVs,
        TArray<FVector>& OutNormals);

    static void GenerateTileMesh(
        const FBuildingTile& Tile,
        TArray<FVector>& OutVertices,
        TArray<int32>& OutTriangles,
        TArray<FVector2D>& OutUVs,
        TArray<FVector>& OutNormals);
};