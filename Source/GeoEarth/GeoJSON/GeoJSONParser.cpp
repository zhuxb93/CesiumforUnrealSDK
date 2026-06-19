#include "GeoJSONParser.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"

UGeoJSONParser::UGeoJSONParser()
{
}

bool UGeoJSONParser::ParseFromString(const FString& JsonString)
{
	Clear();

	if (JsonString.IsEmpty())
	{
		LastErrorMessage = TEXT("JSON string is empty");
		return false;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		LastErrorMessage = TEXT("Failed to parse JSON string");
		return false;
	}

	return ParseJsonObject(JsonObject);
}

bool UGeoJSONParser::ParseFromFile(const FString& FilePath)
{
	Clear();

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		LastErrorMessage = FString::Printf(TEXT("Failed to load file: %s"), *FilePath);
		return false;
	}

	return ParseFromString(JsonString);
}

void UGeoJSONParser::Clear()
{
	FeatureCollection = FGeoJsonFeatureCollection();
	LastErrorMessage.Empty();
}

bool UGeoJSONParser::ParseJsonObject(const TSharedPtr<FJsonObject>& JsonObject)
{
	if (!JsonObject.IsValid())
	{
		LastErrorMessage = TEXT("Invalid JSON object");
		return false;
	}

	FString TypeString;
	if (!JsonObject->TryGetStringField(TEXT("type"), TypeString))
	{
		LastErrorMessage = TEXT("Missing 'type' field in GeoJSON");
		return false;
	}

	if (TypeString.Equals(TEXT("FeatureCollection"), ESearchCase::IgnoreCase))
	{
		return ParseFeatureCollection(JsonObject);
	}
	else if (TypeString.Equals(TEXT("Feature"), ESearchCase::IgnoreCase))
	{
		FGeoJsonFeature Feature;
		if (ParseFeature(JsonObject, Feature))
		{
			FeatureCollection.Features.Add(Feature);
			return true;
		}
		return false;
	}
	else if (TypeString.Equals(TEXT("GeometryCollection"), ESearchCase::IgnoreCase))
	{
		FGeoJsonFeature Feature;
		Feature.Geometry.Type = EGeoJsonGeometryType::GeometryCollection;
		if (ParseGeometry(JsonObject, Feature.Geometry))
		{
			FeatureCollection.Features.Add(Feature);
			return true;
		}
		return false;
	}
	else
	{
		FGeoJsonFeature Feature;
		if (ParseGeometry(JsonObject, Feature.Geometry))
		{
			FeatureCollection.Features.Add(Feature);
			return true;
		}
		return false;
	}
}

bool UGeoJSONParser::ParseFeatureCollection(const TSharedPtr<FJsonObject>& JsonObject)
{
	const TArray<TSharedPtr<FJsonValue>>* FeaturesArray;
	if (!JsonObject->TryGetArrayField(TEXT("features"), FeaturesArray))
	{
		LastErrorMessage = TEXT("Missing 'features' array in FeatureCollection");
		return false;
	}

	for (const TSharedPtr<FJsonValue>& FeatureValue : *FeaturesArray)
	{
		const TSharedPtr<FJsonObject>* FeatureObject;
		if (FeatureValue->TryGetObject(FeatureObject))
		{
			FGeoJsonFeature Feature;
			if (ParseFeature(*FeatureObject, Feature))
			{
				FeatureCollection.Features.Add(Feature);
			}
		}
	}

	return FeatureCollection.Features.Num() > 0;
}

bool UGeoJSONParser::ParseFeature(const TSharedPtr<FJsonObject>& JsonObject, FGeoJsonFeature& OutFeature)
{
	FString TypeString;
	if (!JsonObject->TryGetStringField(TEXT("type"), TypeString) ||
		!TypeString.Equals(TEXT("Feature"), ESearchCase::IgnoreCase))
	{
		LastErrorMessage = TEXT("Invalid Feature object");
		return false;
	}

	JsonObject->TryGetStringField(TEXT("id"), OutFeature.Id);

	const TSharedPtr<FJsonObject>* PropertiesObject;
	if (JsonObject->TryGetObjectField(TEXT("properties"), PropertiesObject))
	{
		ParseProperties(*PropertiesObject, OutFeature.Properties);
	}

	const TSharedPtr<FJsonObject>* GeometryObject;
	if (JsonObject->TryGetObjectField(TEXT("geometry"), GeometryObject))
	{
		if (!ParseGeometry(*GeometryObject, OutFeature.Geometry))
		{
			return false;
		}
	}

	return OutFeature.Geometry.IsValid();
}

bool UGeoJSONParser::ParseGeometry(const TSharedPtr<FJsonObject>& JsonObject, FGeoJsonGeometry& OutGeometry)
{
	FString TypeString;
	if (!JsonObject->TryGetStringField(TEXT("type"), TypeString))
	{
		LastErrorMessage = TEXT("Missing 'type' field in Geometry");
		return false;
	}

	OutGeometry.Type = ParseGeometryType(TypeString);

	const TArray<TSharedPtr<FJsonValue>>* Coordinates;
	if (!JsonObject->TryGetArrayField(TEXT("coordinates"), Coordinates))
	{
		if (OutGeometry.Type != EGeoJsonGeometryType::GeometryCollection)
		{
			LastErrorMessage = TEXT("Missing 'coordinates' field in Geometry");
			return false;
		}
		return true;
	}

	switch (OutGeometry.Type)
	{
	case EGeoJsonGeometryType::Point:
		return ParsePointCoordinates(*Coordinates, OutGeometry);
	case EGeoJsonGeometryType::MultiPoint:
		return ParseMultiPointCoordinates(*Coordinates, OutGeometry);
	case EGeoJsonGeometryType::LineString:
		return ParseLineStringCoordinates(*Coordinates, OutGeometry);
	case EGeoJsonGeometryType::MultiLineString:
		return ParseMultiLineStringCoordinates(*Coordinates, OutGeometry);
	case EGeoJsonGeometryType::Polygon:
		return ParsePolygonCoordinates(*Coordinates, OutGeometry);
	case EGeoJsonGeometryType::MultiPolygon:
		return ParseMultiPolygonCoordinates(*Coordinates, OutGeometry);
	default:
		return false;
	}
}

bool UGeoJSONParser::ParseProperties(const TSharedPtr<FJsonObject>& JsonObject, FGeoJsonProperties& OutProperties)
{
	for (const auto& Pair : JsonObject->Values)
	{
		const FString& Key = Pair.Key;
		const TSharedPtr<FJsonValue>& Value = Pair.Value;

		switch (Value->Type)
		{
		case EJson::String:
			OutProperties.StringProperties.Add(Key, Value->AsString());
			break;
		case EJson::Number:
			OutProperties.NumberProperties.Add(Key, Value->AsNumber());
			break;
		case EJson::Boolean:
			OutProperties.BoolProperties.Add(Key, Value->AsBool());
			break;
		default:
			break;
		}
	}

	return true;
}

FGeoJsonCoordinate UGeoJSONParser::ParseCoordinate(const TArray<TSharedPtr<FJsonValue>>& CoordArray)
{
	FGeoJsonCoordinate Coord;

	if (CoordArray.Num() >= 2)
	{
		Coord.Longitude = CoordArray[0]->AsNumber();
		Coord.Latitude = CoordArray[1]->AsNumber();

		if (CoordArray.Num() >= 3)
		{
			Coord.Altitude = CoordArray[2]->AsNumber();
			Coord.bHasAltitude = true;
		}
	}

	return Coord;
}

TArray<FGeoJsonCoordinate> UGeoJSONParser::ParseCoordinateArray(const TArray<TSharedPtr<FJsonValue>>& CoordArray)
{
	TArray<FGeoJsonCoordinate> Coordinates;

	for (const TSharedPtr<FJsonValue>& CoordValue : CoordArray)
	{
		const TArray<TSharedPtr<FJsonValue>>* SingleCoord;
		if (CoordValue->TryGetArray(SingleCoord))
		{
			Coordinates.Add(ParseCoordinate(*SingleCoord));
		}
	}

	return Coordinates;
}

TArray<FGeoJsonCoordinateArray> UGeoJSONParser::ParseLinearRings(const TArray<TSharedPtr<FJsonValue>>& RingsArray)
{
	TArray<FGeoJsonCoordinateArray> Rings;

	for (const TSharedPtr<FJsonValue>& RingValue : RingsArray)
	{
		const TArray<TSharedPtr<FJsonValue>>* RingCoords;
		if (RingValue->TryGetArray(RingCoords))
		{
			FGeoJsonCoordinateArray Ring;
			Ring.Coordinates = ParseCoordinateArray(*RingCoords);
			Rings.Add(Ring);
		}
	}

	return Rings;
}

bool UGeoJSONParser::ParsePointCoordinates(const TArray<TSharedPtr<FJsonValue>>& Coordinates, FGeoJsonGeometry& OutGeometry)
{
	OutGeometry.Points.Add(ParseCoordinate(Coordinates));
	return true;
}

bool UGeoJSONParser::ParseMultiPointCoordinates(const TArray<TSharedPtr<FJsonValue>>& Coordinates, FGeoJsonGeometry& OutGeometry)
{
	OutGeometry.Points = ParseCoordinateArray(Coordinates);
	return OutGeometry.Points.Num() > 0;
}

bool UGeoJSONParser::ParseLineStringCoordinates(const TArray<TSharedPtr<FJsonValue>>& Coordinates, FGeoJsonGeometry& OutGeometry)
{
	TArray<FGeoJsonCoordinate> LineCoords = ParseCoordinateArray(Coordinates);
	if (LineCoords.Num() > 0)
	{
		FGeoJsonCoordinateArray LineArray;
		LineArray.Coordinates = LineCoords;
		OutGeometry.LineStrings.Add(LineArray);
		return true;
	}
	return false;
}

bool UGeoJSONParser::ParseMultiLineStringCoordinates(const TArray<TSharedPtr<FJsonValue>>& Coordinates, FGeoJsonGeometry& OutGeometry)
{
	for (const TSharedPtr<FJsonValue>& LineValue : Coordinates)
	{
		const TArray<TSharedPtr<FJsonValue>>* LineCoords;
		if (LineValue->TryGetArray(LineCoords))
		{
			TArray<FGeoJsonCoordinate> Line = ParseCoordinateArray(*LineCoords);
			if (Line.Num() > 0)
			{
				FGeoJsonCoordinateArray LineArray;
				LineArray.Coordinates = Line;
				OutGeometry.LineStrings.Add(LineArray);
			}
		}
	}
	return OutGeometry.LineStrings.Num() > 0;
}

bool UGeoJSONParser::ParsePolygonCoordinates(const TArray<TSharedPtr<FJsonValue>>& Coordinates, FGeoJsonGeometry& OutGeometry)
{
	OutGeometry.Polygons = ParseLinearRings(Coordinates);
	return OutGeometry.Polygons.Num() > 0;
}

bool UGeoJSONParser::ParseMultiPolygonCoordinates(const TArray<TSharedPtr<FJsonValue>>& Coordinates, FGeoJsonGeometry& OutGeometry)
{
	for (const TSharedPtr<FJsonValue>& PolygonValue : Coordinates)
	{
		const TArray<TSharedPtr<FJsonValue>>* PolygonRings;
		if (PolygonValue->TryGetArray(PolygonRings))
		{
			TArray<FGeoJsonCoordinateArray> Rings = ParseLinearRings(*PolygonRings);
			if (Rings.Num() > 0)
			{
				OutGeometry.Polygons.Append(Rings);
			}
		}
	}
	return OutGeometry.Polygons.Num() > 0;
}

EGeoJsonGeometryType UGeoJSONParser::ParseGeometryType(const FString& TypeString)
{
	if (TypeString.Equals(TEXT("Point"), ESearchCase::IgnoreCase))
	{
		return EGeoJsonGeometryType::Point;
	}
	else if (TypeString.Equals(TEXT("MultiPoint"), ESearchCase::IgnoreCase))
	{
		return EGeoJsonGeometryType::MultiPoint;
	}
	else if (TypeString.Equals(TEXT("LineString"), ESearchCase::IgnoreCase))
	{
		return EGeoJsonGeometryType::LineString;
	}
	else if (TypeString.Equals(TEXT("MultiLineString"), ESearchCase::IgnoreCase))
	{
		return EGeoJsonGeometryType::MultiLineString;
	}
	else if (TypeString.Equals(TEXT("Polygon"), ESearchCase::IgnoreCase))
	{
		return EGeoJsonGeometryType::Polygon;
	}
	else if (TypeString.Equals(TEXT("MultiPolygon"), ESearchCase::IgnoreCase))
	{
		return EGeoJsonGeometryType::MultiPolygon;
	}
	else if (TypeString.Equals(TEXT("GeometryCollection"), ESearchCase::IgnoreCase))
	{
		return EGeoJsonGeometryType::GeometryCollection;
	}

	return EGeoJsonGeometryType::Unknown;
}