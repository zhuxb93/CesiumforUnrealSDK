#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EarthWidgetActor.generated.h"

class UUserWidget;

UCLASS()
class GEOEARTH_API AEarthWidgetActor : public AActor
{
	GENERATED_BODY()
	
public:	
	AEarthWidgetActor();

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UUserWidget> WidgetClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	UUserWidget* CreatedWidget;
};