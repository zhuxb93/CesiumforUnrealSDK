#include "GeoCameraController.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "CesiumRuntime/Public/CesiumGeoreference.h"

AGeoCameraController::AGeoCameraController()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(RootComponent);

	AutoPossessPlayer = EAutoReceiveInput::Player0;

	bInitialized = false;
	bIsDirty = true;
	bIsDragging = false;
	bWasRightDragging = false;
	bWasMiddleDragging = false;
	bHasValidRightDir = false;
	Distance = 10000000.0;
	RotationAngle = 0.0;
	TiltAngle = 0.0;
	CenterECEF = FVector(0, 0, WGS84_SEMI_MAJOR_AXIS);
	InertiaVelocity = FVector::ZeroVector;
	InertiaVelocityMagnitude = 0.0;
	ZoomInertiaVelocity = 0.0;
	CameraECEFTransform = FMatrix::Identity;
}

void AGeoCameraController::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC)
	{
		PC->Possess(this);
		PC->bShowMouseCursor = true;
		PC->bEnableClickEvents = true;
		PC->bEnableMouseOverEvents = true;
		PC->SetViewTarget(this);
	}

	if (!Georeference)
	{
		Georeference = ACesiumGeoreference::GetDefaultGeoreference(this);
	}

	if (!Georeference)
	{
		UE_LOG(LogTemp, Error, TEXT("CesiumGeoreference not found!"));
		return;
	}

	if (bAdjustGeoreferenceToCenterPoint)
	{
		Georeference->SetOriginEarthCenteredEarthFixed(CenterECEF);
	}

	if (Camera)
	{
		FVector StartLocation = Camera->GetComponentLocation();
		FVector StartLLH = Georeference->TransformUnrealPositionToLongitudeLatitudeHeight(StartLocation);

		if (FMath::IsNaN(StartLLH.X) || FMath::IsNaN(StartLLH.Y) || FMath::IsNaN(StartLLH.Z) ||
			FMath::Abs(StartLLH.Y) > 90.0 || StartLLH.Z < 0.0)
		{
			StartLLH = FVector(116.4, 39.9, 10000.0);
		}

		CenterECEF = LLAToEcef(StartLLH.X, StartLLH.Y, 0.0);
		Distance = StartLLH.Z;
		if (Distance < MinDistance) Distance = MinDistance;
		if (Distance > MaxDistance) Distance = MaxDistance;

		RotationAngle = 0.0;
		TiltAngle = FMath::Clamp(0.0, MinTiltAngle, MaxTiltAngle);

		InitialCenterECEF = CenterECEF;
		InitialDistance = Distance;
		InitialRotationAngle = RotationAngle;
		InitialTiltAngle = TiltAngle;

		bInitialized = true;
		bIsDirty = true;
		FlushCamera();
	}
}

void AGeoCameraController::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	PlayerInputComponent->BindAxis(TEXT("MouseWheelAxis"), this, &AGeoCameraController::OnMouseWheel);
	PlayerInputComponent->BindAction(TEXT("LeftMouseButton"), IE_Pressed, this, &AGeoCameraController::OnLeftMousePressed);
	PlayerInputComponent->BindAction(TEXT("LeftMouseButton"), IE_Released, this, &AGeoCameraController::OnLeftMouseReleased);
	PlayerInputComponent->BindAction(TEXT("LeftMouseButton"), IE_DoubleClick, this, &AGeoCameraController::OnLeftMouseDoubleClick);
	PlayerInputComponent->BindAction(TEXT("RightMouseButton"), IE_Pressed, this, &AGeoCameraController::OnRightMousePressed);
	PlayerInputComponent->BindAction(TEXT("RightMouseButton"), IE_Released, this, &AGeoCameraController::OnRightMouseReleased);
	PlayerInputComponent->BindAction(TEXT("MiddleMouseButton"), IE_Pressed, this, &AGeoCameraController::OnMiddleMousePressed);
	PlayerInputComponent->BindAction(TEXT("MiddleMouseButton"), IE_Released, this, &AGeoCameraController::OnMiddleMouseReleased);

	if (bEnableKeyboardMovement)
	{
		PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &AGeoCameraController::MoveForward);
		PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &AGeoCameraController::MoveRight);
	}

	PlayerInputComponent->BindAction(TEXT("ResetKey"), IE_Pressed, this, &AGeoCameraController::OnResetKey);
}

void AGeoCameraController::MoveForward(float AxisValue)
{
	if (FMath::IsNearlyZero(AxisValue)) return;

	double SpeedScale = FMath::Clamp(Distance / WGS84_SEMI_MAJOR_AXIS, 0.001, 10.0);
	double MoveSpeed = KeyboardMoveSpeed * SpeedScale * FMath::Abs(AxisValue) * 0.1;

	MoveCenterForward(-MoveSpeed * FMath::Sign(AxisValue));
}

void AGeoCameraController::MoveRight(float AxisValue)
{
	if (FMath::IsNearlyZero(AxisValue)) return;

	double SpeedScale = FMath::Clamp(Distance / WGS84_SEMI_MAJOR_AXIS, 0.001, 10.0);
	double MoveSpeed = KeyboardMoveSpeed * SpeedScale * FMath::Abs(AxisValue) * 0.1;

	MoveCenterRight(MoveSpeed * FMath::Sign(AxisValue));
}

void AGeoCameraController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bInitialized || !Georeference || !Camera) return;

	ProcessInput();
	UpdateInertia(DeltaTime);
	FlushCamera();
	UpdateAnimations();
	UpdateFollowTargetPosition();
}

void AGeoCameraController::ProcessInput()
{
	if (ControlMode != EGeoCameraControlMode::GlobalFree) return;

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;

	float MouseX, MouseY;
	PC->GetMousePosition(MouseX, MouseY);
	FVector2D CurrentMousePos(MouseX, MouseY);

	bool bRightDown = PC->IsInputKeyDown(EKeys::RightMouseButton);
	bool bLeftDown = PC->IsInputKeyDown(EKeys::LeftMouseButton);
	bool bMiddleDown = PC->IsInputKeyDown(EKeys::MiddleMouseButton);

	if (bIsDragging && bLeftDown)
	{
		FVector2D Delta = CurrentMousePos - LastMousePos;
		if (!Delta.IsNearlyZero(1.0f))
		{
			FVector SurfaceNormal = ComputeGeodeticSurfaceNormal(CenterECEF);
			SurfaceNormal.Normalize();

			// Use camera orientation for movement direction (stable at any tilt angle)
			FVector CamForward = CameraECEFTransform.GetUnitAxis(EAxis::Z);
			FVector CamRight = CameraECEFTransform.GetUnitAxis(EAxis::X);

			// Project camera directions onto the geodetic surface tangent plane
			FVector ForwardOnSurface = CamForward - FVector::DotProduct(CamForward, SurfaceNormal) * SurfaceNormal;
			FVector RightOnSurface = CamRight - FVector::DotProduct(CamRight, SurfaceNormal) * SurfaceNormal;

			double ForwardLen = ForwardOnSurface.Size();
			double RightLen = RightOnSurface.Size();

			if (ForwardLen > EPSILON && RightLen > EPSILON)
			{
				ForwardOnSurface /= ForwardLen;
				RightOnSurface /= RightLen;

				// Screen X delta → right movement, Screen Y delta → forward movement
				FVector MoveDir = RightOnSurface * (-Delta.X) + ForwardOnSurface * Delta.Y;
				double PixelDistance = Delta.Size();

				double LogDistance = FMath::Log2(FMath::Max(Distance, 100.0)) / FMath::Log2(10.0);
				double LogMin = 3.0;
				double LogMax = FMath::Log2(WGS84_SEMI_MAJOR_AXIS) / FMath::Log2(10.0);
				double T = FMath::Clamp((LogDistance - LogMin) / (LogMax - LogMin), 0.0, 1.0);
				double T_ease = FMath::Sqrt(T);
				double SpeedScale = (0.2 - 0.18 * T_ease) * PanSpeed;

				double ScaledMoveDistance = PixelDistance * SpeedScale * Distance * 0.002;
				ScaledMoveDistance = FMath::Min(ScaledMoveDistance, Distance * 0.1);

				MoveDir.Normalize();
				FVector NewCenter = CenterECEF + MoveDir * ScaledMoveDistance;
				FVector NewLLA = EcefToLLA(NewCenter);
				NewLLA.Y = FMath::Clamp(NewLLA.Y, -MAX_LATITUDE, MAX_LATITUDE);
				NewLLA.Z = 0.0;
				CenterECEF = LLAToEcef(NewLLA);
				bIsDirty = true;

				double InertiaScale = FMath::Max(1.0, FMath::Log2(Distance / 1000.0 + 1.0));
				InertiaVelocity = MoveDir * ScaledMoveDistance;
				InertiaVelocityMagnitude = ScaledMoveDistance * InertiaScale;
			}

			LastMousePos = CurrentMousePos;
		}
	}
	else if (bIsDragging && !bLeftDown)
	{
		bIsDragging = false;
	}

	if (bRightDown && !bWasRightDragging)
	{
		RightDragStartMousePos = CurrentMousePos;
		bWasRightDragging = true;
		return;
	}

	if (!bRightDown && bWasRightDragging)
	{
		bWasRightDragging = false;
	}

	if (bRightDown && bWasRightDragging)
	{
		FVector2D Delta = CurrentMousePos - RightDragStartMousePos;

		double ZoomRatio = (ZoomSpeed / 5000000.0) * 0.01;
		double AdjustedZoomSpeed = Distance * ZoomRatio * (-Delta.Y);
		Distance = FMath::Clamp(Distance + AdjustedZoomSpeed, MinDistance, MaxDistance);

		if (FMath::Abs(AdjustedZoomSpeed) > 0.1)
		{
			ZoomInertiaVelocity = AdjustedZoomSpeed;
		}

		bIsDirty = true;
		RightDragStartMousePos = CurrentMousePos;
	}

	if (bMiddleDown && !bWasMiddleDragging)
	{
		MiddleDragStartMousePos = CurrentMousePos;
		bWasMiddleDragging = true;
		return;
	}

	if (!bMiddleDown && bWasMiddleDragging)
	{
		bWasMiddleDragging = false;
	}

	if (bMiddleDown && bWasMiddleDragging)
	{
		FVector2D Delta = CurrentMousePos - MiddleDragStartMousePos;

		RotationAngle += Delta.X * RotationSpeed;
		TiltAngle -= Delta.Y * TiltSpeed;
		TiltAngle = FMath::Clamp(TiltAngle, MinTiltAngle, MaxTiltAngle);

		bIsDirty = true;
		MiddleDragStartMousePos = CurrentMousePos;
	}
}

void AGeoCameraController::OnLeftMouseDoubleClick()
{
	if (!bInitialized || !bEnableDoubleClickFly) return;

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;

	float MouseX, MouseY;
	PC->GetMousePosition(MouseX, MouseY);
	FVector2D MousePos(MouseX, MouseY);

	FVector HitPoint;
	if (PickEllipsoidAtScreenPos(MousePos, HitPoint))
	{
		double TargetDistance = FMath::Max(Distance * 0.5, MinDistance);
		FlyToLocation(HitPoint, TargetDistance, RotationAngle, TiltAngle, DoubleClickFlyDuration);
	}
}

void AGeoCameraController::UpdateInertia(float DeltaTime)
{
	if (!bEnableInertia) return;

	if (InertiaVelocityMagnitude > 0.01)
	{
		InertiaVelocityMagnitude *= FMath::Pow(InertiaDamping, DeltaTime * 60.0f);

		if (InertiaVelocityMagnitude <= 0.01)
		{
			InertiaVelocity = FVector::ZeroVector;
			InertiaVelocityMagnitude = 0.0;
		}
		else
		{
			FVector MoveDir = InertiaVelocity.GetSafeNormal();
			FVector NewCenter = CenterECEF + MoveDir * InertiaVelocityMagnitude;
			FVector NewLLA = EcefToLLA(NewCenter);
			NewLLA.Y = FMath::Clamp(NewLLA.Y, -MAX_LATITUDE, MAX_LATITUDE);
			NewLLA.Z = 0.0;
			CenterECEF = LLAToEcef(NewLLA);
			bIsDirty = true;
		}
	}

	if (FMath::Abs(ZoomInertiaVelocity) > 0.1)
	{
		ZoomInertiaVelocity *= FMath::Pow(InertiaDamping, DeltaTime * 60.0f);

		if (FMath::Abs(ZoomInertiaVelocity) <= 0.1)
		{
			ZoomInertiaVelocity = 0.0;
		}
		else
		{
			Distance = FMath::Clamp(Distance + ZoomInertiaVelocity, MinDistance, MaxDistance);
			bIsDirty = true;
		}
	}
}

void AGeoCameraController::UpdateCamera()
{
	if (!Camera || !Georeference) return;

	FMatrix Transform = FMatrix::Identity;

	if (ControlMode == EGeoCameraControlMode::GlobalFree)
	{
		Transform = MakeTransform();
	}
	else if (CheckCanFollow())
	{
		Transform = MakeFollowTransform();
	}

	FVector CameraWorldPos = Transform.GetOrigin();
	FVector WorldForward(Transform.M[0][0], Transform.M[0][1], Transform.M[0][2]);
	FVector WorldRight(Transform.M[1][0], Transform.M[1][1], Transform.M[1][2]);
	FVector WorldUp(Transform.M[2][0], Transform.M[2][1], Transform.M[2][2]);

	WorldForward.Normalize();
	WorldRight.Normalize();
	WorldUp.Normalize();

	FMatrix RotMatrix(
		FPlane(WorldForward.X, WorldForward.Y, WorldForward.Z, 0.f),
		FPlane(WorldRight.X, WorldRight.Y, WorldRight.Z, 0.f),
		FPlane(WorldUp.X, WorldUp.Y, WorldUp.Z, 0.f),
		FPlane(0.f, 0.f, 0.f, 1.f)
	);

	FQuat WorldQuat = RotMatrix.ToQuat();

	Camera->SetWorldLocation(CameraWorldPos);
	Camera->SetWorldRotation(WorldQuat);
}

FMatrix AGeoCameraController::MakeTransform()
{
	return MakeTransformInternal(CenterECEF, FVector::ZeroVector, FVector::ZeroVector, RotationAngle, TiltAngle, Distance);
}

FMatrix AGeoCameraController::MakeTransformInternal(const FVector& CenterPos, const FVector& UpDir, const FVector& RightDir, double Rotate, double Tilt, double InDistance)
{
	if (CenterPos.IsNearlyZero(EPSILON))
	{
		return FMatrix::Identity;
	}

	FVector CenterUp = ComputeGeodeticSurfaceNormal(CenterPos);
	CenterUp.Normalize();

	FVector CenterRight = FVector::CrossProduct(FVector(0, 0, 1), CenterUp);
	double CrossLength = CenterRight.Size();

	const double PoleThreshold = 0.01;
	if (CrossLength < PoleThreshold)
	{
		if (bHasValidRightDir)
		{
			FVector ProjectedRight = LastValidRightDir - FVector::DotProduct(LastValidRightDir, CenterUp) * CenterUp;
			if (ProjectedRight.Size() > EPSILON)
			{
				ProjectedRight.Normalize();
				CenterRight = ProjectedRight;
			}
			else
			{
				CenterRight = (CenterUp.Z > 0) ? FVector(1, 0, 0) : FVector(-1, 0, 0);
			}
		}
		else
		{
			CenterRight = (CenterUp.Z > 0) ? FVector(1, 0, 0) : FVector(-1, 0, 0);
		}
	}
	else
	{
		CenterRight.Normalize();
		LastValidRightDir = CenterRight;
		bHasValidRightDir = true;
	}

	FQuat YawQuat(CenterUp, FMath::DegreesToRadians(-Rotate));
	FQuat PitchQuat(CenterRight, FMath::DegreesToRadians(Tilt));
	FQuat LocalCameraRotation = YawQuat * PitchQuat;

	FVector CameraDir = LocalCameraRotation.RotateVector(CenterUp);
	CameraDir.Normalize();

	FVector CameraPosECEF = CenterPos + CameraDir * InDistance;

	FVector CameraForward = -CameraDir;
	CameraForward.Normalize();

	FVector CameraRight = FVector::CrossProduct(CameraForward, CenterUp);
	if (CameraRight.Size() < EPSILON)
	{
		CameraRight = LocalCameraRotation.RotateVector(CenterRight);
	}
	CameraRight.Normalize();

	FVector CameraUp = FVector::CrossProduct(CameraRight, CameraForward);
	CameraUp.Normalize();

	CameraECEFTransform.SetIdentity();
	CameraECEFTransform.SetAxis(0, CameraRight);
	CameraECEFTransform.SetAxis(1, CameraUp);
	CameraECEFTransform.SetAxis(2, CameraForward);
	CameraECEFTransform.SetOrigin(CameraPosECEF);

	FVector WorldPos = EcefToUnityPos(CameraPosECEF);
	FVector WorldForward = EcefToUnityDir(CameraForward);
	FVector WorldRight = EcefToUnityDir(CameraRight);
	FVector WorldUp = EcefToUnityDir(CameraUp);

	WorldForward.Normalize();
	WorldRight.Normalize();
	WorldUp.Normalize();

	FMatrix Result(FPlane(WorldForward.X, WorldForward.Y, WorldForward.Z, 0.f),
		FPlane(WorldRight.X, WorldRight.Y, WorldRight.Z, 0.f),
		FPlane(WorldUp.X, WorldUp.Y, WorldUp.Z, 0.f),
		FPlane(WorldPos.X, WorldPos.Y, WorldPos.Z, 1.f));

	return Result;
}

FMatrix AGeoCameraController::MakeFollowTransform()
{
	if (!FollowTarget || !Georeference) return FMatrix::Identity;

	FVector TargetPos = FollowTarget->GetActorLocation();
	FVector TargetECEF = UnityToEcefPos(TargetPos);

	FVector CenterUp = FVector::ZeroVector;
	FVector CenterRight = FVector::ZeroVector;

	if ((FollowMode & EGeoCameraFollowMode::Rotation) != EGeoCameraFollowMode::None)
	{
		FRotator TargetRotator = FollowTarget->GetActorRotation();
		FVector WorldRight = TargetRotator.RotateVector(FVector::RightVector);
		CenterRight = UnityToEcefDir(WorldRight);
		CenterRight.Normalize();
	}

	if ((FollowMode & EGeoCameraFollowMode::Tilt) != EGeoCameraFollowMode::None)
	{
		FRotator TargetRotator = FollowTarget->GetActorRotation();
		FVector WorldUp = TargetRotator.RotateVector(FVector::UpVector);
		CenterUp = UnityToEcefDir(WorldUp);
		CenterUp.Normalize();
	}

	return MakeTransformInternal(TargetECEF, CenterUp, CenterRight, RotationAngle, TiltAngle, Distance);
}

void AGeoCameraController::FlushCamera()
{
	if (!bIsDirty) return;

	AutoFixGroundDistance();
	UpdateCamera();
	bIsDirty = false;
}

void AGeoCameraController::ApplyMoveAnimation(FGeoVectorAnimationState& Anim)
{
	if (!Anim.IsPlaying()) return;

	float DeltaTime = GetWorld()->GetDeltaSeconds();
	Anim.Tick(DeltaTime);
	FVector NewCenterLLA = Anim.GetValue();
	NewCenterLLA.Y = FMath::Clamp(NewCenterLLA.Y, -MAX_LATITUDE, MAX_LATITUDE);
	CenterECEF = LLAToEcef(NewCenterLLA);
	bIsDirty = true;
}

void AGeoCameraController::ApplyDoubleAnimation(FGeoAnimationState& Anim, double& TargetValue)
{
	if (!Anim.IsPlaying()) return;

	float DeltaTime = GetWorld()->GetDeltaSeconds();
	Anim.Tick(DeltaTime);
	TargetValue = Anim.GetValue();
	bIsDirty = true;
}

void AGeoCameraController::ApplyQuatAnimation(FGeoQuatAnimationState& Anim)
{
	if (!Anim.IsPlaying()) return;

	float DeltaTime = GetWorld()->GetDeltaSeconds();
	Anim.Tick(DeltaTime);
	FQuat NewRotation = Anim.GetValue();
	RotationAngle = NewRotation.Rotator().Yaw;
	bIsDirty = true;
}

void AGeoCameraController::UpdateAnimations()
{
	ApplyMoveAnimation(MoveAnimation);
	ApplyDoubleAnimation(ZoomAnimation, Distance);
	ApplyDoubleAnimation(RotateAnimation, RotationAngle);
	ApplyDoubleAnimation(TiltAnimation, TiltAngle);

	ApplyMoveAnimation(FlightMoveAnimation);
	ApplyDoubleAnimation(FlightZoomAnimation, Distance);
	ApplyDoubleAnimation(FlightRotateAnimation, RotationAngle);
	ApplyDoubleAnimation(FlightTiltAnimation, TiltAngle);

	ApplyMoveAnimation(FollowMoveAnimation);
	ApplyQuatAnimation(FollowRotationAnimation);
	ApplyDoubleAnimation(FollowZoomAnimation, Distance);
	ApplyDoubleAnimation(FollowRotateAnimation, RotationAngle);
	ApplyDoubleAnimation(FollowTiltAnimation, TiltAngle);
}

void AGeoCameraController::UpdateFollowTargetPosition()
{
	if (!CheckCanFollow()) return;

	FVector TargetPos = FollowTarget->GetActorLocation();
	FVector TargetECEF = UnityToEcefPos(TargetPos);
	CenterECEF = TargetECEF;
	bIsDirty = true;
}

bool AGeoCameraController::CheckCanFollow() const
{
	if (ControlMode != EGeoCameraControlMode::FollowActor) return false;
	if (!FollowTarget)
	{
		return false;
	}
	return true;
}

void AGeoCameraController::OnMouseWheel(float AxisValue)
{
	if (!bInitialized || FMath::IsNearlyZero(AxisValue)) return;

	double ZoomRatio = (ZoomSpeed / 5000000.0) * 0.1;
	double AdjustedZoomSpeed = Distance * ZoomRatio * FMath::Abs(AxisValue);

	if (AxisValue > 0)
	{
		Distance = FMath::Max(Distance - AdjustedZoomSpeed, MinDistance);
		ZoomInertiaVelocity = -AdjustedZoomSpeed;
	}
	else
	{
		Distance = FMath::Min(Distance + AdjustedZoomSpeed, MaxDistance);
		ZoomInertiaVelocity = AdjustedZoomSpeed;
	}

	bIsDirty = true;
}

void AGeoCameraController::OnLeftMousePressed()
{
	if (!bInitialized || !Camera) return;

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC)
	{
		float MouseX, MouseY;
		PC->GetMousePosition(MouseX, MouseY);
		LastMousePos = FVector2D(MouseX, MouseY);
		bIsDragging = true;
	}
}

void AGeoCameraController::OnLeftMouseReleased()
{
	bIsDragging = false;
}

void AGeoCameraController::OnRightMousePressed()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC)
	{
		float MouseX, MouseY;
		PC->GetMousePosition(MouseX, MouseY);
		RightDragStartMousePos = FVector2D(MouseX, MouseY);
		bWasRightDragging = true;
	}
}

void AGeoCameraController::OnRightMouseReleased()
{
	bWasRightDragging = false;
}

void AGeoCameraController::OnMiddleMousePressed()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC)
	{
		float MouseX, MouseY;
		PC->GetMousePosition(MouseX, MouseY);
		MiddleDragStartMousePos = FVector2D(MouseX, MouseY);
		bWasMiddleDragging = true;
	}
}

void AGeoCameraController::OnMiddleMouseReleased()
{
	bWasMiddleDragging = false;
}

void AGeoCameraController::SetCenterPoint(const FVector& ECEFCenter, double InDistance, double InRotation, double InTilt)
{
	CenterECEF = ECEFCenter;
	Distance = FMath::Clamp(InDistance, MinDistance, MaxDistance);
	RotationAngle = InRotation;
	TiltAngle = FMath::Clamp(InTilt, MinTiltAngle, MaxTiltAngle);
	bIsDirty = true;
}

void AGeoCameraController::SetCenterPointFromLLA(double Longitude, double Latitude, double Altitude, double InDistance, double InRotation, double InTilt)
{
	CenterECEF = LLAToEcef(Longitude, Latitude, Altitude);
	Distance = FMath::Clamp(InDistance, MinDistance, MaxDistance);
	RotationAngle = InRotation;
	TiltAngle = FMath::Clamp(InTilt, MinTiltAngle, MaxTiltAngle);
	bIsDirty = true;
}

FVector AGeoCameraController::GetCenterPointLLA() const
{
	return EcefToLLA(CenterECEF);
}

double AGeoCameraController::GetAltitude() const
{
	FVector LLH = EcefToLLA(CenterECEF);
	return LLH.Z;
}

void AGeoCameraController::BroadcastFlightComplete(bool bSuccess, EGeoAnimationStopType StopType)
{
	if (OnFlightComplete.IsBound())
	{
		OnFlightComplete.Broadcast(bSuccess, StopType);
	}
}

void AGeoCameraController::FlyToLocation(const FVector& ECEFDestination, double TargetDistance, double TargetRotation, double TargetTilt, float Duration)
{
	float ActualDuration = Duration < 0.0f ? DefaultFlightDuration : Duration;

	InterruptFlight();

	ControlMode = EGeoCameraControlMode::GlobalFree;
	FollowTarget = nullptr;

	FVector StartLLA = EcefToLLA(CenterECEF);
	FVector EndLLA = EcefToLLA(ECEFDestination);

	FlightMoveAnimation.Start(StartLLA, EndLLA, ActualDuration, FlightProgressCurve);
	FlightZoomAnimation.Start(Distance, FMath::Clamp(TargetDistance, MinDistance, MaxDistance), ActualDuration, FlightProgressCurve);
	FlightRotateAnimation.Start(RotationAngle, TargetRotation, ActualDuration, FlightProgressCurve);
	FlightTiltAnimation.Start(TiltAngle, FMath::Clamp(TargetTilt, MinTiltAngle, MaxTiltAngle), ActualDuration, FlightProgressCurve);

	FlightMoveAnimation.OnComplete = [this](EGeoAnimationStopType StopType)
	{
		BroadcastFlightComplete(StopType == EGeoAnimationStopType::Finish, StopType);
	};

	bIsDirty = true;
}

void AGeoCameraController::FlyToLocationLLA(double Longitude, double Latitude, double Altitude, double TargetDistance, double TargetRotation, double TargetTilt, float Duration)
{
	FVector ECEFDest = LLAToEcef(Longitude, Latitude, Altitude);
	FlyToLocation(ECEFDest, TargetDistance, TargetRotation, TargetTilt, Duration);
}

void AGeoCameraController::FlyToLocationUnreal(const FVector& UnrealDestination, double TargetDistance, double TargetRotation, double TargetTilt, float Duration)
{
	FVector ECEFDest = UnityToEcefPos(UnrealDestination);
	FlyToLocation(ECEFDest, TargetDistance, TargetRotation, TargetTilt, Duration);
}

void AGeoCameraController::FollowTo(AActor* Target, double InDistance, double InRotation, double InTilt, EGeoCameraFollowMode InFollowMode, float Duration)
{
	if (!Target) return;

	float ActualDuration = Duration < 0.0f ? DefaultFlightDuration : Duration;

	ControlMode = EGeoCameraControlMode::FollowActor;
	FollowTarget = Target;
	FollowMode = InFollowMode;

	FVector TargetPos = Target->GetActorLocation();
	FVector TargetECEF = UnityToEcefPos(TargetPos);

	FVector StartLLA = EcefToLLA(CenterECEF);
	FVector EndLLA = EcefToLLA(TargetECEF);

	if ((InFollowMode & EGeoCameraFollowMode::Position) != EGeoCameraFollowMode::None)
	{
		FollowMoveAnimation.Start(StartLLA, EndLLA, ActualDuration, FlightProgressCurve);
	}

	if ((InFollowMode & EGeoCameraFollowMode::Rotation) != EGeoCameraFollowMode::None)
	{
		FRotator TargetRotator = Target->GetActorRotation();
		FRotator CurrentRotator(0.0, RotationAngle, 0.0);
		FollowRotationAnimation.Start(CurrentRotator.Quaternion(), TargetRotator.Quaternion(), ActualDuration, FlightProgressCurve);
	}

	FollowZoomAnimation.Start(Distance, FMath::Clamp(InDistance, MinDistance, MaxDistance), ActualDuration, FlightProgressCurve);
	FollowRotateAnimation.Start(RotationAngle, InRotation, ActualDuration, FlightProgressCurve);
	FollowTiltAnimation.Start(TiltAngle, FMath::Clamp(InTilt, MinTiltAngle, MaxTiltAngle), ActualDuration, FlightProgressCurve);

	FollowMoveAnimation.OnComplete = [this](EGeoAnimationStopType StopType)
	{
		BroadcastFlightComplete(StopType == EGeoAnimationStopType::Finish, StopType);
	};

	bIsDirty = true;
}

void AGeoCameraController::InterruptFlight()
{
	FlightMoveAnimation.Stop(EGeoAnimationStopType::Break);
	FlightZoomAnimation.Stop(EGeoAnimationStopType::Break);
	FlightRotateAnimation.Stop(EGeoAnimationStopType::Break);
	FlightTiltAnimation.Stop(EGeoAnimationStopType::Break);

	FollowMoveAnimation.Stop(EGeoAnimationStopType::Break);
	FollowRotationAnimation.Stop(EGeoAnimationStopType::Break);
	FollowZoomAnimation.Stop(EGeoAnimationStopType::Break);
	FollowRotateAnimation.Stop(EGeoAnimationStopType::Break);
	FollowTiltAnimation.Stop(EGeoAnimationStopType::Break);
}

bool AGeoCameraController::IsFlying() const
{
	return FlightMoveAnimation.IsPlaying() || FlightZoomAnimation.IsPlaying() ||
		FlightRotateAnimation.IsPlaying() || FlightTiltAnimation.IsPlaying() ||
		FollowMoveAnimation.IsPlaying() || FollowZoomAnimation.IsPlaying() ||
		FollowRotateAnimation.IsPlaying() || FollowTiltAnimation.IsPlaying();
}

void AGeoCameraController::ZoomTo(double TargetDistance, float Duration)
{
	float ActualDuration = Duration < 0.0f ? DefaultFlightDuration : Duration;
	ZoomAnimation.Start(Distance, FMath::Clamp(TargetDistance, MinDistance, MaxDistance), ActualDuration, ZoomAnimationCurve);
}

void AGeoCameraController::RotateTo(double TargetRotation, float Duration)
{
	float ActualDuration = Duration < 0.0f ? DefaultFlightDuration : Duration;
	RotateAnimation.Start(RotationAngle, TargetRotation, ActualDuration, MoveAnimationCurve);
}

void AGeoCameraController::TiltTo(double TargetTilt, float Duration)
{
	float ActualDuration = Duration < 0.0f ? DefaultFlightDuration : Duration;
	TiltAnimation.Start(TiltAngle, FMath::Clamp(TargetTilt, MinTiltAngle, MaxTiltAngle), ActualDuration, MoveAnimationCurve);
}

void AGeoCameraController::CancelAnimation(EGeoAnimationType AnimationType)
{
	if ((AnimationType & EGeoAnimationType::Move) != EGeoAnimationType::None)
	{
		MoveAnimation.Stop(EGeoAnimationStopType::Break);
		FlightMoveAnimation.Stop(EGeoAnimationStopType::Break);
		FollowMoveAnimation.Stop(EGeoAnimationStopType::Break);
	}
	if ((AnimationType & EGeoAnimationType::Zoom) != EGeoAnimationType::None)
	{
		ZoomAnimation.Stop(EGeoAnimationStopType::Break);
		FlightZoomAnimation.Stop(EGeoAnimationStopType::Break);
		FollowZoomAnimation.Stop(EGeoAnimationStopType::Break);
	}
	if ((AnimationType & EGeoAnimationType::Rotate) != EGeoAnimationType::None)
	{
		RotateAnimation.Stop(EGeoAnimationStopType::Break);
		FlightRotateAnimation.Stop(EGeoAnimationStopType::Break);
		FollowRotateAnimation.Stop(EGeoAnimationStopType::Break);
		FollowRotationAnimation.Stop(EGeoAnimationStopType::Break);
	}
	if ((AnimationType & EGeoAnimationType::Tilt) != EGeoAnimationType::None)
	{
		TiltAnimation.Stop(EGeoAnimationStopType::Break);
		FlightTiltAnimation.Stop(EGeoAnimationStopType::Break);
		FollowTiltAnimation.Stop(EGeoAnimationStopType::Break);
	}
}

void AGeoCameraController::MoveCenterByDirection(double Speed, bool bForward)
{
	FVector Center = CenterECEF;
	if (Center.IsNearlyZero(EPSILON)) return;

	FVector CenterUp = ComputeGeodeticSurfaceNormal(Center);
	CenterUp.Normalize();

	FVector CenterRight = FVector::CrossProduct(FVector(0, 0, 1), CenterUp);
	double CrossLength = CenterRight.Size();

	const double PoleThreshold = 0.01;
	if (CrossLength < PoleThreshold)
	{
		if (bHasValidRightDir)
		{
			CenterRight = LastValidRightDir;
		}
		else
		{
			CenterRight = (CenterUp.Z > 0) ? FVector(1, 0, 0) : FVector(-1, 0, 0);
		}
	}
	CenterRight.Normalize();

	FVector CenterForward = FVector::CrossProduct(CenterRight, CenterUp);
	CenterForward.Normalize();

	// Apply rotation around Up
	FQuat RotateTransform = FQuat(CenterUp, FMath::DegreesToRadians(-RotationAngle));
	FVector RotatedForward = RotateTransform.RotateVector(CenterForward);
	FVector RotatedRight = RotateTransform.RotateVector(CenterRight);

	FVector MoveDir;
	if (bForward)
	{
		// Project the tilted camera view direction onto the geodetic surface plane
		// so that forward movement follows the camera's line of sight along the ground
		FQuat TiltQuat(RotatedRight, FMath::DegreesToRadians(TiltAngle));
		FVector TiltedDir = TiltQuat.RotateVector(RotatedForward);

		// Project onto surface tangent plane
		FVector ProjectedDir = TiltedDir - FVector::DotProduct(TiltedDir, CenterUp) * CenterUp;
		if (ProjectedDir.SizeSquared() < EPSILON)
		{
			ProjectedDir = RotatedForward;
		}
		ProjectedDir.Normalize();
		MoveDir = ProjectedDir;
	}
	else
	{
		MoveDir = RotatedRight;
	}
	MoveDir.Normalize();

	FVector NewCenter = CenterECEF + MoveDir * Speed;

	FVector NewLLA = EcefToLLA(NewCenter);
	NewLLA.Y = FMath::Clamp(NewLLA.Y, -MAX_LATITUDE, MAX_LATITUDE);
	NewLLA.Z = 0.0;
	CenterECEF = LLAToEcef(NewLLA);
	bIsDirty = true;
}

void AGeoCameraController::MoveCenterForward(double Speed)
{
	MoveCenterByDirection(Speed, true);
}

void AGeoCameraController::MoveCenterRight(double Speed)
{
	MoveCenterByDirection(Speed, false);
}

void AGeoCameraController::Zoom(double Delta)
{
	double ZoomRatio = 0.1;
	Distance -= Distance * ZoomRatio * Delta;
	Distance = FMath::Clamp(Distance, MinDistance, MaxDistance);
	bIsDirty = true;
}

void AGeoCameraController::ZoomExtrapolate(double Delta, float Duration)
{
	if (bEnableZoomAnimation)
	{
		ZoomAnimation.Start(0.0, Delta, Duration, ZoomAnimationCurve);
		ZoomAnimation.OnComplete = [this, Delta](EGeoAnimationStopType StopType)
		{
			if (StopType == EGeoAnimationStopType::Finish)
			{
				Zoom(Delta);
			}
		};
	}
	else
	{
		Zoom(Delta);
	}
}

bool AGeoCameraController::PickEllipsoidAtScreenPos(const FVector2D& ScreenPos, FVector& OutHitPoint)
{
	FVector RayOrigin, RayDir;
	if (!ComputePickRay(ScreenPos, RayOrigin, RayDir)) return false;

	double StartT, EndT;
	if (RayEllipsoid(RayOrigin, RayDir, StartT, EndT, AdjustCenterHeight))
	{
		double T = (StartT > 0.0) ? StartT : EndT;
		if (T > 0.0)
		{
			OutHitPoint = RayOrigin + RayDir * T;
			return true;
		}
	}
	return false;
}

bool AGeoCameraController::PickTangentPlaneAtScreenPos(const FVector2D& ScreenPos, FVector& OutHitPoint)
{
	FVector RayOrigin, RayDir;
	if (!ComputePickRay(ScreenPos, RayOrigin, RayDir)) return false;

	// Use camera-to-center direction as plane normal so the plane always faces the camera
	FVector CameraPosECEF = CameraECEFTransform.GetOrigin();
	FVector PlaneNormal = CameraPosECEF - CenterECEF;
	if (PlaneNormal.SizeSquared() < EPSILON) return false;
	PlaneNormal.Normalize();

	double Denom = FVector::DotProduct(RayDir, PlaneNormal);
	if (FMath::Abs(Denom) < 1e-6) return false;

	double T = FVector::DotProduct(CenterECEF - RayOrigin, PlaneNormal) / Denom;
	if (T < 0.0) return false;

	OutHitPoint = RayOrigin + RayDir * T;
	return true;
}

void AGeoCameraController::MoveCenterByHitDiff(const FVector& P0, const FVector& P1, bool bAnimate)
{
	FVector P0LLA = EcefToLLA(P0);
	FVector P1LLA = EcefToLLA(P1);
	FVector Diff = P1LLA - P0LLA;

	FVector CenterLLA = EcefToLLA(CenterECEF);
	CenterLLA -= Diff;
	CenterLLA.Y = FMath::Clamp(CenterLLA.Y, -MAX_LATITUDE, MAX_LATITUDE);
	CenterLLA.Z = AdjustCenterHeight;

	if (bAnimate && bEnableMoveAnimation)
	{
		FVector TargetLLA = CenterLLA - Diff * 5.0;
		MoveAnimation.Start(EcefToLLA(CenterECEF), TargetLLA, 0.3f, MoveAnimationCurve);
	}

	CenterECEF = LLAToEcef(CenterLLA);
	bIsDirty = true;
}

void AGeoCameraController::MoveWithScreenPos(const FVector2D& StartPos, const FVector2D& EndPos)
{
	if (ControlMode != EGeoCameraControlMode::GlobalFree) return;

	FVector P0, P1;
	if (!PickEllipsoidAtScreenPos(StartPos, P0)) return;
	if (!PickEllipsoidAtScreenPos(EndPos, P1)) return;

	MoveCenterByHitDiff(P0, P1, true);
}

void AGeoCameraController::MoveWithRay(const FVector& StartRayOrigin, const FVector& StartRayDir, const FVector& EndRayOrigin, const FVector& EndRayDir)
{
	if (ControlMode != EGeoCameraControlMode::GlobalFree) return;

	double Start, Stop;
	FVector P0, P1;

	if (RayEllipsoid(StartRayOrigin, StartRayDir, Start, Stop, AdjustCenterHeight))
	{
		double T = Start > 0.0 ? Start : Stop;
		P0 = StartRayOrigin + StartRayDir * T;
	}
	else
	{
		return;
	}

	if (RayEllipsoid(EndRayOrigin, EndRayDir, Start, Stop, AdjustCenterHeight))
	{
		double T = Start > 0.0 ? Start : Stop;
		P1 = EndRayOrigin + EndRayDir * T;
	}
	else
	{
		return;
	}

	MoveCenterByHitDiff(P0, P1, true);
}

void AGeoCameraController::AutoFixGroundDistance()
{
	FVector LLH = EcefToLLA(CenterECEF);
	if (LLH.Z < AdjustCenterHeight)
	{
		LLH.Z = AdjustCenterHeight;
		CenterECEF = LLAToEcef(LLH);
	}
}

FVector AGeoCameraController::GetCurrentCameraPositionECEF() const
{
	return CameraECEFTransform.GetOrigin();
}

FVector AGeoCameraController::GetCurrentCameraPositionLLA() const
{
	return EcefToLLA(CameraECEFTransform.GetOrigin());
}

FRotator AGeoCameraController::GetCurrentCameraRotation() const
{
	return CameraECEFTransform.Rotator();
}

FVector AGeoCameraController::UnityToEcefPos(const FVector& UnityPos) const
{
	if (!Georeference) return UnityPos;
	return Georeference->TransformUnrealPositionToEarthCenteredEarthFixed(UnityPos);
}

FVector AGeoCameraController::EcefToUnityPos(const FVector& EcefPos) const
{
	if (!Georeference) return EcefPos;
	return Georeference->TransformEarthCenteredEarthFixedPositionToUnreal(EcefPos);
}

FVector AGeoCameraController::UnityToEcefDir(const FVector& UnityDir) const
{
	if (!Georeference) return UnityDir;
	return Georeference->TransformUnrealDirectionToEarthCenteredEarthFixed(UnityDir);
}

FVector AGeoCameraController::EcefToUnityDir(const FVector& EcefDir) const
{
	if (!Georeference) return EcefDir;
	return Georeference->TransformEarthCenteredEarthFixedDirectionToUnreal(EcefDir);
}

FVector AGeoCameraController::EcefToLLA(const FVector& EcefPos) const
{
	if (!Georeference) return FVector::ZeroVector;

	FVector UnrealPos = Georeference->TransformEarthCenteredEarthFixedPositionToUnreal(EcefPos);
	return Georeference->TransformUnrealPositionToLongitudeLatitudeHeight(UnrealPos);
}

FVector AGeoCameraController::LLAToEcef(const FVector& LLA) const
{
	if (!Georeference) return FVector::ZeroVector;

	FVector UnrealPos = Georeference->TransformLongitudeLatitudeHeightPositionToUnreal(LLA);
	return Georeference->TransformUnrealPositionToEarthCenteredEarthFixed(UnrealPos);
}

FVector AGeoCameraController::LLAToEcef(double Longitude, double Latitude, double Altitude) const
{
	return LLAToEcef(FVector(Longitude, Latitude, Altitude));
}

FVector AGeoCameraController::ComputeGeodeticSurfaceNormal(const FVector& EcefPos) const
{
	FVector Radii(WGS84_SEMI_MAJOR_AXIS, WGS84_SEMI_MAJOR_AXIS, WGS84_SEMI_MINOR_AXIS);
	FVector InverseRadii(1.0 / Radii.X, 1.0 / Radii.Y, 1.0 / Radii.Z);

	FVector Normal = EcefPos * InverseRadii;
	Normal.Normalize();
	return Normal;
}

bool AGeoCameraController::RayEllipsoid(const FVector& Origin, const FVector& Dir, double& OutStart, double& OutStop, double AdjustHeight) const
{
	FVector Radii(WGS84_SEMI_MAJOR_AXIS + AdjustHeight, WGS84_SEMI_MAJOR_AXIS + AdjustHeight, WGS84_SEMI_MINOR_AXIS + AdjustHeight);
	FVector InverseRadii = FVector(1.0 / Radii.X, 1.0 / Radii.Y, 1.0 / Radii.Z);

	FVector Q = Origin * InverseRadii;
	FVector W = Dir * InverseRadii;

	double QLen = Q.Size();
	double Q2 = QLen * QLen;
	double QW = FVector::DotProduct(Q, W);

	if (Q2 > 1.0)
	{
		if (QW >= 0.0)
		{
			return false;
		}

		double QW2 = QW * QW;
		double Difference = Q2 - 1.0;
		double WLen = W.Size();
		double W2 = WLen * WLen;
		double Product = W2 * Difference;

		if (QW2 < Product)
		{
			return false;
		}
		else if (QW2 > Product)
		{
			double Discriminant = QW * QW - Product;
			double Temp = -QW + FMath::Sqrt(Discriminant);
			double Root0 = Temp / W2;
			double Root1 = Difference / Temp;

			if (Root0 < Root1)
			{
				OutStart = Root0;
				OutStop = Root1;
			}
			else
			{
				OutStart = Root1;
				OutStop = Root0;
			}
			return true;
		}

		double Root = FMath::Sqrt(Difference / W2);
		OutStart = Root;
		OutStop = Root;
		return true;
	}
	else if (Q2 < 1.0)
	{
		double Difference = Q2 - 1.0;
		double WLen = W.Size();
		double W2 = WLen * WLen;
		double Product = W2 * Difference;

		double Discriminant = QW * QW - Product;
		double Temp = -QW + FMath::Sqrt(Discriminant);
		OutStart = 0.0;
		OutStop = Temp / W2;
		return true;
	}

	if (QW < 0.0)
	{
		double WLen = W.Size();
		double W2 = WLen * WLen;
		OutStart = 0.0;
		OutStop = -QW / W2;
		return true;
	}

	return false;
}

bool AGeoCameraController::ComputePickRay(const FVector2D& ScreenPos, FVector& OutOrigin, FVector& OutDir) const
{
	if (!Camera) return false;

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return false;

	int32 Width, Height;
	PC->GetViewportSize(Width, Height);
	if (Width <= 0 || Height <= 0) return false;

	float FOV = Camera->FieldOfView;
	float AspectRatio = (float)Width / (float)Height;

	float TanHalfFovY = FMath::Tan(FMath::DegreesToRadians(FOV * 0.5f));
	float TanHalfFovX = AspectRatio * TanHalfFovY;

	float NormalizedX = (2.0f * ScreenPos.X / Width) - 1.0f;
	float NormalizedY = 1.0f - (2.0f * ScreenPos.Y / Height);

	FVector CameraWorldPos = Camera->GetComponentLocation();
	FVector CameraForward = Camera->GetForwardVector();
	FVector CameraRight = Camera->GetRightVector();
	FVector CameraUp = Camera->GetUpVector();

	FVector RayDir = CameraForward
		+ CameraRight * (NormalizedX * TanHalfFovX)
		+ CameraUp * (NormalizedY * TanHalfFovY);
	RayDir.Normalize();

	OutOrigin = UnityToEcefPos(CameraWorldPos);
	OutDir = UnityToEcefDir(RayDir);

	return true;
}

void AGeoCameraController::OnResetKey()
{
	ResetToInitialPosition();
}

void AGeoCameraController::ResetToInitialPosition()
{
	if (!bInitialized) return;

	FlyToLocation(InitialCenterECEF, InitialDistance, InitialRotationAngle, InitialTiltAngle, ResetToInitialDuration);
}