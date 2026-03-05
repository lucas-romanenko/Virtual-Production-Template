#include "DobotLiveLinkSettings.h"
#include "DobotLiveLinkCameraComponent.h"
#include "DobotLiveLinkSource.h"
#include "CineCameraComponent.h"
#include "CineCameraActor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "ILiveLinkClient.h"
#include "Modules/ModuleManager.h"
#include "MediaCapture.h"
#include "MediaOutput.h"

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
	, bAutoConnect(false)
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

UWorld* GetEditorWorld()
{
#if WITH_EDITOR
	if (GEditor)
	{
		return GEditor->GetEditorWorldContext().World();
	}
#endif
	return nullptr;
}

TArray<ACineCameraActor*> UDobotLiveLinkSettings::FindAllDobotCameras() const
{
	TArray<ACineCameraActor*> Result;
	UWorld* World = GetEditorWorld();
	if (!World) return Result;

	for (TActorIterator<ACineCameraActor> It(World); It; ++It)
	{
		ACineCameraActor* CameraActor = *It;
		if (!CameraActor) continue;

		UDobotLiveLinkCameraComponent* DobotComp = CameraActor->FindComponentByClass<UDobotLiveLinkCameraComponent>();
		if (DobotComp)
		{
			Result.Add(CameraActor);
		}
	}

	return Result;
}

void UDobotLiveLinkSettings::SetSelectedCamera(ACineCameraActor* Camera)
{
	SelectedCamera = Camera;
	if (Camera)
	{
		LoadFromCamera();
	}
}

ACineCameraActor* UDobotLiveLinkSettings::GetSelectedCamera() const
{
	return SelectedCamera.Get();
}

void UDobotLiveLinkSettings::ApplyToCamera()
{
	ACineCameraActor* Camera = SelectedCamera.Get();
	if (!Camera)
	{
		// Fallback: find first camera
		TArray<ACineCameraActor*> Cameras = FindAllDobotCameras();
		if (Cameras.Num() > 0)
		{
			Camera = Cameras[0];
			SelectedCamera = Camera;
		}
	}

	if (!Camera) return;

	UCineCameraComponent* CineComp = Camera->GetCineCameraComponent();
	if (CineComp)
	{
		CineComp->CurrentFocalLength = FocalLength;
		CineComp->CurrentAperture = Aperture;

		FCameraFilmbackSettings Filmback = CineComp->Filmback;
		Filmback.SensorWidth = SensorWidth;
		Filmback.SensorHeight = SensorHeight;
		CineComp->Filmback = Filmback;
	}

	UDobotLiveLinkCameraComponent* DobotComp = Camera->FindComponentByClass<UDobotLiveLinkCameraComponent>();
	if (DobotComp)
	{
		DobotComp->LiveLinkSubjectName = FName(*SubjectName);
		DobotComp->bEnableTracking = bEnableTracking;
	}
}

void UDobotLiveLinkSettings::LoadFromCamera()
{
	ACineCameraActor* Camera = SelectedCamera.Get();
	if (!Camera)
	{
		TArray<ACineCameraActor*> Cameras = FindAllDobotCameras();
		if (Cameras.Num() > 0)
		{
			Camera = Cameras[0];
			SelectedCamera = Camera;
		}
	}

	if (!Camera) return;

	UCineCameraComponent* CineComp = Camera->GetCineCameraComponent();
	if (CineComp)
	{
		FocalLength = CineComp->CurrentFocalLength;
		Aperture = CineComp->CurrentAperture;
		SensorWidth = CineComp->Filmback.SensorWidth;
		SensorHeight = CineComp->Filmback.SensorHeight;
	}

	UDobotLiveLinkCameraComponent* DobotComp = Camera->FindComponentByClass<UDobotLiveLinkCameraComponent>();
	if (DobotComp)
	{
		SubjectName = DobotComp->LiveLinkSubjectName.ToString();
		bEnableTracking = DobotComp->bEnableTracking;
	}
}

bool UDobotLiveLinkSettings::ConnectToRobot()
{
	if (bIsConnected)
	{
		DisconnectFromRobot();
	}

	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	if (!ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
	{
		UE_LOG(LogTemp, Error, TEXT("Dobot Settings: LiveLink client not available"));
		return false;
	}

	ILiveLinkClient& LiveLinkClient = ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);

	TSharedPtr<FDobotLiveLinkSource> NewSource = MakeShared<FDobotLiveLinkSource>(
		RobotIPAddress, RobotPort, bTestMode, TrackingDelayMs, SubjectName);

	LiveLinkClient.AddSource(NewSource);

	bIsConnected = true;
	ConnectedSourcePtr = NewSource;

	UE_LOG(LogTemp, Warning, TEXT("Dobot Settings: Connected to %s:%d (Subject: %s)"),
		*RobotIPAddress, RobotPort, *SubjectName);

	// Save config so auto-connect works next time
	SaveConfig();

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

bool UDobotLiveLinkSettings::StartDeckLinkOutput()
{
	// Find the Blackmagic Media Output asset
	UWorld* World = GetEditorWorld();
	if (!World) return false;

	// Look for any MediaOutput asset that's loaded
	UMediaOutput* MediaOutput = nullptr;
	for (TObjectIterator<UMediaOutput> It; It; ++It)
	{
		if (It->GetName().Contains(TEXT("BMD")) || It->GetName().Contains(TEXT("Blackmagic")) || It->GetName().Contains(TEXT("LED")))
		{
			MediaOutput = *It;
			break;
		}
	}

	if (!MediaOutput)
	{
		UE_LOG(LogTemp, Error, TEXT("Dobot Settings: No Blackmagic Media Output asset found"));
		return false;
	}

	ActiveMediaCapture = MediaOutput->CreateMediaCapture();
	if (!ActiveMediaCapture)
	{
		UE_LOG(LogTemp, Error, TEXT("Dobot Settings: Failed to create media capture"));
		return false;
	}

	FMediaCaptureOptions CaptureOptions;
	ActiveMediaCapture->CaptureActiveSceneViewport(CaptureOptions);

	bOutputActive = true;
	UE_LOG(LogTemp, Warning, TEXT("Dobot Settings: DeckLink output started"));
	return true;
}

void UDobotLiveLinkSettings::StopDeckLinkOutput()
{
	if (ActiveMediaCapture)
	{
		ActiveMediaCapture->StopCapture(false);
		ActiveMediaCapture = nullptr;
	}

	bOutputActive = false;
	UE_LOG(LogTemp, Warning, TEXT("Dobot Settings: DeckLink output stopped"));
}

void UDobotLiveLinkSettings::TryAutoConnect()
{
	if (!bAutoConnect) return;
	if (bIsConnected) return;
	if (RobotIPAddress.IsEmpty()) return;

	UE_LOG(LogTemp, Warning, TEXT("Dobot Settings: Auto-connecting to %s:%d..."), *RobotIPAddress, RobotPort);
	ConnectToRobot();
}