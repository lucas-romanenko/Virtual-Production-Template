#include "DobotLiveLinkCameraComponent.h"
#include "DobotLiveLinkSource.h"
#include "DobotLiveLinkSettings.h"
#include "ILiveLinkClient.h"
#include "LiveLinkClientReference.h"
#include "Roles/LiveLinkCameraRole.h"
#include "Roles/LiveLinkCameraTypes.h"
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
	bIsReconnecting = false;
	ReconnectTimer = 0.0f;
	ReconnectLogTimer = 0.0f;
	bHasAttemptedStartupAutoConnect = false;
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

EDobotConnectionState UDobotLiveLinkCameraComponent::GetConnectionState() const
{
	if (bIsReconnecting)
	{
		return EDobotConnectionState::Reconnecting;
	}

	if (!bIsRobotConnected)
	{
		return EDobotConnectionState::NoConnection;
	}

	if (ConnectedSource.IsValid() && ConnectedSource->IsSourceStillValid())
	{
		return EDobotConnectionState::Connected;
	}

	return EDobotConnectionState::ConnectionLost;
}

void UDobotLiveLinkCameraComponent::CleanupDeadSource()
{
	// Remove dead source from LiveLink client
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
				UE_LOG(LogTemp, Warning, TEXT("DobotLiveLinkCamera: Removed dead source from LiveLink"));
				break;
			}
		}
	}

	ConnectedSource.Reset();
	bIsRobotConnected = false;
	bEnableTracking = false;
	bHasRecordedStart = false;
}

void UDobotLiveLinkCameraComponent::AttemptReconnect()
{
	// Just try to connect normally
	// If it fails, the source will die quickly and we'll try again next interval
	bool bSuccess = ConnectToRobot();

	if (bSuccess)
	{
		// ConnectToRobot resets bIsReconnecting via the reconnect stop at the top
		// But we need to re-enable it if the source dies immediately
		// Check on next tick via the normal connection lost detection
		bIsReconnecting = false;

		UE_LOG(LogTemp, Warning, TEXT("DobotLiveLinkCamera: Reconnected to %s:%d (Subject: %s)"),
			*RobotIPAddress, RobotPort, *LiveLinkSubjectName.ToString());

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
				FString::Printf(TEXT("Dobot Reconnected: %s:%d"), *RobotIPAddress, RobotPort));
		}
	}
	else
	{
		// Stay in reconnecting state, ConnectToRobot cleared it so set it back
		bIsReconnecting = true;
	}
}

bool UDobotLiveLinkCameraComponent::ConnectToRobot()
{
	if (bIsRobotConnected)
	{
		DisconnectFromRobot();
	}

	// Stop any reconnect attempts
	bIsReconnecting = false;
	ReconnectTimer = 0.0f;

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
					ConnectedSource = OtherComp->ConnectedSource;
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

	bEnableTracking = true;
	bHasRecordedStart = false;

	UE_LOG(LogTemp, Warning, TEXT("DobotLiveLinkCamera: Connected and tracking enabled"));

	return true;
}

void UDobotLiveLinkCameraComponent::DisconnectFromRobot()
{
	// Stop any reconnect attempts
	bIsReconnecting = false;
	ReconnectTimer = 0.0f;

	if (!bIsRobotConnected) return;

	if (ConnectedSource.IsValid())
	{
		CleanupDeadSource();
	}
	else
	{
		bIsRobotConnected = false;
		bEnableTracking = false;
		bHasRecordedStart = false;
	}

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

	// ---- Auto-connect on startup ----
	if (!bHasAttemptedStartupAutoConnect && bHasRobotConnection && !bIsRobotConnected && !bIsReconnecting)
	{
		UDobotLiveLinkSettings* Settings = UDobotLiveLinkSettings::Get();
		bool bShouldAutoConnect = Settings->ShouldAutoConnect(LiveLinkSubjectName.ToString());

		if (bShouldAutoConnect)
		{
			// Wait a few seconds after editor starts before attempting
			ReconnectTimer += DeltaTime;
			if (ReconnectTimer >= 5.0f)
			{
				bHasAttemptedStartupAutoConnect = true;
				ReconnectTimer = 0.0f;

				UE_LOG(LogTemp, Warning, TEXT("DobotLiveLinkCamera: Auto-connecting to %s:%d on startup..."),
					*RobotIPAddress, RobotPort);

				if (!ConnectToRobot())
				{
					bIsReconnecting = true;
					ReconnectTimer = 0.0f;
					ReconnectLogTimer = 0.0f;

					UE_LOG(LogTemp, Warning, TEXT("DobotLiveLinkCamera: Auto-connect failed, will retry every %.0fs"),
						ReconnectInterval);
				}
			}
		}
		else
		{
			bHasAttemptedStartupAutoConnect = true;
		}
	}

	// ---- Check for connection lost ----
	if (bIsRobotConnected)
	{
		EDobotConnectionState State = GetConnectionState();
		if (State == EDobotConnectionState::ConnectionLost)
		{
			UE_LOG(LogTemp, Warning, TEXT("DobotLiveLinkCamera: Connection lost to %s:%d"), *RobotIPAddress, RobotPort);

			CleanupDeadSource();

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Orange,
					FString::Printf(TEXT("Dobot Connection Lost: %s:%d"), *RobotIPAddress, RobotPort));
			}

			// Start auto-reconnect if enabled in config
			UDobotLiveLinkSettings* Settings = UDobotLiveLinkSettings::Get();
			if (Settings->ShouldAutoConnect(LiveLinkSubjectName.ToString()))
			{
				bIsReconnecting = true;
				ReconnectTimer = 0.0f;
				ReconnectLogTimer = 0.0f;
			}
		}
	}

	// ---- Auto-reconnect loop ----
	if (bIsReconnecting)
	{
		ReconnectTimer += DeltaTime;
		ReconnectLogTimer += DeltaTime;

		if (ReconnectTimer >= ReconnectInterval)
		{
			ReconnectTimer = 0.0f;
			AttemptReconnect();
		}

		// Log periodically so we don't spam
		if (ReconnectLogTimer >= ReconnectLogInterval)
		{
			ReconnectLogTimer = 0.0f;
			UE_LOG(LogTemp, Log, TEXT("DobotLiveLinkCamera: Still trying to reconnect to %s:%d..."),
				*RobotIPAddress, RobotPort);
		}

		return; // Don't try to track while reconnecting
	}

	// ---- Tracking ----
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

	if (LiveLinkClient->EvaluateFrame_AnyThread(SubjectName, ULiveLinkCameraRole::StaticClass(), FrameData))
	{
		FLiveLinkCameraFrameData* CameraData = FrameData.FrameData.Cast<FLiveLinkCameraFrameData>();
		if (CameraData)
		{
			FTransform CurrentRobotTransform = CameraData->Transform;

			// ---- Apply lens data ----
			if (CameraData->FocalLength > 0.0f)
			{
				CameraToControl->CurrentFocalLength = CameraData->FocalLength;
			}
			if (CameraData->Aperture > 0.0f)
			{
				CameraToControl->CurrentAperture = CameraData->Aperture;
			}
			if (CameraData->FocusDistance > 0.0f)
			{
				CameraToControl->FocusSettings.ManualFocusDistance = CameraData->FocusDistance;
			}

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