#include "SearchPanelWidget.h"

#include "GeoAuthToken.h"
#include "GeoCameraController.h"
#include "EngineUtils.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/SlateWrapperTypes.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    // 颜色常量（线性空间，对应 Pencil 设计稿）
    const FLinearColor kBgSelected     = FLinearColor(0.000000f, 0.024000f, 0.068000f, 1.0f); // #002B4A
    const FLinearColor kAccentCyan     = FLinearColor(0.000000f, 0.658400f, 1.000000f, 1.0f); // #00D4FF
    const FLinearColor kTextBright     = FLinearColor(0.729000f, 0.870000f, 1.000000f, 1.0f); // #E0EFFF
    const FLinearColor kTextPrimary    = FLinearColor(0.509000f, 0.687000f, 0.870000f, 1.0f); // #C0D8F0
    const FLinearColor kTextSecondary  = FLinearColor(0.144000f, 0.255000f, 0.420000f, 1.0f); // #6B8AB3
    const FLinearColor kTextTertiary   = FLinearColor(0.068000f, 0.195000f, 0.420000f, 1.0f); // #4A7AB0
    const FLinearColor kTransparent    = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);

    UTexture2D* LoadIconTexture(const TCHAR* Name)
    {
        const FString Path = FString::Printf(TEXT("/Game/UI/DarkBlue/Icons/%s.%s"), Name, Name);
        return LoadObject<UTexture2D>(nullptr, *Path);
    }

    void ApplyImageBrush(UImage* Img, UTexture2D* Tex, FVector2D Size, const FLinearColor& Tint)
    {
        if (!Img || !Tex) return;
        FSlateBrush Brush;
        Brush.SetResourceObject(Tex);
        Brush.ImageSize = Size;
        Brush.TintColor = FSlateColor(Tint);
        Brush.DrawAs = ESlateBrushDrawType::Image;
        Img->SetBrush(Brush);
    }
}

void USearchPanelWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (SearchInput)
    {
        SearchInput->OnTextChanged.RemoveAll(this);
        SearchInput->OnTextCommitted.RemoveAll(this);
        SearchInput->OnTextChanged.AddDynamic(this, &USearchPanelWidget::HandleTextChanged);
        SearchInput->OnTextCommitted.AddDynamic(this, &USearchPanelWidget::HandleTextCommitted);
    }
    if (ClearBtn)
    {
        ClearBtn->OnClicked.RemoveAll(this);
        ClearBtn->OnClicked.AddDynamic(this, &USearchPanelWidget::HandleClearClicked);
    }

    // 绑定 ToggleTab 点击（整个 24×140 Border 作为点击区）
    if (ToggleTabBg)
    {
        ToggleTabBg->OnMouseButtonDownEvent.Clear();
        ToggleTabBg->OnMouseButtonDownEvent.BindUFunction(this, TEXT("HandleToggleMouseDown"));
        ToggleTabBg->SetCursor(EMouseCursor::Hand);
    }

    ClearResults();
    SetHintCount(0);
    EnsureCameraController();

    // 初始化展开/收起状态与外观
    bPanelCollapsed = bStartCollapsed;
    CollapseAnimAlpha = bPanelCollapsed ? 1.f : 0.f;
    CollapseAnimTarget = CollapseAnimAlpha;
    ApplyCollapseAnim();
}

void USearchPanelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!FMath::IsNearlyEqual(CollapseAnimAlpha, CollapseAnimTarget))
    {
        const float Rate = 1.f / FMath::Max(CollapseAnimDuration, 0.01f);
        CollapseAnimAlpha = FMath::FInterpConstantTo(
            CollapseAnimAlpha, CollapseAnimTarget, InDeltaTime, Rate);
        ApplyCollapseAnim();
    }
}

void USearchPanelWidget::TogglePanel()
{
    SetPanelCollapsed(!bPanelCollapsed);
}

void USearchPanelWidget::SetPanelCollapsed(bool bCollapsed)
{
    bPanelCollapsed = bCollapsed;
    CollapseAnimTarget = bCollapsed ? 1.f : 0.f;
    // 箭头立即切换（不等动画完成），与 EarthWidgetUI 风格一致
    if (ToggleArrow)
    {
        ToggleArrow->SetText(FText::FromString(bCollapsed ? TEXT("\u25B6") : TEXT("\u25C0")));
    }
}

void USearchPanelWidget::ApplyCollapseAnim()
{
    // ease-in-out quad
    const float T = FMath::Clamp(CollapseAnimAlpha, 0.f, 1.f);
    const float Eased = (T < 0.5f) ? (2.f * T * T) : (1.f - FMath::Pow(-2.f * T + 2.f, 2.f) / 2.f);

    if (PanelSize)
    {
        if (Eased >= 0.999f)
        {
            PanelSize->SetVisibility(ESlateVisibility::Collapsed);
        }
        else
        {
            PanelSize->SetVisibility(ESlateVisibility::Visible);
            PanelSize->SetWidthOverride(FMath::Lerp(ExpandedPanelWidth, 0.f, Eased));
        }
    }

    if (ToggleArrow)
    {
        ToggleArrow->SetText(FText::FromString(bPanelCollapsed ? TEXT("\u25B6") : TEXT("\u25C0")));
    }
}

FEventReply USearchPanelWidget::HandleToggleMouseDown(FGeometry /*MyGeometry*/, const FPointerEvent& MouseEvent)
{
    if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        TogglePanel();
        FEventReply Reply;
        Reply.NativeReply = FReply::Handled();
        return Reply;
    }
    FEventReply Unhandled;
    Unhandled.NativeReply = FReply::Unhandled();
    return Unhandled;
}

void USearchPanelWidget::EnsureCameraController()
{
    if (CameraController && CameraController->Georeference) return;

    if (APlayerController* PC = GetOwningPlayer())
    {
        if (AGeoCameraController* AsCam = Cast<AGeoCameraController>(PC->GetPawn()))
        {
            CameraController = AsCam;
            return;
        }
    }

    // 备用：遍历关卡找任意一个 AGeoCameraController（和 EarthWidgetUI 的容错一致）
    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<AGeoCameraController> It(World); It; ++It)
        {
            CameraController = *It;
            return;
        }
    }
}

void USearchPanelWidget::ClearResults()
{
    ResultCache.Reset();
    ResultButtons.Reset();

    if (ResultList)
    {
        ResultList->ClearChildren();
    }
    if (Item1_SelBg) Item1_SelBg->SetVisibility(ESlateVisibility::Collapsed);
    if (Item2_Bg)    Item2_Bg->SetVisibility(ESlateVisibility::Collapsed);
}

void USearchPanelWidget::SetHintCount(int32 Count)
{
    if (HintCount)
    {
        HintCount->SetText(FText::Format(
            NSLOCTEXT("SearchPanel", "HintCount", "\u5171 {0} \u4e2a\u7ed3\u679c"),
            FText::AsNumber(Count)));
    }
}

void USearchPanelWidget::SetResults(const TArray<FPoiResult>& Results, int32 HighlightIndex)
{
    ClearResults();
    ResultCache = Results;
    ResultButtons.Reserve(Results.Num());

    for (int32 i = 0; i < Results.Num(); ++i)
    {
        AppendResultRow(Results[i], i == HighlightIndex);
    }

    SetHintCount(Results.Num());
}

void USearchPanelWidget::PerformSearch(const FString& Query)
{
    OnSearchRequested.Broadcast(Query);

    if (Query.IsEmpty())
    {
        ClearResults();
        SetHintCount(0);
        return;
    }

    if (PoiSearchUrl.IsEmpty())
    {
        // 未配置 URL：只广播 delegate 让上层处理
        return;
    }

    const int32 ThisRequestId = ++SearchRequestCounter;

    // 拼接鉴权 query（与 Unity GeoPoiSearch.cs 保持一致）
    const FString FullUrl = FGeoAuthToken::AppendAuthToUrl(PoiSearchUrl);

    // POST body: 对齐 Unity PostBodyModel
    const FString Body = FString::Printf(
        TEXT("{\"keyword\":\"%s\",\"limit\":\"%d\",\"page\":\"1\",\"searchAllWordFlag\":\"0\",\"id\":\"\",\"poiType\":\"\",\"isCluster\":\"0\"}"),
        *Query.ReplaceCharWithEscapedChar(), SearchLimit);

    TSharedRef<IHttpRequest> Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(FullUrl);
    Req->SetVerb(TEXT("POST"));
    Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Req->SetHeader(TEXT("Accept"), TEXT("application/json"));
    Req->SetContentAsString(Body);

    TWeakObjectPtr<USearchPanelWidget> WeakThis(this);
    Req->OnProcessRequestComplete().BindLambda(
        [WeakThis, ThisRequestId](FHttpRequestPtr, FHttpResponsePtr Response, bool bSuccess)
        {
            if (!WeakThis.IsValid() || !bSuccess || !Response.IsValid()) return;
            if (WeakThis->SearchRequestCounter != ThisRequestId) return; // 丢弃过期响应
            WeakThis->HandleSearchResponse(Response->GetContentAsString());
        });
    Req->ProcessRequest();
}

void USearchPanelWidget::HandleSearchResponse(const FString& Body)
{
    // 对齐 Unity GeoSearchManager.cs:29-50 的解析逻辑
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        SetResults({});
        return;
    }

    FString Status;
    if (Root->TryGetStringField(TEXT("status"), Status) && Status != TEXT("200"))
    {
        SetResults({});
        return;
    }

    const TSharedPtr<FJsonObject>* Data = nullptr;
    if (!Root->TryGetObjectField(TEXT("data"), Data) || !Data->IsValid())
    {
        SetResults({});
        return;
    }

    const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
    if (!(*Data)->TryGetArrayField(TEXT("rows"), Rows) || !Rows)
    {
        SetResults({});
        return;
    }

    TArray<FPoiResult> Results;
    Results.Reserve(Rows->Num());
    for (const TSharedPtr<FJsonValue>& V : *Rows)
    {
        if (!V.IsValid()) continue;
        const TSharedPtr<FJsonObject> Row = V->AsObject();
        if (!Row.IsValid()) continue;

        FPoiResult R;
        Row->TryGetStringField(TEXT("name"), R.Name);
        Row->TryGetStringField(TEXT("province"), R.Province);
        Row->TryGetStringField(TEXT("city"), R.City);
        Row->TryGetStringField(TEXT("county"), R.County);
        Row->TryGetStringField(TEXT("address"), R.Address);
        Row->TryGetNumberField(TEXT("lat"), R.Lat);
        Row->TryGetNumberField(TEXT("lng"), R.Lng);
        Results.Add(MoveTemp(R));
    }

    SetResults(Results);
}

void USearchPanelWidget::FlyToResult(const FPoiResult& Result)
{
    EnsureCameraController();

    if (!CameraController)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[SearchPanel] FlyToResult aborted: no AGeoCameraController found. ")
            TEXT("Make sure the player Pawn is AGeoCameraController, or call SetCameraController() from Blueprint."));
        return;
    }
    if (!CameraController->Georeference)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[SearchPanel] FlyToResult aborted: CameraController has no Georeference set."));
        return;
    }

    UE_LOG(LogTemp, Log,
        TEXT("[SearchPanel] FlyTo '%s' lng=%.6f lat=%.6f dist=%.1f tilt=%.1f dur=%.2f"),
        *Result.Name, Result.Lng, Result.Lat, FlyDistance, FlyTilt, FlyDuration);

    // 与 Unity GeoSearchManager.cs:86-91 一致：FlyTo(ecef, distance=2000, heading=0, pitch=5, duration=1.5)
    CameraController->FlyToLocationLLA(
        Result.Lng, Result.Lat, /*Altitude*/ 0.0,
        FlyDistance, FlyRotation, FlyTilt, FlyDuration);
}

void USearchPanelWidget::AppendResultRow(const FPoiResult& Item, bool bSelected)
{
    if (!ResultList) return;

    UButton* RowBtn = NewObject<UButton>(this);
    if (!RowBtn) return;

    FButtonStyle Style;
    Style.Normal.DrawAs    = ESlateBrushDrawType::Image;
    Style.Normal.TintColor = FSlateColor(bSelected ? kBgSelected : kTransparent);
    Style.Hovered.DrawAs   = ESlateBrushDrawType::Image;
    Style.Hovered.TintColor = FSlateColor(FLinearColor(0.0f, 0.04f, 0.10f, 1.0f));
    Style.Pressed.DrawAs   = ESlateBrushDrawType::Image;
    Style.Pressed.TintColor = FSlateColor(FLinearColor(0.0f, 0.06f, 0.14f, 1.0f));
    RowBtn->SetStyle(Style);
    RowBtn->OnClicked.AddDynamic(this, &USearchPanelWidget::HandleItemClicked);

    UHorizontalBox* Row = NewObject<UHorizontalBox>(this);
    RowBtn->AddChild(Row);

    if (bSelected)
    {
        USizeBox* AccentSize = NewObject<USizeBox>(this);
        AccentSize->SetWidthOverride(2.0f);
        UBorder* AccentBar = NewObject<UBorder>(this);
        AccentBar->SetBrushColor(kAccentCyan);
        AccentSize->AddChild(AccentBar);
        if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(AccentSize))
        {
            S->SetVerticalAlignment(VAlign_Fill);
        }
    }

    UBorder* Inner = NewObject<UBorder>(this);
    Inner->SetBrushColor(kTransparent);
    Inner->SetPadding(FMargin(14.0f, 10.0f));
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Inner))
    {
        S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }

    UHorizontalBox* InnerRow = NewObject<UHorizontalBox>(this);
    Inner->SetContent(InnerRow);

    UImage* Pin = NewObject<UImage>(this);
    ApplyImageBrush(Pin, LoadIconTexture(TEXT("map-pin")), FVector2D(16, 16),
                    bSelected ? kAccentCyan : kTextTertiary);
    if (UHorizontalBoxSlot* S = InnerRow->AddChildToHorizontalBox(Pin))
    {
        S->SetPadding(FMargin(0, 0, 12, 0));
        S->SetVerticalAlignment(VAlign_Center);
    }

    UVerticalBox* TextCol = NewObject<UVerticalBox>(this);
    if (UHorizontalBoxSlot* S = InnerRow->AddChildToHorizontalBox(TextCol))
    {
        S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        S->SetVerticalAlignment(VAlign_Center);
    }

    UTextBlock* NameText = NewObject<UTextBlock>(this);
    NameText->SetText(FText::FromString(Item.Name));
    {
        FSlateFontInfo F = NameText->GetFont();
        F.TypefaceFontName = bSelected ? TEXT("Bold") : TEXT("Regular");
        F.Size = 11;
        F.LetterSpacing = 50;
        NameText->SetFont(F);
    }
    NameText->SetColorAndOpacity(FSlateColor(bSelected ? kTextBright : kTextPrimary));
    if (UVerticalBoxSlot* S = TextCol->AddChildToVerticalBox(NameText))
    {
        S->SetPadding(FMargin(0, 0, 0, 3));
    }

    UTextBlock* MetaText = NewObject<UTextBlock>(this);
    MetaText->SetText(FText::FromString(FString::Printf(
        TEXT("%s \u00b7 %s  \u00b7  %.4f, %.4f"),
        *Item.Province, *Item.County, Item.Lat, Item.Lng)));
    {
        FSlateFontInfo F = MetaText->GetFont();
        F.TypefaceFontName = TEXT("Regular");
        F.Size = 8;
        F.LetterSpacing = 50;
        MetaText->SetFont(F);
    }
    MetaText->SetColorAndOpacity(FSlateColor(bSelected ? kTextSecondary : kTextTertiary));
    TextCol->AddChildToVerticalBox(MetaText);

    UImage* Trail = NewObject<UImage>(this);
    ApplyImageBrush(Trail,
                    LoadIconTexture(bSelected ? TEXT("plane") : TEXT("chevron-right")),
                    FVector2D(14, 14),
                    bSelected ? kAccentCyan : kTextTertiary);
    if (UHorizontalBoxSlot* S = InnerRow->AddChildToHorizontalBox(Trail))
    {
        S->SetVerticalAlignment(VAlign_Center);
    }

    ResultList->AddChild(RowBtn);
    ResultButtons.Add(RowBtn);
}

void USearchPanelWidget::HandleTextChanged(const FText& Text)
{
    // Unity GeoSearchManager 是 onValueChanged → 实时搜索
    PerformSearch(Text.ToString());
}

void USearchPanelWidget::HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
    if (CommitMethod == ETextCommit::OnEnter)
    {
        PerformSearch(Text.ToString());
    }
}

void USearchPanelWidget::HandleClearClicked()
{
    if (SearchInput)
    {
        SearchInput->SetText(FText::GetEmpty());
    }
    ClearResults();
    SetHintCount(0);
    OnSearchRequested.Broadcast(FString());
}

void USearchPanelWidget::HandleItemClicked()
{
    // OnClicked 触发时按钮已释放 → IsPressed() 为 false；改用 IsHovered() 定位被点击的 Button。
    for (int32 i = 0; i < ResultButtons.Num(); ++i)
    {
        UButton* Btn = ResultButtons[i].Get();
        if (Btn && Btn->IsHovered())
        {
            if (ResultCache.IsValidIndex(i))
            {
                const FPoiResult& Picked = ResultCache[i];
                UE_LOG(LogTemp, Log, TEXT("[SearchPanel] Item %d clicked: %s"), i, *Picked.Name);
                OnPoiSelected.Broadcast(Picked);
                FlyToResult(Picked);
            }
            return;
        }
    }
    UE_LOG(LogTemp, Warning, TEXT("[SearchPanel] HandleItemClicked: no hovered button resolved (total=%d)"), ResultButtons.Num());
}
