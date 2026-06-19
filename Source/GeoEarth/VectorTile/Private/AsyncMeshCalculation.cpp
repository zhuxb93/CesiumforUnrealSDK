#include "AsyncMeshCalculation.h"
#include "VectorTileParser.h"
#include "BuildingMeshGenerator.h"
#include "RoadMeshGenerator.h"

FAsyncMeshCalculationTask::FAsyncMeshCalculationTask(
	const TArray<uint8>& InTileData,
	const FTileID& InTileID,
	EVectorTileType InTileType)
	: TileData(InTileData)
	, TileID(InTileID)
	, TileType(InTileType)
	, bIsComplete(false)
{
}

void FAsyncMeshCalculationTask::DoWork()
{
	Result.TileID = TileID;

	if (TileType == EVectorTileType::Road || TileType == EVectorTileType::Lrrl)
	{
		FRoadTile RoadTile;
		if (!FVectorTileParser::ParseRoadTile(TileData, RoadTile))
		{
			bIsComplete = true;
			return;
		}

		if (RoadTile.Roads.Num() == 0)
		{
			bIsComplete = true;
			return;
		}

		FRoadMeshGenerator::GenerateTileMesh(RoadTile, Result);
	}
	else
	{
		FBuildingTile BuildingTile;
		if (!FVectorTileParser::ParseVectorTile(TileData, BuildingTile))
		{
			bIsComplete = true;
			return;
		}

		if (BuildingTile.Buildings.Num() == 0)
		{
			bIsComplete = true;
			return;
		}

		FBuildingMeshGenerator::GenerateTileMesh(
			BuildingTile,
			Result.Vertices,
			Result.Triangles,
			Result.UVs,
			Result.Normals
		);
	}

	Result.Tangents.SetNum(Result.Vertices.Num());
	Result.VertexColors.Init(FLinearColor::White, Result.Vertices.Num());
	Result.bIsValid = Result.Vertices.Num() > 0;

	bIsComplete = true;
}
