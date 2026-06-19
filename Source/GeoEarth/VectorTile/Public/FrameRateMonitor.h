#pragma once

#include "CoreMinimal.h"

struct FFrameRateMonitor
{
	float TargetFrameRate = 60.0f;
	int32 SampleCount = 60;
	TArray<float> FrameTimes;
	float TotalFrameTime = 0.0f;
	float AverageFrameTime = 0.016f;
	float CurrentFPS = 60.0f;

	void Initialize(float InTargetFPS = 60.0f, int32 InSampleCount = 60);
	void UpdateFrameTime(float DeltaTime);
	float GetAverageFPS() const;
	bool IsFrameRateLow() const;
	bool IsFrameRateCritical() const;
	int32 GetRecommendedTilesPerFrame(int32 MinTiles, int32 MaxTiles) const;
};
