#include "PoiOcclusionManager.h"
#include "GeoPoiTileActor.h"
#include "PoiWidgetActor.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/WidgetTree.h"

DEFINE_LOG_CATEGORY_STATIC(LogPoiOcclusion, Verbose, All);

UPoiOcclusionManager::UPoiOcclusionManager()
{
    CheckInterval = 1.0f;
    MaxPoiPerCheck = 1000;
}

void UPoiOcclusionManager::Initialize(AGeoPoiTileActor* InOwner)
{
    OwnerActor = InOwner;
    CheckTimer = 0.0f;
}

void UPoiOcclusionManager::CheckOcclusion()
{
    if (!OwnerActor)
    {
        return;
    }
    
    if (!OwnerActor->GetWorld())
    {
        return;
    }

    TArray<APoiWidgetActor*> VisiblePoiActors = CollectVisiblePoiActors();
    
    if (VisiblePoiActors.Num() <= 1)
    {
        return;
    }

    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(OwnerActor->GetWorld(), 0);
    if (!PlayerController)
    {
        return;
    }

    CalculateScreenRects(VisiblePoiActors, PlayerController);

    for (int32 i = 0; i < VisiblePoiActors.Num(); i++)
    {
        VisiblePoiActors[i]->SetSortIndex(i);
    }

    ExecuteCpuOcclusion(VisiblePoiActors);
}

void UPoiOcclusionManager::Cleanup()
{
    OwnerActor = nullptr;
}

TArray<APoiWidgetActor*> UPoiOcclusionManager::CollectVisiblePoiActors()
{
    TArray<APoiWidgetActor*> Result;

    if (!OwnerActor)
    {
        return Result;
    }

    const TMap<FString, APoiWidgetActor*>& AllPoiWidgets = OwnerActor->PoiWidgetActors;
    
    for (const auto& Pair : AllPoiWidgets)
    {
        if (Pair.Value && Pair.Value->IsOccluded())
        {
            Pair.Value->SetOccluded(false);
        }
    }
    
    for (const auto& Pair : AllPoiWidgets)
    {
        if (Pair.Value && !Pair.Value->IsHidden())
        {
            Result.Add(Pair.Value);
        }
    }

    Result.Sort([](const APoiWidgetActor& A, const APoiWidgetActor& B)
    {
        return A.GetCameraDistance() < B.GetCameraDistance();
    });

    if (Result.Num() > MaxPoiPerCheck)
    {
        Result.SetNum(MaxPoiPerCheck);
    }

    return Result;
}

FBox2D UPoiOcclusionManager::GetWidgetContentScreenBounds(UWidgetComponent* WidgetComp, APlayerController* PlayerController)
{
    if (!WidgetComp || !PlayerController)
    {
        return FBox2D(FVector2D(-1, -1), FVector2D(-1, -1));
    }

    FVector WidgetWorldLocation = WidgetComp->GetComponentLocation();
    FVector2D WidgetCenterScreen;
    
    bool bProjected = UGameplayStatics::ProjectWorldToScreen(PlayerController, WidgetWorldLocation, WidgetCenterScreen);
    if (!bProjected)
    {
        return FBox2D(FVector2D(-1, -1), FVector2D(-1, -1));
    }

    int32 ViewportSizeX = 0;
    int32 ViewportSizeY = 0;
    PlayerController->GetViewportSize(ViewportSizeX, ViewportSizeY);
    
    FVector2D ViewportSize(ViewportSizeX, ViewportSizeY);
    if (ViewportSize.X <= 0 || ViewportSize.Y <= 0)
    {
        ViewportSize = FVector2D(1920.0f, 1080.0f);
    }

    FVector2D MarkerSize = DefaultMarkerSize;

    MarkerSize.X = FMath::Clamp(MarkerSize.X, 20.0f, 200.0f);
    MarkerSize.Y = FMath::Clamp(MarkerSize.Y, 20.0f, 200.0f);

    FVector2D HalfSize = MarkerSize * 0.5f;
    FBox2D ScreenRect(WidgetCenterScreen - HalfSize, WidgetCenterScreen + HalfSize);

    if (ScreenRect.Min.X >= ScreenRect.Max.X || ScreenRect.Min.Y >= ScreenRect.Max.Y)
    {
        return FBox2D(FVector2D(-1, -1), FVector2D(-1, -1));
    }

    if (ScreenRect.Max.X < 0 || ScreenRect.Min.X > ViewportSize.X ||
        ScreenRect.Max.Y < 0 || ScreenRect.Min.Y > ViewportSize.Y)
    {
        return FBox2D(FVector2D(-1, -1), FVector2D(-1, -1));
    }

    return ScreenRect;
}

void UPoiOcclusionManager::CalculateScreenRects(TArray<APoiWidgetActor*>& PoiActors, APlayerController* PlayerController)
{
    if (!PlayerController)
    {
        return;
    }

    FVector CameraLocation = PlayerController->PlayerCameraManager ? PlayerController->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
    
    for (APoiWidgetActor* PoiActor : PoiActors)
    {
        if (!PoiActor || !PoiActor->GetWidgetComponent())
        {
            continue;
        }

        UWidgetComponent* WidgetComp = PoiActor->GetWidgetComponent();
        FVector WidgetLocation = WidgetComp->GetComponentLocation();
        
        float DistanceToCamera = FVector::Distance(WidgetLocation, CameraLocation);
        PoiActor->SetCameraDistance(DistanceToCamera);

        FBox2D ScreenRect = GetWidgetContentScreenBounds(WidgetComp, PlayerController);
        
        if (ScreenRect.Min.X >= 0 && ScreenRect.Max.X > ScreenRect.Min.X)
        {
            PoiActor->SetScreenRect(ScreenRect);
        }
        else
        {
            PoiActor->SetScreenRect(FBox2D(FVector2D(-1, -1), FVector2D(-1, -1)));
        }
    }
}

void UPoiOcclusionManager::ExecuteCpuOcclusion(TArray<APoiWidgetActor*>& PoiActors)
{
    if (PoiActors.Num() <= 1)
    {
        return;
    }

    TArray<bool> OccludedFlags;
    OccludedFlags.SetNumZeroed(PoiActors.Num());
    
    for (int32 i = 0; i < PoiActors.Num(); i++)
    {
        FBox2D Rect1 = PoiActors[i]->GetScreenRect();
        
        if (Rect1.Min.X < 0 && Rect1.Max.X < 0)
        {
            continue;
        }

        for (int32 j = i + 1; j < PoiActors.Num(); j++)
        {
            if (OccludedFlags[i] || OccludedFlags[j])
            {
                continue;
            }

            FBox2D Rect2 = PoiActors[j]->GetScreenRect();
            
            if (Rect2.Min.X < 0 && Rect2.Max.X < 0)
            {
                continue;
            }

            bool bOverlaps = Rect1.Min.X < Rect2.Max.X && Rect1.Max.X > Rect2.Min.X &&
                             Rect1.Min.Y < Rect2.Max.Y && Rect1.Max.Y > Rect2.Min.Y;
            
            if (bOverlaps)
            {
                if (PoiActors[i]->GetSortIndex() > PoiActors[j]->GetSortIndex())
                {
                    OccludedFlags[i] = true;
                }
                else
                {
                    OccludedFlags[j] = true;
                }
            }
        }
    }

    for (int32 i = 0; i < PoiActors.Num(); i++)
    {
        PoiActors[i]->SetOccluded(OccludedFlags[i]);
    }
}