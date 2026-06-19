#pragma once

#include "CoreMinimal.h"
#include "GeoJSONTypes.h"
#include "GeoJSONGeometryBuilder.generated.h"

class ACesiumGeoreference;
class AGeoJsonPolygonActor;

USTRUCT(BlueprintType)
struct FGeoJsonBuilderSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CubeSize = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DefaultAltitude = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UStaticMesh* CustomMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UMaterialInterface* PolygonMaterial = nullptr;
};

UCLASS(BlueprintType)
class GEOEARTH_API UGeoJSONGeometryBuilder : public UObject
{
	GENERATED_BODY()

public:
	UGeoJSONGeometryBuilder();

	UFUNCTION(BlueprintCallable, Category = "GeoJSON")
	void SetGeoreference(ACesiumGeoreference* InGeoreference) { Georeference = InGeoreference; }

	UFUNCTION(BlueprintCallable, Category = "GeoJSON")
	void SetSettings(const FGeoJsonBuilderSettings& InSettings) { Settings = InSettings; }

	UFUNCTION(BlueprintCallable, Category = "GeoJSON")
	AActor* BuildFromFeatureCollection(UWorld* World, const FGeoJsonFeatureCollection& FeatureCollection, const FGeoJsonBuilderSettings& InSettings);

	UFUNCTION(BlueprintCallable, Category = "GeoJSON")
	void ClearGeometry();

	UFUNCTION(BlueprintCallable, Category = "GeoJSON")
	AActor* GetRootActor() const { return RootActor; }

private:
	UPROPERTY()
	ACesiumGeoreference* Georeference = nullptr;

	UPROPERTY()
	FGeoJsonBuilderSettings Settings;

	UPROPERTY()
	AActor* RootActor = nullptr;

	UPROPERTY()
	AGeoJsonPolygonActor* PolygonActor = nullptr;

	UPROPERTY()
	TArray<AActor*> CubeActors;

	UPROPERTY()
	UStaticMesh* DefaultCubeMesh = nullptr;

	UPROPERTY()
	UMaterialInterface* DefaultPolygonMaterial = nullptr;

	void CreatePolygonMesh(UWorld* World, const FGeoJsonFeatureCollection& FeatureCollection);
	void CreateCubesForGeometry(UWorld* World, const FGeoJsonGeometry& Geometry);
	AActor* CreateCubeAtCoordinate(UWorld* World, const FGeoJsonCoordinate& Coordinate);
	FVector ConvertGeoCoordinateToWorldPosition(const FGeoJsonCoordinate& Coordinate);
	TArray<FVector2D> ProjectPointsTo2D(const TArray<FVector>& Points3D, FVector& OutOrigin, int32& OutBestAxis);
};