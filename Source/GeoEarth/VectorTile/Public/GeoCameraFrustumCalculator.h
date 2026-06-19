#pragma once

#include "CoreMinimal.h"
#include "TileID.h"

class AGeoCameraController;
class ACesiumGeoreference;

namespace GeoCameraFrustum
{
    GEOEARTH_API TSet<FTileID> GetTilesInFrustum(
        AGeoCameraController* CameraController,
        ACesiumGeoreference* Georeference,
        int32 Level);

    bool IsBehindEarth(
        ACesiumGeoreference* Georeference,
        const FVector& CameraWorldPos,
        const FVector& PoiWorldPos);
}