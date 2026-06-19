#include "Auth/GeoDynamicToken.h"
#include "Auth/GeoAuthToken.h"
#include "TimerManager.h"

UGeoDynamicToken::UGeoDynamicToken()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UGeoDynamicToken::BeginPlay()
{
    Super::BeginPlay();

    if (RefreshIntervalSeconds > 0.0f && GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(RefreshTimerHandle, this, &UGeoDynamicToken::RefreshToken, RefreshIntervalSeconds, true);
    }
}

void UGeoDynamicToken::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(RefreshTimerHandle);
    }

    Super::EndPlay(EndPlayReason);
}

void UGeoDynamicToken::RefreshToken()
{
    // 示例只保留刷新骨架。真实项目应在这里调用自己的公开鉴权服务，
    // 并把密钥放在服务器或安全配置中，避免写入客户端源码。
    if (CurrentAccessToken.IsEmpty())
    {
        CurrentAccessToken = TEXT("YOUR_SERVICE_ACCESS_TOKEN");
    }
}

FString UGeoDynamicToken::BuildAuthorizationHeader() const
{
    return FGeoAuthToken::BuildBearerHeader(CurrentAccessToken);
}
