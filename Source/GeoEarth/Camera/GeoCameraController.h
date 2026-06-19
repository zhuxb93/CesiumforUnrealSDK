#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Camera/CameraComponent.h"
#include "Curves/CurveFloat.h"
#include "CesiumRuntime/Public/CesiumGeoreference.h"
#include "GeoCameraController.generated.h"

UENUM(BlueprintType)
enum class EGeoCameraControlMode : uint8
{
	GlobalFree UMETA(DisplayName = "Global Free"),
	FollowActor UMETA(DisplayName = "Follow Actor")
};

UENUM(BlueprintType, Flags)
enum class EGeoAnimationType : uint8
{
	None = 0,
	Move = 1 << 0,
	Zoom = 1 << 1,
	Rotate = 1 << 2,
	Tilt = 1 << 3,
	All = Move | Zoom | Rotate | Tilt
};
ENUM_CLASS_FLAGS(EGeoAnimationType);

UENUM(BlueprintType)
enum class EGeoAnimationStopType : uint8
{
	Finish,
	Break
};

UENUM(BlueprintType, Flags)
enum class EGeoCameraFollowMode : uint8
{
	None = 0,
	Position = 1 << 0,
	Rotation = 1 << 1,
	Tilt = 1 << 2,
	All = Position | Rotation | Tilt
};
ENUM_CLASS_FLAGS(EGeoCameraFollowMode);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFlightComplete, bool, bSuccess, EGeoAnimationStopType, StopType);

USTRUCT(BlueprintType)
struct FGeoAnimationState
{
	GENERATED_BODY()

	UPROPERTY()
	double CurrentTime = 0.0;

	UPROPERTY()
	double Duration = 1.0;

	UPROPERTY()
	double StartValue = 0.0;

	UPROPERTY()
	double EndValue = 0.0;

	UPROPERTY()
	bool bIsPlaying = false;

	UPROPERTY()
	UCurveFloat* Curve = nullptr;

	TFunction<void(EGeoAnimationStopType)> OnComplete;

	void Start(double InStartValue, double InEndValue, double InDuration, UCurveFloat* InCurve = nullptr)
	{
		StartValue = InStartValue;
		EndValue = InEndValue;
		Duration = InDuration;
		CurrentTime = 0.0;
		Curve = InCurve;
		bIsPlaying = true;
	}

	void Stop(EGeoAnimationStopType StopType = EGeoAnimationStopType::Break)
	{
		if (bIsPlaying)
		{
			bIsPlaying = false;
			if (OnComplete)
			{
				OnComplete(StopType);
			}
		}
	}

	bool IsPlaying() const { return bIsPlaying && CurrentTime < Duration; }

	double GetT() const
	{
		if (Duration <= 0.0) return 1.0;
		return FMath::Clamp(CurrentTime / Duration, 0.0, 1.0);
	}

	double GetCurveValue() const
	{
		double T = GetT();
		if (Curve)
		{
			T = Curve->GetFloatValue(T);
		}
		return T;
	}

	double GetValue() const
	{
		double T = GetCurveValue();
		return FMath::Lerp(StartValue, EndValue, T);
	}

	void Tick(float DeltaTime)
	{
		if (!bIsPlaying) return;
		CurrentTime += DeltaTime;
		if (CurrentTime >= Duration)
		{
			CurrentTime = Duration;
			bIsPlaying = false;
			if (OnComplete)
			{
				OnComplete(EGeoAnimationStopType::Finish);
			}
		}
	}
};

USTRUCT(BlueprintType)
struct FGeoVectorAnimationState
{
	GENERATED_BODY()

	UPROPERTY()
	double CurrentTime = 0.0;

	UPROPERTY()
	double Duration = 1.0;

	UPROPERTY()
	FVector StartValue = FVector::ZeroVector;

	UPROPERTY()
	FVector EndValue = FVector::ZeroVector;

	UPROPERTY()
	bool bIsPlaying = false;

	UPROPERTY()
	UCurveFloat* Curve = nullptr;

	TFunction<void(EGeoAnimationStopType)> OnComplete;

	void Start(const FVector& InStartValue, const FVector& InEndValue, double InDuration, UCurveFloat* InCurve = nullptr)
	{
		StartValue = InStartValue;
		EndValue = InEndValue;
		Duration = InDuration;
		CurrentTime = 0.0;
		Curve = InCurve;
		bIsPlaying = true;
	}

	void Stop(EGeoAnimationStopType StopType = EGeoAnimationStopType::Break)
	{
		if (bIsPlaying)
		{
			bIsPlaying = false;
			if (OnComplete)
			{
				OnComplete(StopType);
			}
		}
	}

	bool IsPlaying() const { return bIsPlaying && CurrentTime < Duration; }

	double GetT() const
	{
		if (Duration <= 0.0) return 1.0;
		return FMath::Clamp(CurrentTime / Duration, 0.0, 1.0);
	}

	double GetCurveValue() const
	{
		double T = GetT();
		if (Curve)
		{
			T = Curve->GetFloatValue(T);
		}
		return T;
	}

	FVector GetValue() const
	{
		double T = GetCurveValue();
		return FMath::Lerp(StartValue, EndValue, T);
	}

	void Tick(float DeltaTime)
	{
		if (!bIsPlaying) return;
		CurrentTime += DeltaTime;
		if (CurrentTime >= Duration)
		{
			CurrentTime = Duration;
			bIsPlaying = false;
			if (OnComplete)
			{
				OnComplete(EGeoAnimationStopType::Finish);
			}
		}
	}
};

USTRUCT(BlueprintType)
struct FGeoQuatAnimationState
{
	GENERATED_BODY()

	UPROPERTY()
	double CurrentTime = 0.0;

	UPROPERTY()
	double Duration = 1.0;

	UPROPERTY()
	FQuat StartValue = FQuat::Identity;

	UPROPERTY()
	FQuat EndValue = FQuat::Identity;

	UPROPERTY()
	bool bIsPlaying = false;

	UPROPERTY()
	UCurveFloat* Curve = nullptr;

	TFunction<void(EGeoAnimationStopType)> OnComplete;

	void Start(const FQuat& InStartValue, const FQuat& InEndValue, double InDuration, UCurveFloat* InCurve = nullptr)
	{
		StartValue = InStartValue;
		EndValue = InEndValue;
		Duration = InDuration;
		CurrentTime = 0.0;
		Curve = InCurve;
		bIsPlaying = true;
	}

	void Stop(EGeoAnimationStopType StopType = EGeoAnimationStopType::Break)
	{
		if (bIsPlaying)
		{
			bIsPlaying = false;
			if (OnComplete)
			{
				OnComplete(StopType);
			}
		}
	}

	bool IsPlaying() const { return bIsPlaying && CurrentTime < Duration; }

	double GetT() const
	{
		if (Duration <= 0.0) return 1.0;
		return FMath::Clamp(CurrentTime / Duration, 0.0, 1.0);
	}

	double GetCurveValue() const
	{
		double T = GetT();
		if (Curve)
		{
			T = Curve->GetFloatValue(T);
		}
		return T;
	}

	FQuat GetValue() const
	{
		double T = GetCurveValue();
		return FQuat::Slerp(StartValue, EndValue, T);
	}

	void Tick(float DeltaTime)
	{
		if (!bIsPlaying) return;
		CurrentTime += DeltaTime;
		if (CurrentTime >= Duration)
		{
			CurrentTime = Duration;
			bIsPlaying = false;
			if (OnComplete)
			{
				OnComplete(EGeoAnimationStopType::Finish);
			}
		}
	}
};

UCLASS()
class GEOEARTH_API AGeoCameraController : public APawn
{
	GENERATED_BODY()

public:
	AGeoCameraController();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	UCameraComponent* Camera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cesium")
	ACesiumGeoreference* Georeference;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	EGeoCameraControlMode ControlMode = EGeoCameraControlMode::GlobalFree;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	AActor* FollowTarget = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	EGeoCameraFollowMode FollowMode = EGeoCameraFollowMode::All;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	double MinDistance = 100.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	double MaxDistance = 50000000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	double MinTiltAngle = -89.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	double MaxTiltAngle = 89.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	double PanSpeed = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	double RotationSpeed = 0.5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	double TiltSpeed = 0.4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	double ZoomSpeed = 5000000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	double DefaultFlightDuration = 2.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	UCurveFloat* FlightProgressCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bEnableMoveAnimation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bEnableZoomAnimation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	UCurveFloat* MoveAnimationCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	UCurveFloat* ZoomAnimationCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	double AdjustCenterHeight = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bEnableInertia = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	double InertiaDamping = 0.95;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	double DoubleClickFlyDuration = 2.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bEnableDoubleClickFly = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float KeyboardMoveSpeed = 1000000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bEnableKeyboardMovement = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bAdjustGeoreferenceToCenterPoint = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float ResetToInitialDuration = 2.0f;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnFlightComplete OnFlightComplete;

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetCenterPoint(const FVector& ECEFCenter, double InDistance, double InRotation, double InTilt);

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetCenterPointFromLLA(double Longitude, double Latitude, double Altitude, double InDistance, double InRotation, double InTilt);

	UFUNCTION(BlueprintCallable, Category = "Camera")
	FVector GetCenterPointECEF() const { return CenterECEF; }

	UFUNCTION(BlueprintCallable, Category = "Camera")
	FVector GetCenterPointLLA() const;

	UFUNCTION(BlueprintCallable, Category = "Camera")
	double GetDistance() const { return Distance; }

	UFUNCTION(BlueprintCallable, Category = "Camera")
	double GetAltitude() const;

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void FlyToLocation(const FVector& ECEFDestination, double TargetDistance, double TargetRotation, double TargetTilt, float Duration = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void FlyToLocationLLA(double Longitude, double Latitude, double Altitude, double TargetDistance, double TargetRotation, double TargetTilt, float Duration = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void FlyToLocationUnreal(const FVector& UnrealDestination, double TargetDistance, double TargetRotation, double TargetTilt, float Duration = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void InterruptFlight();

	UFUNCTION(BlueprintCallable, Category = "Camera")
	bool IsFlying() const;

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void ZoomTo(double TargetDistance, float Duration = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void RotateTo(double TargetRotation, float Duration = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void TiltTo(double TargetTilt, float Duration = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void CancelAnimation(EGeoAnimationType AnimationType);

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void MoveCenterForward(double Speed);

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void MoveCenterRight(double Speed);

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void Zoom(double Delta);

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void ZoomExtrapolate(double Delta, float Duration = 0.5f);

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void MoveWithScreenPos(const FVector2D& StartPos, const FVector2D& EndPos);

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void MoveWithRay(const FVector& StartRayOrigin, const FVector& StartRayDir, const FVector& EndRayOrigin, const FVector& EndRayDir);

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void FollowTo(AActor* Target, double InDistance, double InRotation, double InTilt, EGeoCameraFollowMode InFollowMode, float Duration = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "Camera")
	FVector GetCurrentCameraPositionECEF() const;

	UFUNCTION(BlueprintCallable, Category = "Camera")
	FVector GetCurrentCameraPositionLLA() const;

	UFUNCTION(BlueprintCallable, Category = "Camera")
	FRotator GetCurrentCameraRotation() const;

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void ResetToInitialPosition();

	UFUNCTION(BlueprintCallable, Category = "Camera")
	double GetRotationAngle() const { return RotationAngle; }

	UFUNCTION(BlueprintCallable, Category = "Camera")
	double GetTiltAngle() const { return TiltAngle; }

	UFUNCTION(BlueprintCallable, Category = "Camera")
	double GetMinTiltAngle() const { return MinTiltAngle; }

	UFUNCTION(BlueprintCallable, Category = "Camera")
	double GetMaxTiltAngle() const { return MaxTiltAngle; }

protected:
	UPROPERTY()
	FVector CenterECEF;

	UPROPERTY()
	double Distance;

	UPROPERTY()
	double RotationAngle;

	UPROPERTY()
	double TiltAngle;

	UFUNCTION()
	void OnLeftMouseDoubleClick();

	UFUNCTION()
	void OnResetKey();

	UFUNCTION()
	void MoveForward(float AxisValue);

	UFUNCTION()
	void MoveRight(float AxisValue);

protected:
	UPROPERTY()
	bool bInitialized;

	UPROPERTY()
	bool bIsDirty;

	UPROPERTY()
	FVector LastValidRightDir;

	UPROPERTY()
	bool bHasValidRightDir;

	UPROPERTY()
	FVector InitialCenterECEF;

	UPROPERTY()
	double InitialDistance;

	UPROPERTY()
	double InitialRotationAngle;

	UPROPERTY()
	double InitialTiltAngle;

	FVector2D LastMousePos;
	FVector2D RightDragStartMousePos;
	FVector2D MiddleDragStartMousePos;
	bool bIsDragging;
	bool bWasRightDragging;
	bool bWasMiddleDragging;

	FVector InertiaVelocity;
	double InertiaVelocityMagnitude;
	double ZoomInertiaVelocity;

	FGeoVectorAnimationState MoveAnimation;
	FGeoAnimationState ZoomAnimation;
	FGeoAnimationState RotateAnimation;
	FGeoAnimationState TiltAnimation;

	FGeoVectorAnimationState FlightMoveAnimation;
	FGeoAnimationState FlightZoomAnimation;
	FGeoAnimationState FlightRotateAnimation;
	FGeoAnimationState FlightTiltAnimation;

	FGeoVectorAnimationState FollowMoveAnimation;
	FGeoQuatAnimationState FollowRotationAnimation;
	FGeoAnimationState FollowZoomAnimation;
	FGeoAnimationState FollowRotateAnimation;
	FGeoAnimationState FollowTiltAnimation;

	FMatrix CameraECEFTransform;

	void ProcessInput();
	void UpdateCamera();
	void FlushCamera();
	void UpdateAnimations();
	void UpdateInertia(float DeltaTime);

	void OnMouseWheel(float AxisValue);
	void OnLeftMousePressed();
	void OnLeftMouseReleased();
	void OnRightMousePressed();
	void OnRightMouseReleased();
	void OnMiddleMousePressed();
	void OnMiddleMouseReleased();

	FVector UnityToEcefPos(const FVector& UnityPos) const;
	FVector EcefToUnityPos(const FVector& EcefPos) const;
	FVector UnityToEcefDir(const FVector& UnityDir) const;
	FVector EcefToUnityDir(const FVector& EcefDir) const;
	FVector EcefToLLA(const FVector& EcefPos) const;
	FVector LLAToEcef(const FVector& LLA) const;
	FVector LLAToEcef(double Longitude, double Latitude, double Altitude) const;

	bool RayEllipsoid(const FVector& Origin, const FVector& Dir, double& OutStart, double& OutStop, double AdjustHeight = 0.0) const;
	bool ComputePickRay(const FVector2D& ScreenPos, FVector& OutOrigin, FVector& OutDir) const;

	FVector ComputeGeodeticSurfaceNormal(const FVector& EcefPos) const;

	void AutoFixGroundDistance();
	void UpdateFollowTargetPosition();

	bool CheckCanFollow() const;
	FMatrix MakeTransform();
	FMatrix MakeFollowTransform();
	FMatrix MakeTransformInternal(const FVector& CenterPos, const FVector& UpDir, const FVector& RightDir, double Rotate, double Tilt, double InDistance);

	void ApplyMoveAnimation(FGeoVectorAnimationState& Anim);
	void ApplyDoubleAnimation(FGeoAnimationState& Anim, double& TargetValue);
	void ApplyQuatAnimation(FGeoQuatAnimationState& Anim);
	void MoveCenterByDirection(double Speed, bool bForward);
	bool PickEllipsoidAtScreenPos(const FVector2D& ScreenPos, FVector& OutHitPoint);
	bool PickTangentPlaneAtScreenPos(const FVector2D& ScreenPos, FVector& OutHitPoint);
	void MoveCenterByHitDiff(const FVector& P0, const FVector& P1, bool bAnimate = false);
	void BroadcastFlightComplete(bool bSuccess, EGeoAnimationStopType StopType);

private:
	static constexpr double WGS84_SEMI_MAJOR_AXIS = 6378137.0;
	static constexpr double WGS84_SEMI_MINOR_AXIS = 6356752.3142451793;
	static constexpr double MAX_LATITUDE = 85.0;
	static constexpr double EPSILON = 0.0001;
};