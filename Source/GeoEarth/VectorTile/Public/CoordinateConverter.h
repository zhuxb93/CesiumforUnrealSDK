#pragma once

#include "CoreMinimal.h"

class FCoordinateConverter
{
public:
	static FVector ComputeGeodeticSurfaceNormal(const FVector& EcefPos);
	static FMatrix GetLocalToEcefMatrix(const FVector& OriginEcef);

private:
	static bool EqualsEpsilon(float A, float B, float Epsilon);
	static constexpr double WGS84_SEMI_MAJOR_AXIS = 6378137.0;
	static constexpr double WGS84_SEMI_MINOR_AXIS = 6356752.3142451793;
};