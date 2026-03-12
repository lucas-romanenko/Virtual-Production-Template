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
}

void UDobotLiveLinkCameraComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!CameraToControl) FindCamera();
}

void UDobotLiveLinkCameraComponent::FindCamera()
{
	CameraToControl = GetOwner()->FindComponentByClass<UCineCameraComponent>();
	if (!CameraToControl)
	{
		TArray<UChildActorComponent*> Children;
		GetOwner()->GetComponents<UChildActorComponent>(Children);
		for (auto* Child : Children)
			if (AActor* A = Child->GetChildActor()) { CameraToControl = A->FindComponentByClass<UCineCameraComponent>(); if (CameraToControl) break; }
	}
	if (!CameraToControl)
		if (ACineCameraActor* CA = Cast<ACineCameraActor>(GetOwner()))
			CameraToControl = CA->GetCineCameraComponent();
}

void UDobotLiveLinkCameraComponent::ResetTrackingOrigin() { bHasRecordedStart = false; }

EDobotConnectionState UDobotLiveLinkCameraComponent::GetConnectionState() const
{
	if (bIsReconnecting) return EDobotConnectionState::Reconnecting;
	if (!bIsRobotConnected) return EDobotConnectionState::NoConnection;
	if (ConnectedSource.IsValid() && ConnectedSource->IsSourceStillValid()) return EDobotConnectionState::Connected;
	return EDobotConnectionState::ConnectionLost;
}

void UDobotLiveLinkCameraComponent::CleanupDeadSource()
{
	IModularFeatures& MF = IModularFeatures::Get();
	if (MF.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
	{
		ILiveLinkClient& LLC = MF.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
		FString Expected = FString::Printf(TEXT("%s:%d"), *RobotIPAddress, RobotPort);
		for (const FGuid& G : LLC.GetSources())
			if (LLC.GetSourceMachineName(G).ToString() == Expected) { LLC.RemoveSource(G); break; }
	}
	ConnectedSource.Reset();
	bIsRobotConnected = false;
	bEnableTracking = false;
	bHasRecordedStart = false;
}

void UDobotLiveLinkCameraComponent::AttemptReconnect()
{
	if (ConnectToRobot()) { bIsReconnecting = false; }
	else { bIsReconnecting = true; }
}

bool UDobotLiveLinkCameraComponent::ConnectToRobot()
{
	if (bIsRobotConnected) DisconnectFromRobot();
	bIsReconnecting = false; ReconnectTimer = 0;

	IModularFeatures& MF = IModularFeatures::Get();
	if (!MF.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName)) return false;
	ILiveLinkClient& LLC = MF.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);

	// Clean up any stale sources with the same subject name from previous sessions
	{
		TArray<FGuid> SourcesToRemove;
		for (const FGuid& Guid : LLC.GetSources())
		{
			FText MachineName = LLC.GetSourceMachineName(Guid);
			// Remove sources that match our IP:Port OR any source whose subject matches ours
			if (MachineName.ToString() == FString::Printf(TEXT("%s:%d"), *RobotIPAddress, RobotPort))
			{
				SourcesToRemove.Add(Guid);
			}
		}
		for (const FGuid& Guid : SourcesToRemove)
		{
			LLC.RemoveSource(Guid);
			UE_LOG(LogTemp, Warning, TEXT("LiveLinkCamera: Removed stale source before connect"));
		}
	}

	bool bExists = false;
	UWorld* W = GetWorld();
#if WITH_EDITOR
	if (!W && GEditor) W = GEditor->GetEditorWorldContext().World();
#endif
	if (W)
	{
		for (TActorIterator<AActor> It(W); It; ++It)
		{
			auto* Other = (*It)->FindComponentByClass<UDobotLiveLinkCameraComponent>();
			if (Other && Other != this && Other->IsRobotConnected()
				&& Other->RobotIPAddress == RobotIPAddress && Other->RobotPort == RobotPort
				&& Other->LiveLinkSubjectName == LiveLinkSubjectName)
			{
				bExists = true;
				ConnectedSource = Other->ConnectedSource;
				break;
			}
		}
	}

	if (!bExists)
	{
		auto Src = MakeShared<FDobotLiveLinkSource>(RobotIPAddress, RobotPort, TrackingDelayMs, LiveLinkSubjectName.ToString());
		LLC.AddSource(Src);
		ConnectedSource = Src;
	}

	bHasRobotConnection = true;
	bIsRobotConnected = true;
	bEnableTracking = true;
	bHasRecordedStart = false;
	return true;
}

void UDobotLiveLinkCameraComponent::DisconnectFromRobot()
{
	bIsReconnecting = false; ReconnectTimer = 0;
	if (!bIsRobotConnected) return;
	if (ConnectedSource.IsValid()) CleanupDeadSource();
	else { bIsRobotConnected = false; bEnableTracking = false; bHasRecordedStart = false; }
}

void UDobotLiveLinkCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!CameraToControl) { FindCamera(); }

	// Auto-connect on startup
	if (!bHasAttemptedStartupAutoConnect && bHasRobotConnection && !bIsRobotConnected && !bIsReconnecting)
	{
		UDobotLiveLinkSettings* S = UDobotLiveLinkSettings::Get();
		if (S->ShouldAutoConnect(LiveLinkSubjectName.ToString()))
		{
			ReconnectTimer += DeltaTime;
			if (ReconnectTimer >= 5.0f) { bHasAttemptedStartupAutoConnect = true; ReconnectTimer = 0; if (!ConnectToRobot()) { bIsReconnecting = true; ReconnectTimer = 0; ReconnectLogTimer = 0; } }
		}
		else bHasAttemptedStartupAutoConnect = true;
	}

	// Connection lost
	if (bIsRobotConnected && GetConnectionState() == EDobotConnectionState::ConnectionLost)
	{
		CleanupDeadSource();
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Orange, FString::Printf(TEXT("LiveLink Lost: %s:%d"), *RobotIPAddress, RobotPort));
		UDobotLiveLinkSettings* S = UDobotLiveLinkSettings::Get();
		if (S->ShouldAutoConnect(LiveLinkSubjectName.ToString())) { bIsReconnecting = true; ReconnectTimer = 0; ReconnectLogTimer = 0; }
	}

	// Reconnect loop
	if (bIsReconnecting)
	{
		ReconnectTimer += DeltaTime; ReconnectLogTimer += DeltaTime;
		if (ReconnectTimer >= ReconnectInterval) { ReconnectTimer = 0; AttemptReconnect(); }
		if (ReconnectLogTimer >= ReconnectLogInterval) { ReconnectLogTimer = 0; }
		return;
	}

	// Push lens data to source (so LiveLink frame has real values)
	if (bIsRobotConnected && CameraToControl && ConnectedSource.IsValid())
	{
		ConnectedSource->SetMappedLensData(
			CameraToControl->CurrentFocalLength,
			CameraToControl->CurrentAperture,
			CameraToControl->FocusSettings.ManualFocusDistance);
	}

	// Tracking
	if (!bEnableTracking || !CameraToControl) return;
	if (ConnectedSource.IsValid()) ConnectedSource->SetFrozen(bFreezeTracking);
	if (bFreezeTracking) return;

	IModularFeatures& MF = IModularFeatures::Get();
	if (!MF.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName)) return;
	ILiveLinkClient* LLC = &MF.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
	if (!LLC) return;

	FLiveLinkSubjectFrameData FD;
	if (LLC->EvaluateFrame_AnyThread(FLiveLinkSubjectName(LiveLinkSubjectName), ULiveLinkCameraRole::StaticClass(), FD))
	{
		FLiveLinkCameraFrameData* CD = FD.FrameData.Cast<FLiveLinkCameraFrameData>();
		if (CD)
		{
			FTransform RT = CD->Transform;

			// Apply lens from FreeD
			if (CD->FocalLength > 0) CameraToControl->CurrentFocalLength = CD->FocalLength;
			if (CD->Aperture > 0) CameraToControl->CurrentAperture = CD->Aperture;
			if (CD->FocusDistance > 0) CameraToControl->FocusSettings.ManualFocusDistance = CD->FocusDistance;

			if (!bHasRecordedStart)
			{
				RobotStartTransform = RT;
				CameraStartTransform = CameraToControl->GetRelativeTransform();
				bHasRecordedStart = true;
			}

			FVector Delta = RT.GetLocation() - RobotStartTransform.GetLocation();
			FRotator RDelta = RT.Rotator() - RobotStartTransform.Rotator();
			FTransform New(CameraStartTransform.Rotator() + RDelta, CameraStartTransform.GetLocation() + Delta, CameraStartTransform.GetScale3D());
			CameraToControl->SetRelativeTransform(New);

			if (bShowDebugInfo && GEngine)
				GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Green, FString::Printf(
					TEXT("TRACKING | dX=%.1f dY=%.1f dZ=%.1f"), Delta.X, Delta.Y, Delta.Z));
		}
	}
}