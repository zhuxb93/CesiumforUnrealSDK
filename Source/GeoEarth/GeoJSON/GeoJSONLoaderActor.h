#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CesiumRuntime/Public/CesiumGeoreference.h"
#include "GeoJSONTypes.h"
#include "GeoJSONParser.h"
#include "GeoJSONGeometryBuilder.h"
#include "GeoJSONLoaderActor.generated.h"

UENUM(BlueprintType)
enum class EGeoJsonSourceType : uint8
{
	JsonString,
	File,
	Folder
};

UCLASS(BlueprintType, Blueprintable)
class GEOEARTH_API AGeoJSONLoaderActor : public AActor
{
	GENERATED_BODY()

public:
	AGeoJSONLoaderActor();

protected:
	virtual void BeginPlay() override;
	virtual void PostActorCreated() override;

public:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GeoJSON|Cesium")
	ACesiumGeoreference* Georeference;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GeoJSON|Settings")
	FGeoJsonBuilderSettings BuilderSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GeoJSON|Data Source")
	EGeoJsonSourceType SourceType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GeoJSON|Data Source", meta = (MultiLine = true, EditCondition = "SourceType == EGeoJsonSourceType::JsonString"))
	FString JsonString;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GeoJSON|Data Source", meta = (EditCondition = "SourceType == EGeoJsonSourceType::File"))
	FFilePath GeoJsonFilePath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GeoJSON|Data Source", meta = (EditCondition = "SourceType == EGeoJsonSourceType::Folder"))
	FDirectoryPath GeoJsonFolderPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GeoJSON|Data Source")
	bool bAutoBuildOnLoad;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GeoJSON|Status")
	int32 LoadedFeatureCount;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GeoJSON|Status")
	int32 LoadedPointCount;

	UPROPERTY(BlueprintAssignable, Category = "GeoJSON|Events")
	FOnGeoJsonLoaded OnGeoJsonLoaded;

	UPROPERTY(BlueprintAssignable, Category = "GeoJSON|Events")
	FOnGeoJsonLoadFailed OnGeoJsonLoadFailed;

	UFUNCTION(BlueprintCallable, Category = "GeoJSON")
	bool LoadFromJsonString(const FString& InJsonString);

	UFUNCTION(BlueprintCallable, Category = "GeoJSON")
	bool LoadFromFile(const FString& FilePath);

	UFUNCTION(BlueprintCallable, Category = "GeoJSON")
	void LoadFromUrl(const FString& Url);

	UFUNCTION(BlueprintCallable, Category = "GeoJSON")
	int32 LoadFromFolder(const FString& InFolderPath, bool bAutoBuild = true);

	UFUNCTION(BlueprintCallable, Category = "GeoJSON")
	void ClearLoadedData();

	UFUNCTION(BlueprintCallable, Category = "GeoJSON")
	const FGeoJsonFeatureCollection& GetFeatureCollection() const;

	UFUNCTION(BlueprintCallable, Category = "GeoJSON")
	AActor* GetRootActor() const;

	UFUNCTION(BlueprintCallable, Category = "GeoJSON")
	void SetBuilderSettings(const FGeoJsonBuilderSettings& InSettings);

	UFUNCTION(BlueprintCallable, Category = "GeoJSON")
	void SetGeoreference(ACesiumGeoreference* InGeoreference);

	UFUNCTION(BlueprintCallable, Category = "GeoJSON")
	void RebuildGeometry();

	UFUNCTION(CallInEditor, Category = "GeoJSON")
	void LoadData();

	UFUNCTION(CallInEditor, Category = "GeoJSON")
	void ClearAll();

protected:
	UPROPERTY()
	UGeoJSONParser* Parser;

	UPROPERTY()
	UGeoJSONGeometryBuilder* Builder;

	UPROPERTY()
	FGeoJsonFeatureCollection CurrentFeatureCollection;

	void InitializeComponents();
	void AutoFindGeoreference();
	void UpdateStatus();
};