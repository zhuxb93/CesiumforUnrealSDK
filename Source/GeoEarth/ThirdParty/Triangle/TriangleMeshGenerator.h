#pragma once

#include "CoreMinimal.h"
#include "ThirdParty/Triangle/triangle_api.h"

namespace TriangleMeshGenerator
{
	static bool GenerateTriangles(
		const TArray<FVector2D>& InPoints,
		const TArray<int32>& InSegments,
		TArray<int32>& OutTriangles,
		TArray<FVector2D>& OutPoints)
	{
		if (InPoints.Num() < 3)
		{
			return false;
		}

		context* Ctx = triangle_context_create();
		if (!Ctx)
		{
			return false;
		}

		char Options[] = "pzj";
		int Result = triangle_context_options(Ctx, Options);
		if (Result < 0)
		{
			triangle_context_destroy(Ctx);
			return false;
		}

		triangleio Input;
		FMemory::Memzero(&Input, sizeof(triangleio));

		Input.numberofpoints = InPoints.Num();
		Input.pointlist = (REAL*)FMemory::Malloc(2 * Input.numberofpoints * sizeof(REAL));
		for (int32 i = 0; i < InPoints.Num(); i++)
		{
			Input.pointlist[2 * i] = InPoints[i].X;
			Input.pointlist[2 * i + 1] = InPoints[i].Y;
		}

		if (InSegments.Num() > 0)
		{
			Input.numberofsegments = InSegments.Num() / 2;
			Input.segmentlist = (int*)FMemory::Malloc(InSegments.Num() * sizeof(int));
			FMemory::Memcpy(Input.segmentlist, InSegments.GetData(), InSegments.Num() * sizeof(int));
		}

		Result = triangle_mesh_create(Ctx, &Input);

		FMemory::Free(Input.pointlist);
		if (Input.segmentlist)
		{
			FMemory::Free(Input.segmentlist);
		}

		if (Result < 0)
		{
			triangle_context_destroy(Ctx);
			return false;
		}

		triangleio Output;
		FMemory::Memzero(&Output, sizeof(triangleio));

		Result = triangle_mesh_copy(Ctx, &Output, 0, 0);
		if (Result < 0)
		{
			triangle_context_destroy(Ctx);
			return false;
		}

		OutPoints.SetNum(Output.numberofpoints);
		for (int32 i = 0; i < Output.numberofpoints; i++)
		{
			OutPoints[i] = FVector2D(Output.pointlist[2 * i], Output.pointlist[2 * i + 1]);
		}

		OutTriangles.SetNum(Output.numberoftriangles * 3);
		FMemory::Memcpy(OutTriangles.GetData(), Output.trianglelist, Output.numberoftriangles * 3 * sizeof(int));

		if (Output.pointlist) triangle_free(Output.pointlist);
		if (Output.trianglelist) triangle_free(Output.trianglelist);
		if (Output.pointmarkerlist) triangle_free(Output.pointmarkerlist);

		triangle_context_destroy(Ctx);
		return true;
	}
}