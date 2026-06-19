#include "FrameRateMonitor.h"

void FFrameRateMonitor::Initialize(float InTargetFPS, int32 InSampleCount)
{
	TargetFrameRate = InTargetFPS;
	SampleCount = InSampleCount;
	FrameTimes.Reserve(SampleCount);
	AverageFrameTime = 1.0f / TargetFrameRate;
	TotalFrameTime = 0.0f;
}

void FFrameRateMonitor::UpdateFrameTime(float DeltaTime)
{
	TotalFrameTime += DeltaTime;
	FrameTimes.Add(DeltaTime);

	if (FrameTimes.Num() > SampleCount)
	{
		TotalFrameTime -= FrameTimes[0];
		FrameTimes.RemoveAt(0);
	}

	AverageFrameTime = TotalFrameTime / FrameTimes.Num();
	CurrentFPS = 1.0f / AverageFrameTime;
}

float FFrameRateMonitor::GetAverageFPS() const
{
	return CurrentFPS;
}

bool FFrameRateMonitor::IsFrameRateLow() const
{
	return CurrentFPS < TargetFrameRate * 0.75f;
}

bool FFrameRateMonitor::IsFrameRateCritical() const
{
	return CurrentFPS < TargetFrameRate * 0.5f;
}

int32 FFrameRateMonitor::GetRecommendedTilesPerFrame(int32 MinTiles, int32 MaxTiles) const
{
	float FPSRatio = CurrentFPS / TargetFrameRate;

	int32 RecommendedTiles;
	if (FPSRatio >= 0.92f)
	{
		RecommendedTiles = MaxTiles;
	}
	else if (FPSRatio >= 0.75f)
	{
		RecommendedTiles = FMath::Max(MinTiles, FMath::RoundToInt(MaxTiles * 0.66f));
	}
	else if (FPSRatio >= 0.5f)
	{
		RecommendedTiles = FMath::Max(MinTiles, FMath::RoundToInt(MaxTiles * 0.33f));
	}
	else
	{
		RecommendedTiles = MinTiles;
	}

	return FMath::Clamp(RecommendedTiles, MinTiles, MaxTiles);
}
