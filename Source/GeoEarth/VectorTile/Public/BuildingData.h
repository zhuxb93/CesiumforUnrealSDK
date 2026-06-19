#pragma once

#include "CoreMinimal.h"
#include "BuildingData.generated.h"

USTRUCT(BlueprintType)
struct FBuildingData
{
    GENERATED_BODY()
    
    UPROPERTY()
    int32 Id = 0;
    
    UPROPERTY()
    float CenterZ = 0.0f;
    
    UPROPERTY()
    float Height = 0.0f;
    
    UPROPERTY()
    TArray<FVector> BottomPoints;
    
    UPROPERTY()
    TArray<uint16> Triangles;
    
    UPROPERTY()
    TArray<uint16> Holes;
};

USTRUCT(BlueprintType)
struct FBuildingTile
{
    GENERATED_BODY()
    
    UPROPERTY()
    FVector TileBasePoint;
    
    UPROPERTY()
    TArray<FBuildingData> Buildings;
};