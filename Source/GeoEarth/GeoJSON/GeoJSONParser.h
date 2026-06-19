#pragma once

#include "CoreMinimal.h"
#include "GeoJSONTypes.h"
#include "GeoJSONParser.generated.h"

UCLASS(BlueprintType)
class GEOEARTH_API UGeoJSONParser : public UObject
{
	GENERATED_BODY()

public:
	UGeoJSONParser();

	UFUNCTION(BlueprintCallable, Category = "GeoJSON")
	bool ParseFromString(const FString& JsonString);

	UFUNCTION(BlueprintCallable, Category = "GeoJSON")
	bool ParseFromFile(const FString& FilePath);

	UFUNCTION(BlueprintCallable, Category = "GeoJSON")
	const FGeoJsonFeatureCollection& GetFeatureCollection() const { return FeatureCollection; }

	UFUNCTION(BlueprintCallable, Category = "GeoJSON")
	FString GetLastErrorMessage() const { return LastErrorMessage; }

	UFUNCTION(BlueprintCallable, Category = "GeoJSON")
	void Clear();

protected:
	FGeoJsonFeatureCollection FeatureCollection;
	FString LastErrorMessage;

	bool ParseJsonObject(const TSharedPtr<class FJsonObject>& JsonObject);
	bool ParseFeatureCollection(const TSharedPtr<class FJsonObject>& JsonObject);
	bool ParseFeature(const TSharedPtr<class FJsonObject>& JsonObject, FGeoJsonFeature& OutFeature);
	bool ParseGeometry(const TSharedPtr<class FJsonObject>& JsonObject, FGeoJsonGeometry& OutGeometry);
	bool ParseProperties(const TSharedPtr<class FJsonObject>& JsonObject, FGeoJsonProperties& OutProperties);

	bool ParsePointCoordinates(const TArray<TSharedPtr<class FJsonValue>>& Coordinates, FGeoJsonGeometry& OutGeometry);
	bool ParseMultiPointCoordinates(const TArray<TSharedPtr<class FJsonValue>>& Coordinates, FGeoJsonGeometry& OutGeometry);
	bool ParseLineStringCoordinates(const TArray<TSharedPtr<class FJsonValue>>& Coordinates, FGeoJsonGeometry& OutGeometry);
	bool ParseMultiLineStringCoordinates(const TArray<TSharedPtr<class FJsonValue>>& Coordinates, FGeoJsonGeometry& OutGeometry);
	bool ParsePolygonCoordinates(const TArray<TSharedPtr<class FJsonValue>>& Coordinates, FGeoJsonGeometry& OutGeometry);
	bool ParseMultiPolygonCoordinates(const TArray<TSharedPtr<class FJsonValue>>& Coordinates, FGeoJsonGeometry& OutGeometry);

	FGeoJsonCoordinate ParseCoordinate(const TArray<TSharedPtr<class FJsonValue>>& CoordArray);
	TArray<FGeoJsonCoordinate> ParseCoordinateArray(const TArray<TSharedPtr<class FJsonValue>>& CoordArray);
	TArray<FGeoJsonCoordinateArray> ParseLinearRings(const TArray<TSharedPtr<class FJsonValue>>& RingsArray);

	EGeoJsonGeometryType ParseGeometryType(const FString& TypeString);
};