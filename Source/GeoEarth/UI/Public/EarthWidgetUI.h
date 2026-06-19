#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/SlateWrapperTypes.h"
#include "EarthWidgetUI.generated.h"

class AGeoCameraController;
class UEditableTextBox;
class UButton;
class UBorder;
class UVerticalBox;
class UTextBlock;
class USizeBox;
class ACesium3DTileset;

enum class EBuildingMode : uint8
{
	WhiteModel,
	Dtiles,
	Imagery
};

USTRUCT()
struct FCityData
{
	GENERATED_BODY()

	UPROPERTY() FString Name;
	UPROPERTY() FString Adcode;
	UPROPERTY() FString Url;
	UPROPERTY() bool bHasLatLon = false;
	UPROPERTY() double Lon = 0.0;
	UPROPERTY() double Lat = 0.0;
};

UCLASS()
class GEOEARTH_API UEarthWidgetUI : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	UEditableTextBox* SearchInput = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UVerticalBox* CityList = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* HeaderStatus = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UBorder* StatusDot = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UBorder* TileSAT = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UBorder* TileSTREET = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* TileSATIcon = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* TileSATLbl = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* TileSTREETIcon = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* TileSTREETLbl = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UBorder* TileIMG = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* TileIMGIcon = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* TileIMGLbl = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	USizeBox* TileSATLineBox = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	USizeBox* TileSTREETLineBox = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	USizeBox* TileIMGLineBox = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UBorder* ToggleTab = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* TabIcon = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UBorder* PanelOuter = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Camera")
	AGeoCameraController* CameraController = nullptr;

	UPROPERTY()
	TArray<FCityData> CityDataArray;

	UPROPERTY()
	TMap<UButton*, int32> ButtonToIndexMap;

	UPROPERTY()
	ACesium3DTileset* CurrentTileset = nullptr;

	int32 ActiveCityIndex = -1;
	FString SearchQuery;
	EBuildingMode BuildingMode = EBuildingMode::Imagery;
	bool bDataLoaded = false;
	bool bPanelCollapsed = true;  // default collapsed
	FVector2D ExpandedTabPos = FVector2D::ZeroVector;

	// Collapse animation (0 = fully expanded, 1 = fully collapsed)
	float CollapseAnimAlpha = 1.f;
	float CollapseAnimTarget = 1.f;
	static constexpr float kCollapseAnimDuration = 0.35f;

	FTimerHandle StatusResetTimer;

	// API
	void FetchCityData();
	void OnCityDataReceived(const TArray<uint8>& ResponseData);

	// UI build
	void RebuildCityList();
	void ClearCityList();

	// Handlers
	UFUNCTION()
	void OnCityButtonClicked();

	UFUNCTION()
	FEventReply OnWhiteModeClicked(FGeometry MyGeometry, const FPointerEvent& InMouseEvent);

	UFUNCTION()
	FEventReply OnDtilesClicked(FGeometry MyGeometry, const FPointerEvent& InMouseEvent);

	UFUNCTION()
	FEventReply OnImageryClicked(FGeometry MyGeometry, const FPointerEvent& InMouseEvent);

	UFUNCTION()
	FEventReply OnToggleTabClicked(FGeometry MyGeometry, const FPointerEvent& InMouseEvent);

	UFUNCTION()
	void OnSearchTextChanged(const FText& Text);

	// Tileset
	void LoadCityTileset(int32 CityIndex);
	void FlyToLonLat(double Lon, double Lat, double Height);

	// Visual state
	void SetFlyingStatus(bool bFlying);
	void UpdateBuildingModeVisual();
	void ApplyBuildingMode();
	void ApplyCollapseAnim();
};
