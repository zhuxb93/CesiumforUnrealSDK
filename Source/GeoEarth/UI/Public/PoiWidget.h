#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "PoiWidget.generated.h"

UCLASS()
class GEOEARTH_API UPoiWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(meta = (BindWidget))
    UTextBlock* PoiName = nullptr;

    void SetPoiName(const FString& Name);
};