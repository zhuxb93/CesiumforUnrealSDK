#include "EarthWidgetUI.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/SlateTypes.h"
#include "Async/Async.h"
#include "TimerManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "CesiumRuntime/Public/Cesium3DTileset.h"
#include "CesiumRuntime/Public/CesiumGeoreference.h"
#include "CesiumRuntime/Public/GeoTransforms.h"
#include "GeoCameraController.h"
#include "VectorTile/Public/GeoVectorTileActor.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	constexpr float kActiveBgR = 0.f;
	constexpr float kActiveBgG = 0.01033f;
	constexpr float kActiveBgB = 0.04227f;

	constexpr float kInactiveBgR = 0.000303f;
	constexpr float kInactiveBgG = 0.003035f;
	constexpr float kInactiveBgB = 0.00913f;

	const FLinearColor kCyan(0.f, 0.6584f, 1.f, 1.f);
	const FLinearColor kInactiveDot(0.00401f, 0.0685f, 0.1441f, 1.f);
	const FLinearColor kInactiveName(0.0685f, 0.2546f, 0.4026f, 1.f);
	const FLinearColor kActiveCode(0.01034f, 0.1441f, 0.2546f, 1.f);
	const FLinearColor kInactiveCode(0.00401f, 0.04228f, 0.0803f, 1.f);

	const FLinearColor kTileActiveBg(0.f, 0.007f, 0.02126f, 1.f);
	const FLinearColor kTileInactiveBg(0.000303f, 0.001822f, 0.002732f, 1.f);
	const FLinearColor kTileActiveFg = kCyan;
	const FLinearColor kTileInactiveFg(0.01034f, 0.1022f, 0.1946f, 1.f);

	const FLinearColor kStatusOnlineGreen(0.f, 0.7913f, 0.1812f, 1.f);
	const FLinearColor kStatusFlyingAmber(0.7913f, 0.3f, 0.03f, 1.f);
}

bool UEarthWidgetUI::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (APlayerController* PC = GetOwningPlayer())
	{
		CameraController = Cast<AGeoCameraController>(PC->GetPawn());
	}

	if (SearchInput)
	{
		SearchInput->OnTextChanged.RemoveAll(this);
		SearchInput->OnTextChanged.AddDynamic(this, &UEarthWidgetUI::OnSearchTextChanged);
	}

	if (TileSAT)
	{
		TileSAT->OnMouseButtonDownEvent.Unbind();
		TileSAT->OnMouseButtonDownEvent.BindUFunction(this, FName("OnWhiteModeClicked"));
	}

	if (TileSTREET)
	{
		TileSTREET->OnMouseButtonDownEvent.Unbind();
		TileSTREET->OnMouseButtonDownEvent.BindUFunction(this, FName("OnDtilesClicked"));
	}

	if (TileIMG)
	{
		TileIMG->OnMouseButtonDownEvent.Unbind();
		TileIMG->OnMouseButtonDownEvent.BindUFunction(this, FName("OnImageryClicked"));
	}

	if (ToggleTab)
	{
		ToggleTab->OnMouseButtonDownEvent.Unbind();
		ToggleTab->OnMouseButtonDownEvent.BindUFunction(this, FName("OnToggleTabClicked"));

		// Cache the designer-authored tab position (expanded state)
		if (UCanvasPanelSlot* CPS = Cast<UCanvasPanelSlot>(ToggleTab->Slot))
		{
			ExpandedTabPos = CPS->GetPosition();
		}
	}

	ClearCityList();
	UpdateBuildingModeVisual();
	ApplyBuildingMode();
	SetFlyingStatus(false);

	// Default: panel collapsed, tab at x=0
	bPanelCollapsed = true;
	CollapseAnimAlpha = 1.f;
	CollapseAnimTarget = 1.f;
	if (TabIcon)
	{
		TabIcon->SetText(FText::FromString(TEXT("\u25B6")));
	}
	ApplyCollapseAnim();

	FetchCityData();

	return true;
}

void UEarthWidgetUI::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StatusResetTimer);
	}
	Super::NativeDestruct();
}

void UEarthWidgetUI::FetchCityData()
{
	TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(TEXT("https://example.com/service-endpoint"));
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	TWeakObjectPtr<UEarthWidgetUI> WeakThis(this);
	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis](FHttpRequestPtr, FHttpResponsePtr Response, bool bSuccess)
		{
			if (!WeakThis.IsValid() || !bSuccess || !Response.IsValid())
			{
				return;
			}
			WeakThis->OnCityDataReceived(Response->GetContent());
		});
	Request->ProcessRequest();
}

void UEarthWidgetUI::OnCityDataReceived(const TArray<uint8>& ResponseData)
{
	if (ResponseData.Num() == 0)
	{
		return;
	}

	TArray<uint8> Buffer = ResponseData;
	Buffer.Add(0);
	FString JsonString = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Buffer.GetData())));

	TSharedPtr<FJsonValue> JsonValue;
	TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, JsonValue) || !JsonValue.IsValid())
	{
		return;
	}
	if (JsonValue->Type != EJson::Array)
	{
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>& Arr = JsonValue->AsArray();
	CityDataArray.Reset();
	CityDataArray.Reserve(Arr.Num());
	for (const TSharedPtr<FJsonValue>& V : Arr)
	{
		if (!V.IsValid()) continue;
		TSharedPtr<FJsonObject> Obj = V->AsObject();
		if (!Obj.IsValid()) continue;

		FCityData Data;
		Obj->TryGetStringField(TEXT("name"), Data.Name);
		Obj->TryGetStringField(TEXT("adcode"), Data.Adcode);
		Obj->TryGetStringField(TEXT("url"), Data.Url);

		// Try common coordinate field names the backend might return
		double Lon = 0.0, Lat = 0.0;
		bool bGotLon = Obj->TryGetNumberField(TEXT("lon"), Lon)
			|| Obj->TryGetNumberField(TEXT("lng"), Lon)
			|| Obj->TryGetNumberField(TEXT("longitude"), Lon);
		bool bGotLat = Obj->TryGetNumberField(TEXT("lat"), Lat)
			|| Obj->TryGetNumberField(TEXT("latitude"), Lat);
		if (bGotLon && bGotLat)
		{
			Data.Lon = Lon;
			Data.Lat = Lat;
			Data.bHasLatLon = true;
		}

		CityDataArray.Add(MoveTemp(Data));
	}

	bDataLoaded = true;
	RebuildCityList();
}

void UEarthWidgetUI::ClearCityList()
{
	ButtonToIndexMap.Empty();
	if (CityList && IsValid(CityList))
	{
		CityList->ClearChildren();
	}
}

void UEarthWidgetUI::RebuildCityList()
{
	if (!CityList || !WidgetTree)
	{
		return;
	}

	ClearCityList();

	const FString Query = SearchQuery.TrimStartAndEnd();

	for (int32 i = 0; i < CityDataArray.Num(); ++i)
	{
		const FCityData& City = CityDataArray[i];
		if (!Query.IsEmpty() && !City.Name.Contains(Query, ESearchCase::IgnoreCase))
		{
			continue;
		}

		const bool bActive = (i == ActiveCityIndex);

		// Outer transparent Button (click target) — hover gets a subtle cyan tint overlay
		UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		if (!Btn) continue;
		{
			FButtonStyle Style = Btn->GetStyle();

			FSlateBrush TransparentBrush;
			TransparentBrush.TintColor = FSlateColor(FLinearColor(0, 0, 0, 0));
			TransparentBrush.DrawAs = ESlateBrushDrawType::NoDrawType;

			FSlateBrush HoverBrush;
			HoverBrush.TintColor = FSlateColor(FLinearColor(0.f, 0.6584f, 1.f, 0.10f));
			HoverBrush.DrawAs = ESlateBrushDrawType::Box;

			Style.SetNormal(TransparentBrush);
			Style.SetHovered(HoverBrush);
			Style.SetPressed(HoverBrush);
			Style.SetDisabled(TransparentBrush);
			Style.NormalPadding = FMargin(0);
			Style.PressedPadding = FMargin(0);
			Btn->SetStyle(Style);
		}
		if (UVerticalBoxSlot* VSlot = CityList->AddChildToVerticalBox(Btn))
		{
			VSlot->SetHorizontalAlignment(HAlign_Fill);
			VSlot->SetVerticalAlignment(VAlign_Fill);
			VSlot->SetPadding(FMargin(0));
		}

		// Row HBox
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UButtonSlot* ButtonContentSlot = Cast<UButtonSlot>(Btn->AddChild(Row)))
		{
			ButtonContentSlot->SetHorizontalAlignment(HAlign_Fill);
			ButtonContentSlot->SetVerticalAlignment(VAlign_Fill);
			ButtonContentSlot->SetPadding(FMargin(0));
		}

		// Accent bar (2 wide; cyan if active, transparent otherwise)
		USizeBox* AccentBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		AccentBox->SetWidthOverride(2.f);
		UBorder* AccentBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		AccentBar->SetPadding(FMargin(0));
		AccentBar->SetBrushColor(bActive ? kCyan : FLinearColor(0, 0, 0, 0));
		AccentBox->AddChild(AccentBar);
		Row->AddChildToHorizontalBox(AccentBox);

		// BG Border (Fill horizontally, so the whole row bg stretches panel width)
		UBorder* Bg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Bg->SetPadding(FMargin(12, 10, 14, 10));
		Bg->SetHorizontalAlignment(HAlign_Fill);
		Bg->SetVerticalAlignment(VAlign_Fill);
		Bg->SetBrushColor(bActive
			? FLinearColor(kActiveBgR, kActiveBgG, kActiveBgB, 1.f)
			: FLinearColor(kInactiveBgR, kInactiveBgG, kInactiveBgB, 1.f));
		if (UHorizontalBoxSlot* BgSlot = Row->AddChildToHorizontalBox(Bg))
		{
			BgSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			BgSlot->SetHorizontalAlignment(HAlign_Fill);
			BgSlot->SetVerticalAlignment(VAlign_Fill);
		}

		// Inner content HBox
		UHorizontalBox* Content = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Bg->AddChild(Content);

		// Dot (fixed 4px container width so name column doesn't shift between states)
		USizeBox* DotBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		DotBox->SetWidthOverride(4.f);
		DotBox->SetHeightOverride(bActive ? 4.f : 3.f);
		UBorder* Dot = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Dot->SetPadding(FMargin(0));
		Dot->SetBrushColor(bActive ? kCyan : kInactiveDot);
		DotBox->AddChild(Dot);
		if (UHorizontalBoxSlot* DotSlot = Content->AddChildToHorizontalBox(DotBox))
		{
			DotSlot->SetPadding(FMargin(0, 0, 10, 0));
			DotSlot->SetVerticalAlignment(VAlign_Center);
		}

		// Name (auto-sized; takes as much space as it needs, long names show in full)
		UTextBlock* NameTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		NameTxt->SetText(FText::FromString(City.Name));
		NameTxt->SetColorAndOpacity(FSlateColor(bActive ? kCyan : kInactiveName));
		{
			FSlateFontInfo Font = NameTxt->GetFont();
			Font.Size = 13;
			Font.TypefaceFontName = bActive ? FName("Bold") : FName("Regular");
			Font.LetterSpacing = 50;
			NameTxt->SetFont(Font);
		}
		if (UHorizontalBoxSlot* NameSlot = Content->AddChildToHorizontalBox(NameTxt))
		{
			NameSlot->SetPadding(FMargin(0, 0, 10, 0));
			NameSlot->SetVerticalAlignment(VAlign_Center);
		}

		// Spacer (Fill — takes remaining width, pushing Arrow to the far right)
		USizeBox* Spacer = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		if (UHorizontalBoxSlot* SpacerSlot = Content->AddChildToHorizontalBox(Spacer))
		{
			SpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		// Arrow slot (always present for layout stability; text only visible when active)
		USizeBox* ArrowBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		ArrowBox->SetWidthOverride(16.f);
		if (bActive)
		{
			UTextBlock* Arrow = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			Arrow->SetText(FText::FromString(TEXT("\u203A")));
			Arrow->SetColorAndOpacity(FSlateColor(kCyan));
			FSlateFontInfo Font = Arrow->GetFont();
			Font.Size = 14;
			Font.TypefaceFontName = FName("Bold");
			Arrow->SetFont(Font);
			ArrowBox->AddChild(Arrow);
		}
		if (UHorizontalBoxSlot* ArrowSlot = Content->AddChildToHorizontalBox(ArrowBox))
		{
			ArrowSlot->SetVerticalAlignment(VAlign_Center);
		}

		ButtonToIndexMap.Add(Btn, i);
		Btn->OnClicked.AddDynamic(this, &UEarthWidgetUI::OnCityButtonClicked);
	}
}

void UEarthWidgetUI::OnCityButtonClicked()
{
	for (const TPair<UButton*, int32>& Pair : ButtonToIndexMap)
	{
		if (Pair.Key && Pair.Key->IsHovered())
		{
			const int32 NewIndex = Pair.Value;
			if (NewIndex != ActiveCityIndex)
			{
				ActiveCityIndex = NewIndex;
				RebuildCityList();
				LoadCityTileset(NewIndex);
			}
			return;
		}
	}
}

void UEarthWidgetUI::OnSearchTextChanged(const FText& Text)
{
	SearchQuery = Text.ToString();
	RebuildCityList();
}

FEventReply UEarthWidgetUI::OnWhiteModeClicked(FGeometry MyGeometry, const FPointerEvent& InMouseEvent)
{
	if (BuildingMode != EBuildingMode::WhiteModel)
	{
		BuildingMode = EBuildingMode::WhiteModel;
		UpdateBuildingModeVisual();
		ApplyBuildingMode();
	}
	return FEventReply(true);
}

FEventReply UEarthWidgetUI::OnDtilesClicked(FGeometry MyGeometry, const FPointerEvent& InMouseEvent)
{
	if (BuildingMode != EBuildingMode::Dtiles)
	{
		BuildingMode = EBuildingMode::Dtiles;
		UpdateBuildingModeVisual();
		ApplyBuildingMode();
	}
	return FEventReply(true);
}

FEventReply UEarthWidgetUI::OnImageryClicked(FGeometry MyGeometry, const FPointerEvent& InMouseEvent)
{
	if (BuildingMode != EBuildingMode::Imagery)
	{
		BuildingMode = EBuildingMode::Imagery;
		UpdateBuildingModeVisual();
		ApplyBuildingMode();
	}
	return FEventReply(true);
}

void UEarthWidgetUI::ApplyBuildingMode()
{
	UWorld* World = GetWorld();
	if (!World) return;

	const bool bShowWhiteModel = (BuildingMode == EBuildingMode::WhiteModel);
	const bool bShowDtiles     = (BuildingMode == EBuildingMode::Dtiles);

	// Procedural vector-tile buildings (白模) — active only in WhiteModel mode
	TArray<AActor*> VectorActors;
	UGameplayStatics::GetAllActorsOfClass(World, AGeoVectorTileActor::StaticClass(), VectorActors);
	for (AActor* A : VectorActors)
	{
		if (AGeoVectorTileActor* V = Cast<AGeoVectorTileActor>(A))
		{
			V->SetEnabled(bShowWhiteModel);
		}
	}

	// Cesium 3D Tileset (数字表亲) — active only in Dtiles mode
	if (CurrentTileset)
	{
		CurrentTileset->SetActorHiddenInGame(!bShowDtiles);
	}
	// Imagery mode: both hidden (nothing extra to do — imagery is the base layer already rendered)
}

FEventReply UEarthWidgetUI::OnToggleTabClicked(FGeometry MyGeometry, const FPointerEvent& InMouseEvent)
{
	bPanelCollapsed = !bPanelCollapsed;
	CollapseAnimTarget = bPanelCollapsed ? 1.f : 0.f;

	// Swap tab icon instantly
	if (TabIcon)
	{
		TabIcon->SetText(FText::FromString(bPanelCollapsed ? TEXT("\u25B6") : TEXT("\u25C0")));
	}

	// Pre-show panel when expanding so fade-in is visible
	if (!bPanelCollapsed && PanelOuter)
	{
		PanelOuter->SetVisibility(ESlateVisibility::Visible);
	}
	return FEventReply(true);
}

void UEarthWidgetUI::ApplyCollapseAnim()
{
	// Ease-in-out cubic: symmetric smooth curve feeling like modern native apps
	auto EaseInOutCubic = [](float t)
	{
		t = FMath::Clamp(t, 0.f, 1.f);
		return t < 0.5f
			? 4.f * t * t * t
			: 1.f - FMath::Pow(-2.f * t + 2.f, 3.f) * 0.5f;
	};

	const float t = EaseInOutCubic(CollapseAnimAlpha);
	const bool bFullyCollapsed = FMath::IsNearlyEqual(CollapseAnimAlpha, 1.f);

	if (PanelOuter)
	{
		// Slide panel leftwards off-screen as it collapses (GPU-level, no layout churn)
		PanelOuter->SetRenderTranslation(FVector2D(-ExpandedTabPos.X * t, 0.f));
		PanelOuter->SetRenderOpacity(1.f - t);
		PanelOuter->SetVisibility(bFullyCollapsed
			? ESlateVisibility::Collapsed
			: ESlateVisibility::Visible);
	}

	if (ToggleTab)
	{
		if (UCanvasPanelSlot* CPS = Cast<UCanvasPanelSlot>(ToggleTab->Slot))
		{
			const float X = FMath::Lerp(ExpandedTabPos.X, 0.f, t);
			CPS->SetPosition(FVector2D(X, ExpandedTabPos.Y));
		}
	}
}

void UEarthWidgetUI::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (FMath::IsNearlyEqual(CollapseAnimAlpha, CollapseAnimTarget))
	{
		return;
	}

	const float Step = InDeltaTime / kCollapseAnimDuration;
	if (CollapseAnimAlpha < CollapseAnimTarget)
	{
		CollapseAnimAlpha = FMath::Min(CollapseAnimAlpha + Step, CollapseAnimTarget);
	}
	else
	{
		CollapseAnimAlpha = FMath::Max(CollapseAnimAlpha - Step, CollapseAnimTarget);
	}
	ApplyCollapseAnim();
}

void UEarthWidgetUI::UpdateBuildingModeVisual()
{
	const bool bWhite   = (BuildingMode == EBuildingMode::WhiteModel);
	const bool bDtiles  = (BuildingMode == EBuildingMode::Dtiles);
	const bool bImagery = (BuildingMode == EBuildingMode::Imagery);

	auto ApplyTile = [&](UBorder* Border, UTextBlock* Icon, UTextBlock* Lbl, bool bActive)
	{
		if (Border) Border->SetBrushColor(bActive ? kTileActiveBg : kTileInactiveBg);
		if (Icon) Icon->SetColorAndOpacity(FSlateColor(bActive ? kTileActiveFg : kTileInactiveFg));
		if (Lbl) Lbl->SetColorAndOpacity(FSlateColor(bActive ? kTileActiveFg : kTileInactiveFg));
	};

	ApplyTile(TileSAT, TileSATIcon, TileSATLbl, bWhite);
	ApplyTile(TileSTREET, TileSTREETIcon, TileSTREETLbl, bDtiles);
	ApplyTile(TileIMG, TileIMGIcon, TileIMGLbl, bImagery);

	// Active underline — each tile has its own LineBox; show the one matching current mode
	auto SetLine = [](USizeBox* Line, bool bVisible)
	{
		if (Line) Line->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	};
	SetLine(TileIMGLineBox, bImagery);
	SetLine(TileSATLineBox, bWhite);
	SetLine(TileSTREETLineBox, bDtiles);
}

void UEarthWidgetUI::LoadCityTileset(int32 CityIndex)
{
	if (!CityDataArray.IsValidIndex(CityIndex))
	{
		return;
	}

	if (CurrentTileset)
	{
		CurrentTileset->Destroy();
		CurrentTileset = nullptr;
	}

	SetFlyingStatus(true);

	const FString CityUrl = CityDataArray[CityIndex].Url;

	TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(CityUrl);
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	TWeakObjectPtr<UEarthWidgetUI> WeakThis(this);
	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis, CityIndex, CityUrl](FHttpRequestPtr, FHttpResponsePtr Response, bool bSuccess)
		{
			double ECEFx = 0, ECEFy = 0, ECEFz = 0;
			bool bHasBox = false;

			if (bSuccess && Response.IsValid())
			{
				const TArray<uint8>& Content = Response->GetContent();
				if (Content.Num() > 0)
				{
					TArray<uint8> Buffer = Content;
					Buffer.Add(0);
					FString Json = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Buffer.GetData())));

					const int32 BoxStart = Json.Find(TEXT("\"box\":["));
					if (BoxStart != INDEX_NONE)
					{
						const int32 ArrStart = Json.Find(TEXT("["), ESearchCase::IgnoreCase, ESearchDir::FromStart, BoxStart + 5);
						if (ArrStart != INDEX_NONE)
						{
							const int32 ArrEnd = Json.Find(TEXT("]"), ESearchCase::IgnoreCase, ESearchDir::FromStart, ArrStart);
							if (ArrEnd != INDEX_NONE)
							{
								FString ArrayContent = Json.Mid(ArrStart + 1, ArrEnd - ArrStart - 1);
								TArray<FString> Values;
								ArrayContent.ParseIntoArray(Values, TEXT(","), true);
								if (Values.Num() >= 3)
								{
									ECEFx = FCString::Atod(*Values[0].TrimStartAndEnd());
									ECEFy = FCString::Atod(*Values[1].TrimStartAndEnd());
									ECEFz = FCString::Atod(*Values[2].TrimStartAndEnd());
									bHasBox = true;
								}
							}
						}
					}
				}
			}

			AsyncTask(ENamedThreads::GameThread, [WeakThis, CityIndex, CityUrl, ECEFx, ECEFy, ECEFz, bHasBox]()
			{
				if (!WeakThis.IsValid())
				{
					return;
				}
				UEarthWidgetUI* Self = WeakThis.Get();
				if (!Self->CityDataArray.IsValidIndex(CityIndex))
				{
					Self->SetFlyingStatus(false);
					return;
				}

				const FString Url = Self->CityDataArray[CityIndex].Url;

				if (UWorld* World = Self->GetWorld())
				{
					Self->CurrentTileset = World->SpawnActor<ACesium3DTileset>();
					if (Self->CurrentTileset)
					{
						Self->CurrentTileset->SetTilesetSource(ETilesetSource::FromUrl);
						Self->CurrentTileset->SetUrl(Url);
						// Only show when Dtiles mode is active
						Self->CurrentTileset->SetActorHiddenInGame(Self->BuildingMode != EBuildingMode::Dtiles);
					}
				}

				if (bHasBox && Self->CameraController && Self->CameraController->Georeference)
				{
					GeoTransforms T = Self->CameraController->Georeference->GetGeoTransforms();
					glm::dvec3 EcefVec(ECEFx, ECEFy, ECEFz);
					glm::dvec3 LLH = T.TransformEcefToLongitudeLatitudeHeight(EcefVec);

					Self->CityDataArray[CityIndex].bHasLatLon = true;
					Self->CityDataArray[CityIndex].Lon = LLH.x;
					Self->CityDataArray[CityIndex].Lat = LLH.y;
					Self->RebuildCityList();
					Self->FlyToLonLat(LLH.x, LLH.y, LLH.z);
				}
				else
				{
					Self->SetFlyingStatus(false);
				}
			});
		});

	Request->ProcessRequest();
}

void UEarthWidgetUI::FlyToLonLat(double Lon, double Lat, double Height)
{
	if (!CameraController)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			CameraController = Cast<AGeoCameraController>(PC->GetPawn());
		}
	}

	if (!CameraController || !CameraController->Georeference)
	{
		SetFlyingStatus(false);
		return;
	}

	// Restore original scaling: target distance scales with city height, min 50km
	const double TargetDistance = FMath::Max(50000.0, Height * 0.5);
	CameraController->FlyToLocationLLA(Lon, Lat, 0.0, TargetDistance, 0.0, -45.0, 3.0f);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StatusResetTimer);
		TWeakObjectPtr<UEarthWidgetUI> WeakThis(this);
		World->GetTimerManager().SetTimer(StatusResetTimer,
			FTimerDelegate::CreateLambda([WeakThis]()
			{
				if (WeakThis.IsValid())
				{
					WeakThis->SetFlyingStatus(false);
				}
			}),
			3.2f, false);
	}
}

void UEarthWidgetUI::SetFlyingStatus(bool bFlying)
{
	if (HeaderStatus)
	{
		HeaderStatus->SetText(FText::FromString(bFlying ? TEXT("\u524D\u5F80\u4E2D") : TEXT("\u5728\u7EBF")));
		HeaderStatus->SetColorAndOpacity(FSlateColor(bFlying ? kCyan : FLinearColor(0, 0.4792f, 0.0934f, 1.f)));
	}
	if (StatusDot)
	{
		StatusDot->SetBrushColor(bFlying ? kStatusFlyingAmber : kStatusOnlineGreen);
	}
}
