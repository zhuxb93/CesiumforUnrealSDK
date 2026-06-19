#include "BuildingMeshGenerator.h"
#include "CoordinateConverter.h"

static bool IsValidTriangle(const FVector& V0, const FVector& V1, const FVector& V2, float MinAreaThreshold = 1.0f)
{
    FVector Edge1 = V1 - V0;
    FVector Edge2 = V2 - V0;
    FVector Cross = FVector::CrossProduct(Edge1, Edge2);
    float Area = Cross.Size() * 0.5f;
    return Area >= MinAreaThreshold;
}

void FBuildingMeshGenerator::GenerateBuildingMesh(
    const FBuildingData& Building,
    TArray<FVector>& OutVertices,
    TArray<int32>& OutTriangles,
    TArray<FVector2D>& OutUVs,
    TArray<FVector>& OutNormals)
{
    if (Building.BottomPoints.Num() == 0 || Building.Height <= 0)
    {
        return;
    }

    float RoofHeight = Building.CenterZ + Building.Height;

    // In local ECEF space (ENU): X=East, Y=Up, Z=North
    const FVector LocalUp(0.0f, 1.0f, 0.0f);

    // Compute building centroid in XZ plane for outward normal check
    FVector Centroid = FVector::ZeroVector;
    for (const FVector& P : Building.BottomPoints)
    {
        Centroid += FVector(P.X, 0.0f, P.Z);
    }
    Centroid /= Building.BottomPoints.Num();

    int32 StartIndex = OutVertices.Num();
    for (int32 i = 0; i < Building.BottomPoints.Num(); i++)
    {
        const FVector& P = Building.BottomPoints[i];
        FVector RoofPoint = FVector(P.X, P.Y + RoofHeight, P.Z);
        OutVertices.Add(RoofPoint);
        OutUVs.Add(FVector2D(0.0f, 0.0f));
        OutNormals.Add(LocalUp);
    }

    for (int32 i = 0; i < Building.Triangles.Num(); i += 3)
    {
        if (i + 2 >= Building.Triangles.Num())
        {
            break;
        }

        int32 Idx0 = Building.Triangles[i];
        int32 Idx1 = Building.Triangles[i + 1];
        int32 Idx2 = Building.Triangles[i + 2];

        if (Idx0 >= Building.BottomPoints.Num() ||
            Idx1 >= Building.BottomPoints.Num() ||
            Idx2 >= Building.BottomPoints.Num())
        {
            continue;
        }

        int32 VertexIdx0 = StartIndex + Idx0;
        int32 VertexIdx1 = StartIndex + Idx1;
        int32 VertexIdx2 = StartIndex + Idx2;

        FVector V0 = OutVertices[VertexIdx0];
        FVector V1 = OutVertices[VertexIdx1];
        FVector V2 = OutVertices[VertexIdx2];

        if (!IsValidTriangle(V0, V1, V2))
        {
            continue;
        }

        OutTriangles.Add(VertexIdx2);
        OutTriangles.Add(VertexIdx1);
        OutTriangles.Add(VertexIdx0);
    }

    int32 RingCount = Building.BottomPoints.Num();
    for (int32 i = 0; i < RingCount; i++)
    {
        int32 NextIndex = (i + 1) % RingCount;

        bool bIsHoleCurrent = false;
        bool bIsHoleNext = false;

        for (int32 j = 0; j < Building.Holes.Num(); j += 2)
        {
            int32 HoleStart = Building.Holes[j];
            int32 HoleEnd = (j + 1 >= Building.Holes.Num()) ? RingCount - 1 : Building.Holes[j + 1];

            if (i >= HoleStart && i <= HoleEnd)
            {
                bIsHoleCurrent = true;
            }
            if (NextIndex >= HoleStart && NextIndex <= HoleEnd)
            {
                bIsHoleNext = true;
            }
        }

        if (bIsHoleCurrent != bIsHoleNext)
        {
            continue;
        }

        const FVector& P0 = Building.BottomPoints[i];
        const FVector& P1 = Building.BottomPoints[NextIndex];

        // Skip degenerate wall edges (identical XZ positions)
        FVector EdgeDir = P1 - P0;
        EdgeDir.Y = 0.0f;
        if (EdgeDir.IsNearlyZero())
        {
            continue;
        }

        StartIndex = OutVertices.Num();

        FVector WallBottom0 = FVector(P0.X, P0.Y + Building.CenterZ, P0.Z);
        FVector WallBottom1 = FVector(P1.X, P1.Y + Building.CenterZ, P1.Z);
        FVector WallTop0 = FVector(P0.X, P0.Y + RoofHeight, P0.Z);
        FVector WallTop1 = FVector(P1.X, P1.Y + RoofHeight, P1.Z);

        OutVertices.Add(WallBottom0);
        OutVertices.Add(WallBottom1);
        OutVertices.Add(WallTop1);
        OutVertices.Add(WallTop0);

        OutUVs.Add(FVector2D(0.0f, 0.0f));
        OutUVs.Add(FVector2D(1.0f, 0.0f));
        OutUVs.Add(FVector2D(1.0f, 1.0f));
        OutUVs.Add(FVector2D(0.0f, 1.0f));

        // Wall outward normal: cross(Up, horizontal edge direction)
        EdgeDir.Normalize();
        FVector WallNormal = FVector::CrossProduct(LocalUp, EdgeDir);
        // Ensure normal points outward (away from building center in XZ plane)
        FVector WallMidXZ = FVector((P0.X + P1.X) * 0.5f, 0.0f, (P0.Z + P1.Z) * 0.5f);
        if (FVector::DotProduct(WallNormal, WallMidXZ - Centroid) < 0.0f)
        {
            WallNormal = -WallNormal;
        }
        OutNormals.Add(WallNormal);
        OutNormals.Add(WallNormal);
        OutNormals.Add(WallNormal);
        OutNormals.Add(WallNormal);

        if (IsValidTriangle(WallBottom0, WallTop1, WallBottom1))
        {
            OutTriangles.Add(StartIndex);
            OutTriangles.Add(StartIndex + 2);
            OutTriangles.Add(StartIndex + 1);
        }

        if (IsValidTriangle(WallBottom0, WallTop0, WallTop1))
        {
            OutTriangles.Add(StartIndex);
            OutTriangles.Add(StartIndex + 3);
            OutTriangles.Add(StartIndex + 2);
        }
    }
}

void FBuildingMeshGenerator::GenerateTileMesh(
    const FBuildingTile& Tile,
    TArray<FVector>& OutVertices,
    TArray<int32>& OutTriangles,
    TArray<FVector2D>& OutUVs,
    TArray<FVector>& OutNormals)
{
    FMatrix LocalToEcef = FCoordinateConverter::GetLocalToEcefMatrix(Tile.TileBasePoint);

    TArray<FVector> LocalVertices;
    TArray<int32> LocalTriangles;
    TArray<FVector2D> LocalUVs;
    TArray<FVector> LocalNormals;

    for (const FBuildingData& Building : Tile.Buildings)
    {
        LocalVertices.Empty();
        LocalTriangles.Empty();
        LocalUVs.Empty();
        LocalNormals.Empty();

        GenerateBuildingMesh(Building, LocalVertices, LocalTriangles, LocalUVs, LocalNormals);

        if (LocalVertices.Num() == 0)
        {
            continue;
        }

        int32 VertexOffset = OutVertices.Num();

        for (int32 i = 0; i < LocalVertices.Num(); i++)
        {
            FVector FullECEF = LocalToEcef.TransformPosition(LocalVertices[i]);
            OutVertices.Add(FullECEF);

            FVector EcefDir = LocalToEcef.TransformVector(LocalNormals[i]).GetSafeNormal();
            OutNormals.Add(EcefDir);
        }

        for (int32 Index : LocalTriangles)
        {
            OutTriangles.Add(VertexOffset + Index);
        }

        OutUVs.Append(LocalUVs);
    }
}