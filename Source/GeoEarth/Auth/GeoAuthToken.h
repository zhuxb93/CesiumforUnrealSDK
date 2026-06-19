#pragma once

#include "CoreMinimal.h"

class GEOEARTH_API FGeoAuthToken
{
public:
    static FString BuildBearerHeader(const FString& AccessToken);
    static FString AppendAuthToUrl(const FString& Url);
    static FString AppendAccessKeyPlaceholder(const FString& Url, const FString& AccessKey);
};