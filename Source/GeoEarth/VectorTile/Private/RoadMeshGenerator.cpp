#include "RoadMeshGenerator.h"
#include "CoordinateConverter.h"

void FRoadMeshGenerator::GenerateRoadMesh(
    const FRoadData& RoadData,
    TArray<FVector>& OutVertices,
    TArray<int32>& OutTriangles,
    TArray<FVector2D>& OutUVs,
    TArray<FVector>& OutNormals
)
{
    const float HalfWidth = 15.0f;
    const FVector Up = FVector(0.0f, -1.0f, 0.0f);
    const float MinSegmentLength = 0.01f;
    
    if (RoadData.Positions.Num() < 2)
    {
        return;
    }
    
    float SumLen = 0.0f;
    TArray<float> Lengths;
    TArray<FVector> Inners;
    TArray<FVector> Outers;
    TArray<bool> ValidSegments;
    
    Lengths.Reserve(RoadData.Positions.Num());
    Lengths.Add(0.0f);
    
    for (int32 i = 0; i < RoadData.Positions.Num() - 1; i++)
    {
        const FVector& P0 = RoadData.Positions[i];
        const FVector& P1 = RoadData.Positions[i + 1];
        FVector Dir = P1 - P0;
        float Len = Dir.Size();
        
        if (Len < MinSegmentLength)
        {
            ValidSegments.Add(false);
            Inners.Add(FVector::ZeroVector);
            Outers.Add(FVector::ZeroVector);
            Lengths.Add(SumLen);
            continue;
        }
        
        Dir.Normalize();
        
        FVector Inner = FVector::CrossProduct(Up, Dir);
        FVector Outer = FVector::CrossProduct(Dir, Up);
        
        Inners.Add(Inner);
        Outers.Add(Outer);
        ValidSegments.Add(true);
        
        SumLen += Len;
        Lengths.Add(SumLen);
    }
    
    for (int32 i = 0; i < RoadData.Positions.Num() - 1; i++)
    {
        if (!ValidSegments[i])
        {
            continue;  // 跳过退化段
        }
        
        int32 StartIndex = OutVertices.Num();
        int32 CurIndex = i;
        int32 NextIndex = i + 1;
        
        const FVector& P0 = RoadData.Positions[CurIndex];
        const FVector& P1 = RoadData.Positions[NextIndex];
        
        const FVector& Inner = Inners[CurIndex];
        const FVector& Outer = Outers[CurIndex];
        
        float CurTexcoordY = Lengths[CurIndex];
        
        FVector V0 = P0 + Inner * HalfWidth;
        OutVertices.Add(V0);
        OutUVs.Add(FVector2D(0.0f, CurTexcoordY));
        OutNormals.Add(Up);
        
        FVector V1 = P0 + Outer * HalfWidth;
        OutVertices.Add(V1);
        OutUVs.Add(FVector2D(1.0f, CurTexcoordY));
        OutNormals.Add(Up);
        
        float NextTexcoordY = Lengths[NextIndex];
        
        FVector V2 = P1 + Inner * HalfWidth;
        OutVertices.Add(V2);
        OutUVs.Add(FVector2D(0.0f, NextTexcoordY));
        OutNormals.Add(Up);
        
        FVector V3 = P1 + Outer * HalfWidth;
        OutVertices.Add(V3);
        OutUVs.Add(FVector2D(1.0f, NextTexcoordY));
        OutNormals.Add(Up);
        
        OutTriangles.Add(StartIndex);
        OutTriangles.Add(StartIndex + 1);
        OutTriangles.Add(StartIndex + 2);
        
        OutTriangles.Add(StartIndex + 1);
        OutTriangles.Add(StartIndex + 3);
        OutTriangles.Add(StartIndex + 2);
    }
}

void FRoadMeshGenerator::GenerateTileMesh(
    const FRoadTile& RoadTile,
    FRawMeshData& OutMeshData
)
{
    FMatrix LocalToEcef = FCoordinateConverter::GetLocalToEcefMatrix(RoadTile.TileBasePoint);

    TArray<FVector> AllVertices;
    TArray<int32> AllTriangles;
    TArray<FVector2D> AllUVs;
    TArray<FVector> AllNormals;

    for (const FRoadData& Road : RoadTile.Roads)
    {
        TArray<FVector> RoadVertices;
        TArray<int32> RoadTriangles;
        TArray<FVector2D> RoadUVs;
        TArray<FVector> RoadNormals;

        GenerateRoadMesh(Road, RoadVertices, RoadTriangles, RoadUVs, RoadNormals);

        int32 VertexOffset = AllVertices.Num();

        for (const FVector& Vert : RoadVertices)
        {
            FVector FullECEF = LocalToEcef.TransformPosition(Vert);
            AllVertices.Add(FullECEF);
        }

        for (int32 Index : RoadTriangles)
        {
            AllTriangles.Add(Index + VertexOffset);
        }

        AllUVs.Append(RoadUVs);

        for (const FVector& Normal : RoadNormals)
        {
            FVector EcefDir = LocalToEcef.TransformVector(Normal).GetSafeNormal();
            AllNormals.Add(EcefDir);
        }
    }

    OutMeshData.Vertices = AllVertices;
    OutMeshData.Triangles = AllTriangles;
    OutMeshData.UVs = AllUVs;
    OutMeshData.Normals = AllNormals;
    OutMeshData.bIsValid = true;
}