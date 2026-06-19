#pragma once

#include "CoreMinimal.h"
#include "GeoJSONTypes.generated.h"

UENUM(BlueprintType)
enum class EGeoJsonGeometryType : uint8
{
	Point,
	MultiPoint,
	LineString,
	MultiLineString,
	Polygon,
	MultiPolygon,
	GeometryCollection,
	Unknown
};

USTRUCT(BlueprintType)
struct FGeoJsonCoordinate
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	double Longitude = 0.0;

	UPROPERTY(BlueprintReadOnly)
	double Latitude = 0.0;

	UPROPERTY(BlueprintReadOnly)
	double Altitude = 0.0;

	UPROPERTY(BlueprintReadOnly)
	bool bHasAltitude = false;

	FGeoJsonCoordinate() {}

	FGeoJsonCoordinate(double InLon, double InLat, double InAlt = 0.0, bool bInHasAltitude = false)
		: Longitude(InLon)
		, Latitude(InLat)
		, Altitude(InAlt)
		, bHasAltitude(bInHasAltitude)
	{}
};

USTRUCT(BlueprintType)
struct FGeoJsonCoordinateArray
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TArray<FGeoJsonCoordinate> Coordinates;
};

USTRUCT(BlueprintType)
struct FGeoJsonGeometry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	EGeoJsonGeometryType Type = EGeoJsonGeometryType::Unknown;

	UPROPERTY(BlueprintReadOnly)
	TArray<FGeoJsonCoordinate> Points;

	UPROPERTY(BlueprintReadOnly)
	TArray<FGeoJsonCoordinateArray> LineStrings;

	UPROPERTY(BlueprintReadOnly)
	TArray<FGeoJsonCoordinateArray> Polygons;

	bool IsValid() const { return Type != EGeoJsonGeometryType::Unknown; }

	TArray<FGeoJsonCoordinate> GetAllCoordinates() const
	{
		TArray<FGeoJsonCoordinate> AllCoords;
		AllCoords.Append(Points);
		
		for (const auto& Line : LineStrings)
		{
			AllCoords.Append(Line.Coordinates);
		}
		
		for (const auto& Polygon : Polygons)
		{
			AllCoords.Append(Polygon.Coordinates);
		}
		
		return AllCoords;
	}
};

USTRUCT(BlueprintType)
struct FGeoJsonProperties
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TMap<FString, FString> StringProperties;

	UPROPERTY(BlueprintReadOnly)
	TMap<FString, double> NumberProperties;

	UPROPERTY(BlueprintReadOnly)
	TMap<FString, bool> BoolProperties;

	bool TryGetString(const FString& Key, FString& OutValue) const
	{
		const FString* Found = StringProperties.Find(Key);
		if (Found)
		{
			OutValue = *Found;
			return true;
		}
		return false;
	}

	bool TryGetNumber(const FString& Key, double& OutValue) const
	{
		const double* Found = NumberProperties.Find(Key);
		if (Found)
		{
			OutValue = *Found;
			return true;
		}
		return false;
	}

	bool TryGetBool(const FString& Key, bool& OutValue) const
	{
		const bool* Found = BoolProperties.Find(Key);
		if (Found)
		{
			OutValue = *Found;
			return true;
		}
		return false;
	}
};

USTRUCT(BlueprintType)
struct FGeoJsonFeature
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Id;

	UPROPERTY(BlueprintReadOnly)
	FGeoJsonGeometry Geometry;

	UPROPERTY(BlueprintReadOnly)
	FGeoJsonProperties Properties;

	bool IsValid() const { return Geometry.IsValid(); }
};

USTRUCT(BlueprintType)
struct FGeoJsonFeatureCollection
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TArray<FGeoJsonFeature> Features;

	bool IsValid() const { return Features.Num() > 0; }

	TArray<FGeoJsonCoordinate> GetAllCoordinates() const
	{
		TArray<FGeoJsonCoordinate> AllCoords;
		for (const auto& Feature : Features)
		{
			AllCoords.Append(Feature.Geometry.GetAllCoordinates());
		}
		return AllCoords;
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGeoJsonLoaded, const FGeoJsonFeatureCollection&, FeatureCollection);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGeoJsonLoadFailed, const FString&, ErrorMessage, const FString&, Context);