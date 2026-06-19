#include "GeoJSONGeometryBuilder.h"
#include "GeoJsonPolygonActor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "CesiumRuntime/Public/CesiumGeoreference.h"
#include "triangle_api.h"

UGeoJSONGeometryBuilder::UGeoJSONGeometryBuilder()
{
}

AActor* UGeoJSONGeometryBuilder::BuildFromFeatureCollection(UWorld* World, const FGeoJsonFeatureCollection& FeatureCollection, const FGeoJsonBuilderSettings& InSettings)
{
	if (!World)
	{
		return nullptr;
	}

	ClearGeometry();
	Settings = InSettings;

	if (!DefaultCubeMesh)
	{
		DefaultCubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	RootActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (!RootActor)
	{
		return nullptr;
	}

	static int32 GeoJsonRootCounter = 0;
	FString UniqueName = FString::Printf(TEXT("GeoJSON_Root_%d"), GeoJsonRootCounter++);
	RootActor->Rename(*UniqueName);
	
	USceneComponent* NewRootComponent = NewObject<USceneComponent>(RootActor, USceneComponent::GetDefaultSceneRootVariableName());
	if (NewRootComponent)
	{
		NewRootComponent->SetupAttachment(nullptr);
		NewRootComponent->RegisterComponent();
		RootActor->AddInstanceComponent(NewRootComponent);
		RootActor->SetRootComponent(NewRootComponent);
	}

	// Process polygons
	int32 PolygonCount = 0;
	for (const FGeoJsonFeature& Feature : FeatureCollection.Features)
	{
		const FGeoJsonGeometry& Geometry = Feature.Geometry;
		if (Geometry.Type == EGeoJsonGeometryType::Polygon || Geometry.Type == EGeoJsonGeometryType::MultiPolygon)
		{
			PolygonCount += Geometry.Polygons.Num();
		}
	}

	if (PolygonCount > 0)
	{
		CreatePolygonMesh(World, FeatureCollection);
	}

	// Process points and lines
	for (const FGeoJsonFeature& Feature : FeatureCollection.Features)
	{
		const FGeoJsonGeometry& Geometry = Feature.Geometry;
		
		if (Geometry.Type == EGeoJsonGeometryType::Polygon || Geometry.Type == EGeoJsonGeometryType::MultiPolygon)
		{
			continue;
		}

		CreateCubesForGeometry(World, Geometry);
	}

	return RootActor;
}

void UGeoJSONGeometryBuilder::ClearGeometry()
{
	for (AActor* Actor : CubeActors)
	{
		if (Actor)
		{
			Actor->Destroy();
		}
	}
	CubeActors.Empty();

	if (PolygonActor)
	{
		PolygonActor->Destroy();
		PolygonActor = nullptr;
	}

	if (RootActor)
	{
		RootActor->Destroy();
		RootActor = nullptr;
	}
}

void UGeoJSONGeometryBuilder::CreatePolygonMesh(UWorld* World, const FGeoJsonFeatureCollection& FeatureCollection)
{
	if (!World || !RootActor)
	{
		return;
	}

	if (!DefaultPolygonMaterial)
	{
		DefaultPolygonMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	PolygonActor = World->SpawnActor<AGeoJsonPolygonActor>(AGeoJsonPolygonActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (!PolygonActor)
	{
		return;
	}

	static int32 PolygonCounter = 0;
	FString UniqueName = FString::Printf(TEXT("Polygon_%d"), PolygonCounter++);
	PolygonActor->Rename(*UniqueName);
	PolygonActor->AttachToActor(RootActor, FAttachmentTransformRules::KeepWorldTransform);

	bool bHasMesh = false;

	for (const FGeoJsonFeature& Feature : FeatureCollection.Features)
	{
		const FGeoJsonGeometry& Geometry = Feature.Geometry;
		
		if (Geometry.Type != EGeoJsonGeometryType::Polygon && Geometry.Type != EGeoJsonGeometryType::MultiPolygon)
		{
			continue;
		}

		for (const FGeoJsonCoordinateArray& Ring : Geometry.Polygons)
		{
			if (Ring.Coordinates.Num() < 3)
			{
				continue;
			}

			TArray<FVector> WorldPositions;
			WorldPositions.Reserve(Ring.Coordinates.Num());

			for (const FGeoJsonCoordinate& Coord : Ring.Coordinates)
			{
				WorldPositions.Add(ConvertGeoCoordinateToWorldPosition(Coord));
			}

			// Remove duplicate last point
			if (WorldPositions.Num() > 1 && WorldPositions[0].Equals(WorldPositions.Last(), 1.0))
			{
				WorldPositions.Pop();
			}

			// Remove consecutive duplicate points
			for (int32 i = WorldPositions.Num() - 1; i > 0; i--)
			{
				if (WorldPositions[i].Equals(WorldPositions[i - 1], 0.1))
				{
					WorldPositions.RemoveAt(i);
				}
			}

			if (WorldPositions.Num() < 3)
			{
				continue;
			}

			FVector Origin;
			int32 BestAxis;
			TArray<FVector2D> Points2D = ProjectPointsTo2D(WorldPositions, Origin, BestAxis);
			
			// Remove consecutive duplicate 2D points
			for (int32 i = Points2D.Num() - 1; i > 0; i--)
			{
				if (Points2D[i].Equals(Points2D[i - 1], 0.0001))
				{
					Points2D.RemoveAt(i);
				}
			}
			
			if (Points2D.Num() < 3)
			{
				continue;
			}

			// Validate polygon area
			double TotalArea = 0.0;
			for (int32 i = 0; i < Points2D.Num(); i++)
			{
				int32 j = (i + 1) % Points2D.Num();
				TotalArea += Points2D[i].X * Points2D[j].Y;
				TotalArea -= Points2D[j].X * Points2D[i].Y;
			}
			TotalArea = FMath::Abs(TotalArea) * 0.5;
			
			if (TotalArea < 0.0001)
			{
				continue;
			}

			int32 NumPoints = Points2D.Num();

			context* Ctx = triangle_context_create();
			if (!Ctx)
			{
				continue;
			}

			char Options[] = "pzj";
			if (triangle_context_options(Ctx, Options) < 0)
			{
				triangle_context_destroy(Ctx);
				continue;
			}

			triangleio Input;
			FMemory::Memzero(&Input, sizeof(triangleio));

			Input.numberofpoints = NumPoints;
			Input.pointlist = (REAL*)FMemory::Malloc(2 * NumPoints * sizeof(REAL));
			for (int32 i = 0; i < NumPoints; i++)
			{
				Input.pointlist[2 * i] = Points2D[i].X;
				Input.pointlist[2 * i + 1] = Points2D[i].Y;
			}

			Input.numberofsegments = NumPoints;
			Input.segmentlist = (int*)FMemory::Malloc(2 * NumPoints * sizeof(int));
			for (int32 i = 0; i < NumPoints; i++)
			{
				Input.segmentlist[2 * i] = i;
				Input.segmentlist[2 * i + 1] = (i + 1) % NumPoints;
			}

			int Result = triangle_mesh_create(Ctx, &Input);

			FMemory::Free(Input.pointlist);
			FMemory::Free(Input.segmentlist);

			if (Result < 0)
			{
				triangle_context_destroy(Ctx);
				continue;
			}

			triangleio Output;
			FMemory::Memzero(&Output, sizeof(triangleio));

			Result = triangle_mesh_copy(Ctx, &Output, 0, 0);
			if (Result < 0)
			{
				triangle_context_destroy(Ctx);
				continue;
			}

			if (Output.numberoftriangles > 0 && Output.numberofpoints > 0)
			{
				TArray<FVector> Vertices;
				TArray<int32> Triangles;

				if (Output.numberofpoints == NumPoints)
				{
					Vertices = WorldPositions;
				}
				else
				{
					for (int32 i = 0; i < Output.numberofpoints; i++)
					{
						REAL Px = Output.pointlist[2 * i];
						REAL Py = Output.pointlist[2 * i + 1];
						
						FVector Vertex3D;
						if (BestAxis == 0)
						{
							Vertex3D = FVector(Origin.X, Origin.Y + Px, Origin.Z + Py);
						}
						else if (BestAxis == 1)
						{
							Vertex3D = FVector(Origin.X + Px, Origin.Y, Origin.Z + Py);
						}
						else
						{
							Vertex3D = FVector(Origin.X + Px, Origin.Y + Py, Origin.Z);
						}
						
						Vertices.Add(Vertex3D);
					}
				}

				for (int32 i = 0; i < Output.numberoftriangles; i++)
				{
					Triangles.Add(Output.trianglelist[3 * i]);
					Triangles.Add(Output.trianglelist[3 * i + 2]);
					Triangles.Add(Output.trianglelist[3 * i + 1]);
				}

				PolygonActor->AddPolygonSection(Vertices, Triangles);
				bHasMesh = true;
			}

			if (Output.pointlist) triangle_free(Output.pointlist);
			if (Output.trianglelist) triangle_free(Output.trianglelist);
			if (Output.pointmarkerlist) triangle_free(Output.pointmarkerlist);
			if (Output.segmentlist) triangle_free(Output.segmentlist);
			if (Output.segmentmarkerlist) triangle_free(Output.segmentmarkerlist);

			triangle_context_destroy(Ctx);
		}
	}

	if (bHasMesh)
	{
		UMaterialInterface* MaterialToUse = Settings.PolygonMaterial ? Settings.PolygonMaterial : DefaultPolygonMaterial;
		PolygonActor->FinalizeMesh(MaterialToUse);
	}
	else
	{
		// No valid mesh, destroy the empty actor
		PolygonActor->Destroy();
		PolygonActor = nullptr;
	}
}

void UGeoJSONGeometryBuilder::CreateCubesForGeometry(UWorld* World, const FGeoJsonGeometry& Geometry)
{
	if (!World || !RootActor)
	{
		return;
	}

	TArray<FGeoJsonCoordinate> AllCoords = Geometry.GetAllCoordinates();
	for (const FGeoJsonCoordinate& Coord : AllCoords)
	{
		AActor* NewActor = CreateCubeAtCoordinate(World, Coord);
		if (NewActor)
		{
			CubeActors.Add(NewActor);
		}
	}
}

AActor* UGeoJSONGeometryBuilder::CreateCubeAtCoordinate(UWorld* World, const FGeoJsonCoordinate& Coordinate)
{
	if (!World)
	{
		return nullptr;
	}

	FVector WorldPosition = ConvertGeoCoordinateToWorldPosition(Coordinate);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	AStaticMeshActor* NewActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), WorldPosition, FRotator::ZeroRotator, SpawnParams);
	
	if (!NewActor)
	{
		return nullptr;
	}

	static int32 CubeCounter = 0;
	FString UniqueName = FString::Printf(TEXT("Cube_%d_%.6f_%.6f"), CubeCounter++, Coordinate.Longitude, Coordinate.Latitude);
	NewActor->Rename(*UniqueName);

	UStaticMeshComponent* MeshComponent = NewActor->GetStaticMeshComponent();
	if (MeshComponent)
	{
		UStaticMesh* MeshToUse = Settings.CustomMesh ? Settings.CustomMesh : DefaultCubeMesh;
		if (MeshToUse)
		{
			MeshComponent->SetStaticMesh(MeshToUse);
		}

		MeshComponent->SetMobility(EComponentMobility::Movable);
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

		FVector Scale = FVector(Settings.CubeSize / 100.0f);
		NewActor->SetActorScale3D(Scale);
	}

	NewActor->AttachToActor(RootActor, FAttachmentTransformRules::KeepWorldTransform);

	return NewActor;
}

FVector UGeoJSONGeometryBuilder::ConvertGeoCoordinateToWorldPosition(const FGeoJsonCoordinate& Coordinate)
{
	if (Georeference)
	{
		double Altitude = Coordinate.bHasAltitude ? Coordinate.Altitude : Settings.DefaultAltitude;
		return Georeference->TransformLongitudeLatitudeHeightPositionToUnreal(
			FVector(Coordinate.Longitude, Coordinate.Latitude, Altitude)
		);
	}

	return FVector(Coordinate.Longitude * 100.0, Coordinate.Latitude * 100.0, Coordinate.Altitude * 100.0);
}

TArray<FVector2D> UGeoJSONGeometryBuilder::ProjectPointsTo2D(const TArray<FVector>& Points3D, FVector& OutOrigin, int32& OutBestAxis)
{
	TArray<FVector2D> Points2D;
	
	if (Points3D.Num() == 0)
	{
		OutBestAxis = 2;
		OutOrigin = FVector::ZeroVector;
		return Points2D;
	}

	FVector Min(FLT_MAX), Max(-FLT_MAX);
	for (const FVector& P : Points3D)
	{
		Min.X = FMath::Min(Min.X, P.X);
		Min.Y = FMath::Min(Min.Y, P.Y);
		Min.Z = FMath::Min(Min.Z, P.Z);
		Max.X = FMath::Max(Max.X, P.X);
		Max.Y = FMath::Max(Max.Y, P.Y);
		Max.Z = FMath::Max(Max.Z, P.Z);
	}

	OutOrigin = Min;
	FVector Extent = Max - Min;

	if (Extent.X <= Extent.Y && Extent.X <= Extent.Z)
	{
		OutBestAxis = 0;
		for (const FVector& P : Points3D)
		{
			Points2D.Add(FVector2D(P.Y - Min.Y, P.Z - Min.Z));
		}
	}
	else if (Extent.Y <= Extent.X && Extent.Y <= Extent.Z)
	{
		OutBestAxis = 1;
		for (const FVector& P : Points3D)
		{
			Points2D.Add(FVector2D(P.X - Min.X, P.Z - Min.Z));
		}
	}
	else
	{
		OutBestAxis = 2;
		for (const FVector& P : Points3D)
		{
			Points2D.Add(FVector2D(P.X - Min.X, P.Y - Min.Y));
		}
	}

	return Points2D;
}