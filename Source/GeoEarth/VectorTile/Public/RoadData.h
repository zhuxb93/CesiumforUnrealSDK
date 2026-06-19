#pragma once

#include "CoreMinimal.h"
#include "RoadData.generated.h"

USTRUCT(BlueprintType)
struct FRoadData
{
    GENERATED_BODY()
    
    UPROPERTY()
    int32 Id = 0;
    
    UPROPERTY()
    int32 TypeId = 0;
    
    UPROPERTY()
    TArray<FVector> Positions;
};

USTRUCT(BlueprintType)
struct FRoadTile
{
    GENERATED_BODY()
    
    UPROPERTY()
    FVector TileBasePoint;
    
    UPROPERTY()
    TArray<FRoadData> Roads;
};