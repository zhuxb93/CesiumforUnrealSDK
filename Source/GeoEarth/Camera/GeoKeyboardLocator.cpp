#include "GeoKeyboardLocator.h"
#include "GeoCameraController.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

AGeoKeyboardLocator::AGeoKeyboardLocator()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AGeoKeyboardLocator::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoFindCameraController && !CameraController)
	{
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGeoCameraController::StaticClass(), FoundActors);
		if (FoundActors.Num() > 0)
		{
			CameraController = Cast<AGeoCameraController>(FoundActors[0]);
		}
	}

	SetupInputBindings();
}

void AGeoKeyboardLocator::SetupInputBindings()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC || !PC->InputComponent)
	{
		return;
	}

	for (int32 i = 0; i < Locations.Num(); ++i)
	{
		const FGeoKeyboardLocation& Location = Locations[i];
		if (!Location.bEnabled || !Location.Hotkey.IsValid())
		{
			continue;
		}

		FInputChord Chord(Location.Hotkey, false, false, false, false);
		FInputKeyBinding KeyBinding(Chord, IE_Pressed);
		KeyBinding.KeyDelegate.GetDelegateForManualSet().BindLambda([this, i]()
		{
			OnHotkeyPressed(i);
		});
		PC->InputComponent->KeyBindings.Add(KeyBinding);
	}
}

void AGeoKeyboardLocator::OnHotkeyPressed(int32 LocationIndex)
{
	if (!Locations.IsValidIndex(LocationIndex))
	{
		return;
	}

	const FGeoKeyboardLocation& Location = Locations[LocationIndex];
	if (!Location.bEnabled)
	{
		return;
	}

	FlyToLocation(Location);
}

void AGeoKeyboardLocator::FlyToLocation(const FGeoKeyboardLocation& Location)
{
	if (!CameraController)
	{
		UE_LOG(LogTemp, Warning, TEXT("GeoKeyboardLocator: CameraController is not set"));
		return;
	}

	CameraController->FlyToLocationLLA(
		Location.Longitude,
		Location.Latitude,
		0.0,
		Location.Distance,
		Location.Rotation,
		Location.Tilt,
		Location.FlyDuration
	);

	UE_LOG(LogTemp, Log, TEXT("GeoKeyboardLocator: Flying to %s (%.6f, %.6f, Distance: %.2f)"),
		*Location.LocationName,
		Location.Longitude,
		Location.Latitude,
		Location.Distance
	);
}