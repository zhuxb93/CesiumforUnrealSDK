#include "GeoJsonPolygonActor.h"
#include "ProceduralMeshComponent.h"

AGeoJsonPolygonActor::AGeoJsonPolygonActor()
{
	PrimaryActorTick.bCanEverTick = false;

	ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("PolygonMesh"));
	SetRootComponent(ProceduralMesh);
}

void AGeoJsonPolygonActor::AddPolygonSection(const TArray<FVector>& Vertices, const TArray<int32>& Triangles)
{
	if (!ProceduralMesh || Vertices.Num() == 0 || Triangles.Num() == 0)
	{
		return;
	}

	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;

	Normals.Init(FVector(0, 0, 1), Vertices.Num());
	UVs.Init(FVector2D::ZeroVector, Vertices.Num());
	VertexColors.Init(FColor::White, Vertices.Num());
	Tangents.Init(FProcMeshTangent(1, 0, 0), Vertices.Num());

	ProceduralMesh->CreateMeshSection(
		CurrentSectionIndex,
		Vertices,
		Triangles,
		Normals,
		UVs,
		VertexColors,
		Tangents,
		true
	);

	CurrentSectionIndex++;
}

void AGeoJsonPolygonActor::FinalizeMesh(UMaterialInterface* Material)
{
	if (!ProceduralMesh || !Material)
	{
		return;
	}

	for (int32 i = 0; i < CurrentSectionIndex; i++)
	{
		ProceduralMesh->SetMaterial(i, Material);
	}
}