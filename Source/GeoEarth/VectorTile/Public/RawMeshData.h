#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "TileID.h"

struct FRawMeshData
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector2D> UVs;
	TArray<FVector> Normals;
	TArray<FProcMeshTangent> Tangents;
	TArray<FLinearColor> VertexColors;
	FTileID TileID;
	bool bIsValid = false;
};