#include "DobotLiveLinkSettings.h"
#include "DobotLiveLinkCameraComponent.h"
#include "CineCameraComponent.h"
#include "CineCameraActor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
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
	, bOutputActive(false)
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
	ApplyCameraSettings();
}
#endif

static UWorld* GetEditorWorld()
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
		LoadCameraSettings();
	}
}

ACineCameraActor* UDobotLiveLinkSettings::GetSelectedCamera() const
{
	return SelectedCamera.Get();
}

UDobotLiveLinkCameraComponent* UDobotLiveLinkSettings::GetSelectedDobotComponent() const
{
	ACineCameraActor* Camera = SelectedCamera.Get();
	if (!Camera) return nullptr;
	return Camera->FindComponentByClass<UDobotLiveLinkCameraComponent>();
}

void UDobotLiveLinkSettings::ApplyCameraSettings()
{
	ACineCameraActor* Camera = SelectedCamera.Get();
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
}

void UDobotLiveLinkSettings::LoadCameraSettings()
{
	ACineCameraActor* Camera = SelectedCamera.Get();
	if (!Camera) return;

	UCineCameraComponent* CineComp = Camera->GetCineCameraComponent();
	if (CineComp)
	{
		FocalLength = CineComp->CurrentFocalLength;
		Aperture = CineComp->CurrentAperture;
		SensorWidth = CineComp->Filmback.SensorWidth;
		SensorHeight = CineComp->Filmback.SensorHeight;
	}
}

bool UDobotLiveLinkSettings::StartDeckLinkOutput()
{
	UWorld* World = GetEditorWorld();
	if (!World) return false;

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

ACineCameraActor* UDobotLiveLinkSettings::SpawnDobotCamera()
{
	UWorld* World = GetEditorWorld();
	if (!World) return nullptr;

	// Spawn a CineCameraActor at origin
	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = MakeUniqueObjectName(World, ACineCameraActor::StaticClass(), FName(TEXT("DobotTrackedCamera")));
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ACineCameraActor* NewCamera = World->SpawnActor<ACineCameraActor>(ACineCameraActor::StaticClass(), FTransform::Identity, SpawnParams);
	if (!NewCamera) return nullptr;

	// Set a nice label
	TArray<ACineCameraActor*> ExistingCameras = FindAllDobotCameras();
	int32 CameraNum = ExistingCameras.Num() + 1;
	FString Label = FString::Printf(TEXT("DobotCamera_%d"), CameraNum);
	NewCamera->SetActorLabel(Label);

	// Add DobotLiveLinkCamera component
	UDobotLiveLinkCameraComponent* DobotComp = NewObject<UDobotLiveLinkCameraComponent>(NewCamera, UDobotLiveLinkCameraComponent::StaticClass(), FName(TEXT("DobotLiveLinkCamera")));
	if (DobotComp)
	{
		DobotComp->RegisterComponent();
		NewCamera->AddInstanceComponent(DobotComp);

		// Set subject name based on camera number
		if (CameraNum == 1)
		{
			DobotComp->LiveLinkSubjectName = FName(TEXT("DobotCamera"));
		}
		else
		{
			DobotComp->LiveLinkSubjectName = FName(*FString::Printf(TEXT("DobotCamera%d"), CameraNum));
		}
	}

	// Select the new camera
	SetSelectedCamera(NewCamera);

	UE_LOG(LogTemp, Warning, TEXT("Dobot Settings: Spawned new tracked camera '%s'"), *Label);

	return NewCamera;
}