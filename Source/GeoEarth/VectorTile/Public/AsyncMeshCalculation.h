#pragma once

#include "CoreMinimal.h"
#include <atomic>
#include "RawMeshData.h"
#include "BuildingData.h"
#include "RoadData.h"
#include "TileID.h"
#include "VectorTileType.h"

class FAsyncMeshCalculationTask
{
public:
	FAsyncMeshCalculationTask(
		const TArray<uint8>& InTileData,
		const FTileID& InTileID,
		EVectorTileType InTileType);

	void DoWork();

	FRawMeshData GetResult() const { return Result; }
	bool IsDone() const { return bIsComplete; }

private:
	TArray<uint8> TileData;
	FTileID TileID;
	EVectorTileType TileType;
	FRawMeshData Result;
	std::atomic<bool> bIsComplete;
};
