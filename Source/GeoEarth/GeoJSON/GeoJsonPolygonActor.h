#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GeoJsonPolygonActor.generated.h"

class UProceduralMeshComponent;

UCLASS()
class GEOEARTH_API AGeoJsonPolygonActor : public AActor
{
	GENERATED_BODY()
	
public:	
	AGeoJsonPolygonActor();

	void AddPolygonSection(const TArray<FVector>& Vertices, const TArray<int32>& Triangles);
	void FinalizeMesh(UMaterialInterface* Material);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UProceduralMeshComponent* ProceduralMesh;

private:
	int32 CurrentSectionIndex = 0;
};