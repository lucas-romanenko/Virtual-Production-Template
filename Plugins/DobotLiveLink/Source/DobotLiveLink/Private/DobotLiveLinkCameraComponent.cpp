#include "DobotLiveLinkCameraComponent.h"
#include "ILiveLinkClient.h"
#include "LiveLinkClientReference.h"
#include "Roles/LiveLinkTransformRole.h"
#include "Roles/LiveLinkTransformTypes.h"
#include "Engine/Engine.h"
#include "CineCameraActor.h"
#include "Components/ChildActorComponent.h"

UDobotLiveLinkCameraComponent::UDobotLiveLinkCameraComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	// CRITICAL: Enable ticking in editor
	PrimaryComponentTick.bTickEvenWhenPaused = true;
	bTickInEditor = true;

	LiveLinkSubjectName = TEXT("DobotCamera");
	bEnableTracking = false;  // Start disabled
	bShowDebugInfo = true;
	bHasRecordedStart = false;
}

void UDobotLiveLinkCameraComponent::BeginPlay()
{
	Super::BeginPlay();

	// Auto-find camera if not set
	if (!CameraToControl)
	{
		FindCamera();
	}
}

void UDobotLiveLinkCameraComponent::FindCamera()
{
	// 1. Try direct component on this actor
	CameraToControl = GetOwner()->FindComponentByClass<UCineCameraComponent>();

	// 2. Try Child Actor Components
	if (!CameraToControl)
	{
		TArray<UChildActorComponent*> ChildActors;
		GetOwner()->GetComponents<UChildActorComponent>(ChildActors);
		for (UChildActorComponent* ChildComp : ChildActors)
		{
			if (AActor* ChildActor = ChildComp->GetChildActor())
			{
				CameraToControl = ChildActor->FindComponentByClass<UCineCameraComponent>();
				if (CameraToControl)
				{
					UE_LOG(LogTemp, Warning, TEXT("DobotLiveLinkCamera: Found camera in Child Actor"));
					break;
				}
			}
		}
	}

	// 3. Try if owner IS a CineCameraActor (component added directly to camera in level)
	if (!CameraToControl)
	{
		if (ACineCameraActor* CineActor = Cast<ACineCameraActor>(GetOwner()))
		{
			CameraToControl = CineActor->GetCineCameraComponent();
			if (CameraToControl)
			{
				UE_LOG(LogTemp, Warning, TEXT("DobotLiveLinkCamera: Found camera on CineCameraActor owner"));
			}
		}
	}
}

void UDobotLiveLinkCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Auto-find camera if not set (works in editor)
	if (!CameraToControl)
	{
		FindCamera();
		if (CameraToControl)
		{
			UE_LOG(LogTemp, Warning, TEXT("DobotLiveLinkCamera: Auto-found camera %s"), *CameraToControl->GetName());
		}
	}

	// Skip if tracking disabled or no camera
	if (!bEnableTracking || !CameraToControl)
	{
		return;
	}

	// Get LiveLink client
	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	if (!ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
	{
		return;
	}

	ILiveLinkClient* LiveLinkClient = &IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
	if (!LiveLinkClient)
	{
		return;
	}

	// Get LiveLink data
	FLiveLinkSubjectName SubjectName(LiveLinkSubjectName);
	FLiveLinkSubjectFrameData FrameData;

	if (LiveLinkClient->EvaluateFrame_AnyThread(SubjectName, ULiveLinkTransformRole::StaticClass(), FrameData))
	{
		FLiveLinkTransformFrameData* TransformData = FrameData.FrameData.Cast<FLiveLinkTransformFrameData>();
		if (TransformData)
		{
			FTransform CurrentRobotTransform = TransformData->Transform;

			// Record starting positions on first frame
			if (!bHasRecordedStart)
			{
				RobotStartTransform = CurrentRobotTransform;
				CameraStartTransform = CameraToControl->GetRelativeTransform();
				bHasRecordedStart = true;

				UE_LOG(LogTemp, Log, TEXT("Recorded start positions - Camera: %s, Robot: %s"),
					*CameraStartTransform.GetLocation().ToString(),
					*RobotStartTransform.GetLocation().ToString());
			}

			// Calculate DELTA (change) from robot start position
			FVector PositionDelta = CurrentRobotTransform.GetLocation() - RobotStartTransform.GetLocation();
			FRotator RotationDelta = (CurrentRobotTransform.Rotator() - RobotStartTransform.Rotator());

			// Apply DELTA to camera's start position
			FVector NewCameraLocation = CameraStartTransform.GetLocation() + PositionDelta;
			FRotator NewCameraRotation = CameraStartTransform.Rotator() + RotationDelta;

			FTransform NewCameraTransform(NewCameraRotation, NewCameraLocation, CameraStartTransform.GetScale3D());
			CameraToControl->SetRelativeTransform(NewCameraTransform);

			// Debug display
			if (bShowDebugInfo && GEngine)
			{
				FString DebugText = FString::Printf(
					TEXT("TRACKING ACTIVE\nMovement Delta: X=%.1f Y=%.1f Z=%.1f\nCamera Pos: X=%.1f Y=%.1f Z=%.1f"),
					PositionDelta.X, PositionDelta.Y, PositionDelta.Z,
					NewCameraLocation.X, NewCameraLocation.Y, NewCameraLocation.Z
				);
				GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Green, DebugText);
			}
		}
	}
}

void UDobotLiveLinkCameraComponent::StartTracking()
{
	if (!CameraToControl)
	{
		FindCamera();
	}

	if (!CameraToControl)
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot start tracking: No camera assigned"));
		return;
	}

	bEnableTracking = true;
	bHasRecordedStart = false;  // Will record on next tick

	UE_LOG(LogTemp, Log, TEXT("Tracking STARTED - Camera will move relative to current position"));

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, TEXT("Tracking Started!"));
	}
}

void UDobotLiveLinkCameraComponent::StopTracking()
{
	bEnableTracking = false;
	bHasRecordedStart = false;

	UE_LOG(LogTemp, Log, TEXT("Tracking STOPPED"));

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, TEXT("Tracking Stopped"));
	}
}