#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GeoDynamicToken.generated.h"

UCLASS(ClassGroup=(GeoEarth), meta=(BlueprintSpawnableComponent))
class GEOEARTH_API UGeoDynamicToken : public UActorComponent
{
    GENERATED_BODY()

public:
    UGeoDynamicToken();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Auth")
    FString TokenEndpoint = TEXT("https://example.com/token");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Auth")
    FString CurrentAccessToken = TEXT("YOUR_SERVICE_ACCESS_TOKEN");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Auth")
    float RefreshIntervalSeconds = 1800.0f;

    UFUNCTION(BlueprintCallable, Category="Auth")
    void RefreshToken();

    UFUNCTION(BlueprintCallable, Category="Auth")
    FString BuildAuthorizationHeader() const;

private:
    FTimerHandle RefreshTimerHandle;

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
