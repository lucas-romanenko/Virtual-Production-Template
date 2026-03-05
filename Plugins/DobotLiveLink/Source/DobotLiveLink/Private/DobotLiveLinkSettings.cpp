#include "DobotLiveLinkSettings.h"
#include "DobotLiveLinkCameraComponent.h"
#include "DobotLiveLinkSource.h"
#include "CineCameraComponent.h"
#include "CineCameraActor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "ILiveLinkClient.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

UDobotLiveLinkSettings::UDobotLiveLinkSettings()
	: FocalLength(24.0f)
	, Aperture(2.8f)
	, SensorWidth(36.0f)
	, SensorHeight(24.0f)
	, RobotIPAddress(TEXT("192.168.5.1"))
	, RobotPort(30004)
	, SubjectName(TEXT("DobotCamera"))
	, bEnableTracking(false)
	, TrackingDelayMs(0.0f)
	, bTestMode(false)
	, bOutputActive(false)
	, bIsConnected(false)
{
}

UDobotLiveLinkSettings* UDobotLiveLinkSettings::Get()
{
	return GetMutableDefault<UDobotLiveLinkSettings>();
}

#if WITH_EDITOR
void UDobotLiveLinkSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	ApplyToCamera();
}
#endif

void UDobotLiveLinkSettings::ApplyToCamera()
{
	UWorld* World = nullptr;

#if WITH_EDITOR
	if (GEditor)
	{
		World = GEditor->GetEditorWorldContext().World();
	}
#endif

	if (!World) return;

	for (TActorIterator<ACineCameraActor> It(World); It; ++It)
	{
		ACineCameraActor* CameraActor = *It;
		if (!CameraActor) continue;

		UDobotLiveLinkCameraComponent* DobotComp = CameraActor->FindComponentByClass<UDobotLiveLinkCameraComponent>();
		if (!DobotComp) continue;

		UCineCameraComponent* CineComp = CameraActor->GetCineCameraComponent();
		if (CineComp)
		{
			CineComp->CurrentFocalLength = FocalLength;
			CineComp->CurrentAperture = Aperture;

			FCameraFilmbackSettings Filmback = CineComp->Filmback;
			Filmback.SensorWidth = SensorWidth;
			Filmback.SensorHeight = SensorHeight;
			CineComp->Filmback = Filmback;
		}

		DobotComp->LiveLinkSubjectName = FName(*SubjectName);
		DobotComp->bEnableTracking = bEnableTracking;

		break;
	}
}

void UDobotLiveLinkSettings::LoadFromCamera()
{
	UWorld* World = nullptr;

#if WITH_EDITOR
	if (GEditor)
	{
		World = GEditor->GetEditorWorldContext().World();
	}
#endif

	if (!World) return;

	for (TActorIterator<ACineCameraActor> It(World); It; ++It)
	{
		ACineCameraActor* CameraActor = *It;
		if (!CameraActor) continue;

		UDobotLiveLinkCameraComponent* DobotComp = CameraActor->FindComponentByClass<UDobotLiveLinkCameraComponent>();
		if (!DobotComp) continue;

		UCineCameraComponent* CineComp = CameraActor->GetCineCameraComponent();
		if (CineComp)
		{
			FocalLength = CineComp->CurrentFocalLength;
			Aperture = CineComp->CurrentAperture;
			SensorWidth = CineComp->Filmback.SensorWidth;
			SensorHeight = CineComp->Filmback.SensorHeight;
		}

		SubjectName = DobotComp->LiveLinkSubjectName.ToString();
		bEnableTracking = DobotComp->bEnableTracking;

		break;
	}
}

bool UDobotLiveLinkSettings::ConnectToRobot()
{
	// Disconnect existing connection first
	if (bIsConnected)
	{
		DisconnectFromRobot();
	}

	// Get the LiveLink client
	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	if (!ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
	{
		UE_LOG(LogTemp, Error, TEXT("Dobot Settings: LiveLink client not available"));
		return false;
	}

	ILiveLinkClient& LiveLinkClient = ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);

	// Create the Dobot source
	TSharedPtr<FDobotLiveLinkSource> NewSource = MakeShared<FDobotLiveLinkSource>(
		RobotIPAddress, RobotPort, bTestMode, TrackingDelayMs, SubjectName);

	// Add the source to LiveLink - store as virtual source for disconnect
	LiveLinkClient.AddSource(NewSource);

	bIsConnected = true;
	// Store source pointer for later removal
	ConnectedSourcePtr = NewSource;

	UE_LOG(LogTemp, Warning, TEXT("Dobot Settings: Connected to %s:%d (Subject: %s)"),
		*RobotIPAddress, RobotPort, *SubjectName);

	return true;
}

void UDobotLiveLinkSettings::DisconnectFromRobot()
{
	if (!bIsConnected) return;

	if (ConnectedSourcePtr.IsValid())
	{
		ConnectedSourcePtr->RequestSourceShutdown();
		ConnectedSourcePtr.Reset();
	}

	bIsConnected = false;

	UE_LOG(LogTemp, Warning, TEXT("Dobot Settings: Disconnected from robot"));
}