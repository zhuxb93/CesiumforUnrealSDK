#pragma once

#include "CoreMinimal.h"
#include "RoadData.h"
#include "RawMeshData.h"

class GEOEARTH_API FRoadMeshGenerator
{
public:
    static void GenerateRoadMesh(
        const FRoadData& RoadData,
        TArray<FVector>& OutVertices,
        TArray<int32>& OutTriangles,
        TArray<FVector2D>& OutUVs,
        TArray<FVector>& OutNormals
    );

    static void GenerateTileMesh(
        const FRoadTile& RoadTile,
        FRawMeshData& OutMeshData
    );
};