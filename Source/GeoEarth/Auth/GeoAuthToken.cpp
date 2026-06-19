#include "Auth/GeoAuthToken.h"

FString FGeoAuthToken::BuildBearerHeader(const FString& AccessToken)
{
    if (AccessToken.IsEmpty())
    {
        return FString();
    }

    return FString::Printf(TEXT("Bearer %s"), *AccessToken);
}

FString FGeoAuthToken::AppendAuthToUrl(const FString& Url)
{
    return AppendAccessKeyPlaceholder(Url, TEXT("YOUR_SERVICE_ACCESS_KEY"));
}

FString FGeoAuthToken::AppendAccessKeyPlaceholder(const FString& Url, const FString& AccessKey)
{
    if (Url.IsEmpty() || AccessKey.IsEmpty())
    {
        return Url;
    }

    const TCHAR* Separator = Url.Contains(TEXT("?")) ? TEXT("&") : TEXT("?");
    return FString::Printf(TEXT("%s%saccess_key=%s"), *Url, Separator, *AccessKey);
}