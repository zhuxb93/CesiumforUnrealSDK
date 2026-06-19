#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SearchPanelWidget.generated.h"

class UEditableTextBox;
class UButton;
class UTextBlock;
class UVerticalBox;
class UBorder;
class USizeBox;
struct FEventReply;
class AGeoCameraController;

/** 单条 POI 搜索结果，对应 Unity 端 GeoPoiSearch 返回 JSON.data.rows 的一项。 */
USTRUCT(BlueprintType)
struct FPoiResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere) FString Name;
    UPROPERTY(BlueprintReadWrite, EditAnywhere) FString Province;
    UPROPERTY(BlueprintReadWrite, EditAnywhere) FString City;
    UPROPERTY(BlueprintReadWrite, EditAnywhere) FString County;
    UPROPERTY(BlueprintReadWrite, EditAnywhere) FString Address;
    UPROPERTY(BlueprintReadWrite, EditAnywhere) double  Lat = 0.0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere) double  Lng = 0.0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPoiSelected, const FPoiResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSearchRequested, const FString&, Query);

/**
 * 地名搜索面板（对应 WBP_SearchPanel / Unity GeoSearchManager）。
 *
 * 与 Unity 端行为对齐：
 *  - 输入框值变化 → 异步 HTTP 搜索 POI → 解析 JSON（status/data.rows）→ 动态生成列表。
 *  - 结果项点击 → AGeoCameraController::FlyToLocationLLA(Lng, Lat, 0, FlyDistance, FlyRotation, FlyTilt, FlyDuration)。
 *  - 支持折叠/展开（ToggleTab 通过 BindWidgetOptional）。
 *
 * 使用步骤：
 *  1. 在 WBP_SearchPanel 的 Class Settings 里把 Parent Class 改为 USearchPanelWidget。
 *  2. 确保 BindWidget 成员名一致（SearchInput / ClearBtn / HintCount / ResultList / Item1_SelBg / Item2_Bg）。
 *  3. 在关卡/HUD 里 CreateWidget<USearchPanelWidget> + AddToViewport；运行时自动从 PlayerController Pawn 拿 AGeoCameraController。
 */
UCLASS()
class GEOEARTH_API USearchPanelWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** 使用外部数据填充结果列表；HighlightIndex >= 0 的项用"选中"样式。 */
    UFUNCTION(BlueprintCallable, Category = "Search")
    void SetResults(const TArray<FPoiResult>& Results, int32 HighlightIndex = 0);

    /** 清空结果列表并隐藏设计态占位。 */
    UFUNCTION(BlueprintCallable, Category = "Search")
    void ClearResults();

    /** 更新提示行数字（如 "共 8 个结果"）。 */
    UFUNCTION(BlueprintCallable, Category = "Search")
    void SetHintCount(int32 Count);

    /** 手动触发一次 POI 搜索（蓝图可调用）。空 Query 则清空结果。 */
    UFUNCTION(BlueprintCallable, Category = "Search")
    void PerformSearch(const FString& Query);

    /** 飞到指定 POI（点击结果时内部也会调用此方法）。 */
    UFUNCTION(BlueprintCallable, Category = "Search")
    void FlyToResult(const FPoiResult& Result);

    /** 外部指定/切换 CameraController（若未指定会自动从 PlayerController Pawn 解析）。 */
    UFUNCTION(BlueprintCallable, Category = "Search")
    void SetCameraController(AGeoCameraController* Controller) { CameraController = Controller; }

    /** 监听：用户点击了某条结果。 */
    UPROPERTY(BlueprintAssignable, Category = "Search")
    FOnPoiSelected OnPoiSelected;

    /** 监听：输入变化 / 提交 / 清除时触发，Query 为当前输入。 */
    UPROPERTY(BlueprintAssignable, Category = "Search")
    FOnSearchRequested OnSearchRequested;

    // -------- 运行时配置（蓝图可编辑） --------

    /**
     * POI 搜索 API URL（与 Unity 端 GeoPoiSearch.cs 的 geoRecommend 一致）。
     * 留空则只广播 OnSearchRequested 不自动请求，由上层接管搜索逻辑。
     * 运行时自动拼接国图鉴权查询串 (clientId/clientId/expireTime/sign)。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Search|API")
    FString PoiSearchUrl = TEXT("https://example.com/service-endpoint");

    /** 单次搜索返回上限（对齐 Unity limit=15）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Search|API")
    int32 SearchLimit = 15;

    /** 飞行距离（对齐 Unity 默认 2000m）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Search|Fly")
    double FlyDistance = 2000.0;

    /** 飞行方位角（对齐 Unity 默认 0°）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Search|Fly")
    double FlyRotation = 0.0;

    /** 飞行俯仰角（Unity pitch=5 对应 UE tilt=-5，负值向下俯视）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Search|Fly")
    double FlyTilt = -5.0;

    /** 飞行时长（对齐 Unity 默认 1.5s）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Search|Fly")
    float FlyDuration = 1.5f;

    // -------- 展开/收起 --------

    /** 展开收起动画总时长（秒）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Search|Collapse")
    float CollapseAnimDuration = 0.3f;

    /** 初始是否为收起状态。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Search|Collapse")
    bool bStartCollapsed = false;

    /** 主面板宽度（展开态目标宽度；收起态插值到 0）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Search|Collapse")
    float ExpandedPanelWidth = 380.f;

    /** 切换展开 / 收起。 */
    UFUNCTION(BlueprintCallable, Category = "Search|Collapse")
    void TogglePanel();

    /** 立即设置展开或收起（带动画）。 */
    UFUNCTION(BlueprintCallable, Category = "Search|Collapse")
    void SetPanelCollapsed(bool bCollapsed);

    /** 当前是否处于收起状态。 */
    UFUNCTION(BlueprintCallable, Category = "Search|Collapse")
    bool IsPanelCollapsed() const { return bPanelCollapsed; }

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // --- BindWidget：WBP_SearchPanel 中同名 widget 自动绑定 ---
    UPROPERTY(meta = (BindWidget))
    UEditableTextBox* SearchInput = nullptr;

    UPROPERTY(meta = (BindWidget))
    UButton* ClearBtn = nullptr;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* HintCount = nullptr;

    UPROPERTY(meta = (BindWidget))
    UVerticalBox* ResultList = nullptr;

    /** 设计态占位：选中样式原型。 */
    UPROPERTY(meta = (BindWidgetOptional))
    UBorder* Item1_SelBg = nullptr;

    /** 设计态占位：普通样式原型。 */
    UPROPERTY(meta = (BindWidgetOptional))
    UBorder* Item2_Bg = nullptr;

    /** ToggleTab 的点击区域（整个深色竖条 Border，带 OnMouseButtonDown）。 */
    UPROPERTY(meta = (BindWidgetOptional))
    UBorder* ToggleTabBg = nullptr;

    /** ToggleTab 顶部箭头，展开态 ◀ / 收起态 ▶。 */
    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* ToggleArrow = nullptr;

    /** 主面板容器（会在动画中被 Lerp 宽度）。 */
    UPROPERTY(meta = (BindWidgetOptional))
    USizeBox* PanelSize = nullptr;

    /** 相机控制器（不强制绑定 widget，运行时解析获得）。 */
    UPROPERTY(BlueprintReadWrite, Category = "Search")
    AGeoCameraController* CameraController = nullptr;

private:
    UFUNCTION() void HandleTextChanged(const FText& Text);
    UFUNCTION() void HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);
    UFUNCTION() void HandleClearClicked();
    UFUNCTION() void HandleItemClicked();
    UFUNCTION() FEventReply HandleToggleMouseDown(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

    /** 根据 CollapseAnimAlpha 应用可见性、宽度与箭头方向。 */
    void ApplyCollapseAnim();

    /** 解析国图 POI 搜索响应 JSON（status/data.rows）。 */
    void HandleSearchResponse(const FString& ResponseBody);

    /** 构造并添加一行结果 widget 到 ResultList。 */
    void AppendResultRow(const FPoiResult& Item, bool bSelected);

    /** 若 CameraController 为空，尝试从 PlayerController Pawn 解析。 */
    void EnsureCameraController();

    /** 最近一次 POI 结果数据缓存与对应按钮，用于 HandleItemClicked 定位。 */
    TArray<FPoiResult> ResultCache;
    TArray<TWeakObjectPtr<UButton>> ResultButtons;

    /** 用于抑制并发请求；仅保留最新 query 的响应。 */
    int32 SearchRequestCounter = 0;

    /** 当前是否为收起状态。 */
    bool bPanelCollapsed = false;

    /** 动画插值进度：0 = 完全展开，1 = 完全收起。 */
    float CollapseAnimAlpha = 0.f;
    float CollapseAnimTarget = 0.f;
};
