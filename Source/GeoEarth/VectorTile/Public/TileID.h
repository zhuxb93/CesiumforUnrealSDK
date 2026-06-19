#pragma once

#include "CoreMinimal.h"
#include "TileID.generated.h"

USTRUCT(BlueprintType)
struct FTileID
{
    GENERATED_BODY()
    
    UPROPERTY()
    uint32 X = 0;
    
    UPROPERTY()
    uint32 Y = 0;
    
    UPROPERTY()
    uint32 Z = 0;
    
    FTileID() {}
    
    FTileID(uint32 InX, uint32 InY, uint32 InZ)
        : X(InX), Y(InY), Z(InZ) {}
    
    static FTileID Create(uint32 InX, uint32 InY, uint32 InZ)
    {
        return FTileID(InX, InY, InZ);
    }
    
    bool operator==(const FTileID& Other) const
    {
        return X == Other.X && Y == Other.Y && Z == Other.Z;
    }
    
    bool operator!=(const FTileID& Other) const
    {
        return !(*this == Other);
    }
    
    friend uint32 GetTypeHash(const FTileID& TileID)
    {
        return HashCombine(HashCombine(GetTypeHash(TileID.X), GetTypeHash(TileID.Y)), GetTypeHash(TileID.Z));
    }
    
    FString ToString() const
    {
        return FString::Printf(TEXT("Tile_%u_%u_%u"), X, Y, Z);
    }
    
    uint32 InvertedY() const
    {
        if (Z == 0 || Z >= 32) return 0;
        return (1U << Z) - Y - 1;
    }
};

USTRUCT(BlueprintType)
struct FTileBounds
{
    GENERATED_BODY()
    
    UPROPERTY()
    FVector2D TopLeft;
    
    UPROPERTY()
    FVector2D BottomRight;
    
    FTileBounds() {}
    
    FTileBounds(const FVector2D& InTopLeft, const FVector2D& InBottomRight)
        : TopLeft(InTopLeft), BottomRight(InBottomRight) {}
    
    FVector2D GetCenter() const
    {
        return FVector2D(
            (TopLeft.X + BottomRight.X) * 0.5,
            (TopLeft.Y + BottomRight.Y) * 0.5
        );
    }
};