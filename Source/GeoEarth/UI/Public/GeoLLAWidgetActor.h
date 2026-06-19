#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GeoLLAWidgetActor.generated.h"

class UUserWidget;

UCLASS()
class GEOEARTH_API AGeoLLAWidgetActor : public AActor
{
	GENERATED_BODY()
	
public:
	AGeoLLAWidgetActor();

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UUserWidget> LLAWidgetClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	UUserWidget* LLADisplayWidget;
};