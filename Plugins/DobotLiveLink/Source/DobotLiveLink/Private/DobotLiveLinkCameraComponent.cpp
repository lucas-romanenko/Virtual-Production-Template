#include "DobotLiveLinkCameraComponent.h"
#include "DobotLiveLinkSource.h"
#include "ILiveLinkClient.h"
#include "LiveLinkClientReference.h"
#include "Roles/LiveLinkTransformRole.h"
#include "Roles/LiveLinkTransformTypes.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "CineCameraActor.h"
#include "Components/ChildActorComponent.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

UDobotLiveLinkCameraComponent::UDobotLiveLinkCameraComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.bTickEvenWhenPaused = true;
	bTickInEditor = true;

	LiveLinkSubjectName = TEXT("DobotCamera");
	bEnableTracking = false;
	bShowDebugInfo = true;
	bHasRecordedStart = false;
	bHasRobotConnection = false;
	bIsRobotConnected = false;
}

void UDobotLiveLinkCameraComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!CameraToControl)
	{
		FindCamera();
	}
}

void UDobotLiveLinkCameraComponent::FindCamera()
{
	CameraToControl = GetOwner()->FindComponentByClass<UCineCameraComponent>();

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

void UDobotLiveLinkCameraComponent::ResetTrackingOrigin()
{
	bHasRecordedStart = false;
}

bool UDobotLiveLinkCameraComponent::ConnectToRobot()
{
	if (bIsRobotConnected)
	{
		DisconnectFromRobot();
	}

	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	if (!ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
	{
		UE_LOG(LogTemp, Error, TEXT("DobotLiveLinkCamera: LiveLink client not available"));
		return false;
	}

	ILiveLinkClient& LiveLinkClient = ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);

	// Check if another component already created a source with same IP, port, and subject
	bool bSourceExists = false;

	UWorld* World = GetWorld();
	if (!World)
	{
#if WITH_EDITOR
		if (GEditor)
		{
			World = GEditor->GetEditorWorldContext().World();
		}
#endif
	}

	if (World)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			UDobotLiveLinkCameraComponent* OtherComp = (*It)->FindComponentByClass<UDobotLiveLinkCameraComponent>();
			if (OtherComp && OtherComp != this && OtherComp->IsRobotConnected())
			{
				if (OtherComp->RobotIPAddress == RobotIPAddress
					&& OtherComp->RobotPort == RobotPort
					&& OtherComp->LiveLinkSubjectName == LiveLinkSubjectName)
				{
					bSourceExists = true;
					UE_LOG(LogTemp, Warning, TEXT("DobotLiveLinkCamera: Reusing existing source from %s for subject %s"),
						*RobotIPAddress, *LiveLinkSubjectName.ToString());
					break;
				}
			}
		}
	}

	if (!bSourceExists)
	{
		TSharedPtr<FDobotLiveLinkSource> NewSource = MakeShared<FDobotLiveLinkSource>(
			RobotIPAddress, RobotPort, TrackingDelayMs, LiveLinkSubjectName.ToString());

		LiveLinkClient.AddSource(NewSource);
		ConnectedSource = NewSource;

		UE_LOG(LogTemp, Warning, TEXT("DobotLiveLinkCamera: Created new source - IP:%s Port:%d Subject:%s"),
			*RobotIPAddress, RobotPort, *LiveLinkSubjectName.ToString());
	}

	bHasRobotConnection = true;
	bIsRobotConnected = true;

	// Auto-enable tracking on connect
	bEnableTracking = true;
	bHasRecordedStart = false;

	UE_LOG(LogTemp, Warning, TEXT("DobotLiveLinkCamera: Connected and tracking enabled"));

	return true;
}

void UDobotLiveLinkCameraComponent::DisconnectFromRobot()
{
	if (!bIsRobotConnected) return;

	if (ConnectedSource.IsValid())
	{
		// Remove from LiveLink client
		IModularFeatures& ModularFeatures = IModularFeatures::Get();
		if (ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
		{
			ILiveLinkClient& LiveLinkClient = ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);

			TArray<FGuid> SourceGuids = LiveLinkClient.GetSources();
			for (const FGuid& Guid : SourceGuids)
			{
				FText SourceMachine = LiveLinkClient.GetSourceMachineName(Guid);
				if (SourceMachine.ToString() == RobotIPAddress)
				{
					LiveLinkClient.RemoveSource(Guid);
					UE_LOG(LogTemp, Warning, TEXT("DobotLiveLinkCamera: Removed source from LiveLink client"));
					break;
				}
			}
		}

		ConnectedSource->RequestSourceShutdown();
		ConnectedSource.Reset();
	}

	bIsRobotConnected = false;

	// Auto-disable tracking on disconnect
	bEnableTracking = false;
	bHasRecordedStart = false;

	UE_LOG(LogTemp, Warning, TEXT("DobotLiveLinkCamera: Disconnected and tracking disabled"));
}

void UDobotLiveLinkCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!CameraToControl)
	{
		FindCamera();
		if (CameraToControl)
		{
			UE_LOG(LogTemp, Warning, TEXT("DobotLiveLinkCamera: Auto-found camera %s"), *CameraToControl->GetName());
		}
	}

	if (!bEnableTracking || !CameraToControl)
	{
		return;
	}

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

	FLiveLinkSubjectName SubjectName(LiveLinkSubjectName);
	FLiveLinkSubjectFrameData FrameData;

	if (LiveLinkClient->EvaluateFrame_AnyThread(SubjectName, ULiveLinkTransformRole::StaticClass(), FrameData))
	{
		FLiveLinkTransformFrameData* TransformData = FrameData.FrameData.Cast<FLiveLinkTransformFrameData>();
		if (TransformData)
		{
			FTransform CurrentRobotTransform = TransformData->Transform;

			if (!bHasRecordedStart)
			{
				RobotStartTransform = CurrentRobotTransform;
				CameraStartTransform = CameraToControl->GetRelativeTransform();
				bHasRecordedStart = true;

				UE_LOG(LogTemp, Log, TEXT("Recorded start positions - Camera: %s, Robot: %s"),
					*CameraStartTransform.GetLocation().ToString(),
					*RobotStartTransform.GetLocation().ToString());
			}

			FVector PositionDelta = CurrentRobotTransform.GetLocation() - RobotStartTransform.GetLocation();
			FRotator RotationDelta = (CurrentRobotTransform.Rotator() - RobotStartTransform.Rotator());

			FVector NewCameraLocation = CameraStartTransform.GetLocation() + PositionDelta;
			FRotator NewCameraRotation = CameraStartTransform.Rotator() + RotationDelta;

			FTransform NewCameraTransform(NewCameraRotation, NewCameraLocation, CameraStartTransform.GetScale3D());
			CameraToControl->SetRelativeTransform(NewCameraTransform);

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