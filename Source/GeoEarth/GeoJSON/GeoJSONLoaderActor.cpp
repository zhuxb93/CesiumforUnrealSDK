#include "GeoJSONLoaderActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

AGeoJSONLoaderActor::AGeoJSONLoaderActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Parser = CreateDefaultSubobject<UGeoJSONParser>(TEXT("GeoJSONParser"));
	Builder = CreateDefaultSubobject<UGeoJSONGeometryBuilder>(TEXT("GeoJSONGeometryBuilder"));

	SourceType = EGeoJsonSourceType::Folder;
	bAutoBuildOnLoad = true;
	LoadedFeatureCount = 0;
	LoadedPointCount = 0;
}

void AGeoJSONLoaderActor::BeginPlay()
{
	Super::BeginPlay();
	InitializeComponents();
}

void AGeoJSONLoaderActor::PostActorCreated()
{
	Super::PostActorCreated();
	AutoFindGeoreference();
}

#if WITH_EDITOR
void AGeoJSONLoaderActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!PropertyChangedEvent.Property)
	{
		return;
	}

	static const FName NAME_SourceType = GET_MEMBER_NAME_CHECKED(AGeoJSONLoaderActor, SourceType);
	static const FName NAME_JsonString = GET_MEMBER_NAME_CHECKED(AGeoJSONLoaderActor, JsonString);
	static const FName NAME_GeoJsonFilePath = GET_MEMBER_NAME_CHECKED(AGeoJSONLoaderActor, GeoJsonFilePath);
	static const FName NAME_GeoJsonFolderPath = GET_MEMBER_NAME_CHECKED(AGeoJSONLoaderActor, GeoJsonFolderPath);

	FName PropertyName = PropertyChangedEvent.Property->GetFName();

	if (PropertyName == NAME_SourceType || 
		PropertyName == NAME_JsonString ||
		PropertyName == NAME_GeoJsonFilePath ||
		PropertyName == NAME_GeoJsonFolderPath)
	{
		LoadData();
	}
}
#endif

void AGeoJSONLoaderActor::InitializeComponents()
{
	if (!Georeference)
	{
		AutoFindGeoreference();
	}

	if (Builder && Georeference)
	{
		Builder->SetGeoreference(Georeference);
	}
}

void AGeoJSONLoaderActor::AutoFindGeoreference()
{
	if (Georeference)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ACesiumGeoreference> It(World); It; ++It)
	{
		Georeference = *It;
		if (Georeference)
		{
			break;
		}
	}
}

void AGeoJSONLoaderActor::UpdateStatus()
{
	LoadedFeatureCount = CurrentFeatureCollection.Features.Num();

	LoadedPointCount = 0;
	for (const FGeoJsonFeature& Feature : CurrentFeatureCollection.Features)
	{
		LoadedPointCount += Feature.Geometry.GetAllCoordinates().Num();
	}
}

void AGeoJSONLoaderActor::LoadData()
{
	ClearLoadedData();

	bool bSuccess = false;

	switch (SourceType)
	{
	case EGeoJsonSourceType::JsonString:
		if (!JsonString.IsEmpty())
		{
			bSuccess = LoadFromJsonString(JsonString);
		}
		break;

	case EGeoJsonSourceType::File:
		if (!GeoJsonFilePath.FilePath.IsEmpty())
		{
			bSuccess = LoadFromFile(GeoJsonFilePath.FilePath);
		}
		break;

	case EGeoJsonSourceType::Folder:
		if (!GeoJsonFolderPath.Path.IsEmpty())
		{
			int32 LoadedCount = LoadFromFolder(GeoJsonFolderPath.Path, bAutoBuildOnLoad);
			bSuccess = LoadedCount > 0;
		}
		break;
	}

	if (bSuccess && bAutoBuildOnLoad && SourceType != EGeoJsonSourceType::Folder)
	{
		RebuildGeometry();
	}

	UpdateStatus();
}

void AGeoJSONLoaderActor::ClearAll()
{
	ClearLoadedData();
	UpdateStatus();
}

bool AGeoJSONLoaderActor::LoadFromJsonString(const FString& InJsonString)
{
	if (!Parser)
	{
		OnGeoJsonLoadFailed.Broadcast(TEXT("Parser not initialized"), TEXT("LoadFromJsonString"));
		return false;
	}

	if (!Parser->ParseFromString(InJsonString))
	{
		OnGeoJsonLoadFailed.Broadcast(Parser->GetLastErrorMessage(), TEXT("LoadFromJsonString"));
		return false;
	}

	CurrentFeatureCollection = Parser->GetFeatureCollection();
	OnGeoJsonLoaded.Broadcast(CurrentFeatureCollection);

	return true;
}

bool AGeoJSONLoaderActor::LoadFromFile(const FString& FilePath)
{
	if (!Parser)
	{
		OnGeoJsonLoadFailed.Broadcast(TEXT("Parser not initialized"), FilePath);
		return false;
	}

	if (!Parser->ParseFromFile(FilePath))
	{
		OnGeoJsonLoadFailed.Broadcast(Parser->GetLastErrorMessage(), FilePath);
		return false;
	}

	CurrentFeatureCollection = Parser->GetFeatureCollection();
	OnGeoJsonLoaded.Broadcast(CurrentFeatureCollection);

	return true;
}

void AGeoJSONLoaderActor::LoadFromUrl(const FString& Url)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("GET"));

	Request->OnProcessRequestComplete().BindLambda([this, Url](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSuccess)
	{
		if (bSuccess && HttpResponse.IsValid())
		{
			int32 ResponseCode = HttpResponse->GetResponseCode();
			if (ResponseCode >= 200 && ResponseCode < 300)
			{
				FString ResponseJsonString = HttpResponse->GetContentAsString();
				if (LoadFromJsonString(ResponseJsonString))
				{
					if (bAutoBuildOnLoad)
					{
						RebuildGeometry();
					}
					UpdateStatus();
				}
			}
			else
			{
				FString ErrorMsg = FString::Printf(TEXT("HTTP Error: %d"), ResponseCode);
				OnGeoJsonLoadFailed.Broadcast(ErrorMsg, Url);
			}
		}
		else
		{
			OnGeoJsonLoadFailed.Broadcast(TEXT("Failed to fetch GeoJSON from URL"), Url);
		}
	});

	Request->ProcessRequest();
}

int32 AGeoJSONLoaderActor::LoadFromFolder(const FString& InFolderPath, bool bAutoBuild)
{
	if (InFolderPath.IsEmpty())
	{
		return 0;
	}

	TArray<FString> FoundFiles;
	IFileManager::Get().FindFilesRecursive(FoundFiles, *InFolderPath, TEXT("*.geojson"), true, false);
	IFileManager::Get().FindFilesRecursive(FoundFiles, *InFolderPath, TEXT("*.json"), true, false);

	int32 SuccessCount = 0;
	for (const FString& File : FoundFiles)
	{
		if (!Parser)
		{
			break;
		}

		if (Parser->ParseFromFile(File))
		{
			const FGeoJsonFeatureCollection& NewCollection = Parser->GetFeatureCollection();
			CurrentFeatureCollection.Features.Append(NewCollection.Features);
			SuccessCount++;
		}
	}

	if (SuccessCount > 0)
	{
		OnGeoJsonLoaded.Broadcast(CurrentFeatureCollection);
		
		if (bAutoBuild)
		{
			RebuildGeometry();
		}
	}

	return SuccessCount;
}

void AGeoJSONLoaderActor::ClearLoadedData()
{
	CurrentFeatureCollection = FGeoJsonFeatureCollection();

	if (Builder)
	{
		Builder->ClearGeometry();
	}
}

const FGeoJsonFeatureCollection& AGeoJSONLoaderActor::GetFeatureCollection() const
{
	return CurrentFeatureCollection;
}

AActor* AGeoJSONLoaderActor::GetRootActor() const
{
	if (Builder)
	{
		return Builder->GetRootActor();
	}
	return nullptr;
}

void AGeoJSONLoaderActor::SetBuilderSettings(const FGeoJsonBuilderSettings& InSettings)
{
	BuilderSettings = InSettings;
	if (Builder)
	{
		Builder->SetSettings(InSettings);
	}
}

void AGeoJSONLoaderActor::SetGeoreference(ACesiumGeoreference* InGeoreference)
{
	Georeference = InGeoreference;
	if (Builder)
	{
		Builder->SetGeoreference(Georeference);
	}
}

void AGeoJSONLoaderActor::RebuildGeometry()
{
	if (!Builder || !CurrentFeatureCollection.IsValid())
	{
		return;
	}

	Builder->ClearGeometry();

	if (!Georeference)
	{
		AutoFindGeoreference();
	}

	if (Builder && Georeference)
	{
		Builder->SetGeoreference(Georeference);
	}

	Builder->BuildFromFeatureCollection(GetWorld(), CurrentFeatureCollection, BuilderSettings);
}