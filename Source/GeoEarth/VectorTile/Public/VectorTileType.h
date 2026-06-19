#pragma once

#include "CoreMinimal.h"

UENUM(BlueprintType)
enum class EVectorTileType : uint8
{
	Buia    UMETA(DisplayName = "Building"),
	Road    UMETA(DisplayName = "Road"),
	Lrrl    UMETA(DisplayName = "Lrrl"),
	Tree    UMETA(DisplayName = "Tree"),
	Poi     UMETA(DisplayName = "POI")
};
