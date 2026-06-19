#include "CoordinateConverter.h"

FVector FCoordinateConverter::ComputeGeodeticSurfaceNormal(const FVector& EcefPos)
{
	double X2 = EcefPos.X * EcefPos.X;
	double Y2 = EcefPos.Y * EcefPos.Y;
	double Z2 = EcefPos.Z * EcefPos.Z;

	double A2 = WGS84_SEMI_MAJOR_AXIS * WGS84_SEMI_MAJOR_AXIS;
	double B2 = WGS84_SEMI_MINOR_AXIS * WGS84_SEMI_MINOR_AXIS;

	double Ratio = (A2 - B2) / A2;

	double Factor = 1.0 / FMath::Sqrt(X2 + Y2 + Z2);

	FVector Result(
		EcefPos.X * Factor,
		EcefPos.Y * Factor,
		EcefPos.Z * Factor * (1.0 - Ratio)
	);

	return Result;
}

bool FCoordinateConverter::EqualsEpsilon(float A, float B, float Epsilon)
{
	return FMath::Abs(A - B) <= Epsilon;
}

FMatrix FCoordinateConverter::GetLocalToEcefMatrix(const FVector& OriginEcef)
{
	float Epsilon = 1e-7f;
	
	if (EqualsEpsilon(OriginEcef.X, 0.0f, Epsilon) &&
		EqualsEpsilon(OriginEcef.Y, 0.0f, Epsilon) &&
		EqualsEpsilon(OriginEcef.Z, 0.0f, Epsilon))
	{
		return FMatrix(
			FPlane(0, 1, 0, 0),
			FPlane(-1, 0, 0, 0),
			FPlane(0, 0, 1, 0),
			FPlane(0, 0, 0, 1)
		);
	}
	
	FVector Up = ComputeGeodeticSurfaceNormal(OriginEcef);
	FVector East = FVector(-OriginEcef.Y, OriginEcef.X, 0.0f);
	if (East.IsNearlyZero(Epsilon))
	{
		// At poles: use arbitrary East direction aligned with X axis
		East = (OriginEcef.Z > 0.0f) ? FVector(1.0f, 0.0f, 0.0f) : FVector(-1.0f, 0.0f, 0.0f);
	}
	else
	{
		East.Normalize();
	}
	FVector North = FVector::CrossProduct(Up, East);
	
	return FMatrix(
		FPlane(East.X, East.Y, East.Z, 0),
		FPlane(Up.X, Up.Y, Up.Z, 0),
		FPlane(North.X, North.Y, North.Z, 0),
		FPlane(OriginEcef.X, OriginEcef.Y, OriginEcef.Z, 1)
	);
}