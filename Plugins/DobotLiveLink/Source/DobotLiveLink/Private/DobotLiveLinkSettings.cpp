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

FString UDobotLiveLinkSettings::GetNextAvailableSubjectName() const
{
	TArray<ACineCameraActor*> Cameras = FindAllDobotCameras();

	// Collect all existing subject names
	TSet<FString> UsedNames;
	for (ACineCameraActor* Cam : Cameras)
	{
		UDobotLiveLinkCameraComponent* Comp = Cam->FindComponentByClass<UDobotLiveLinkCameraComponent>();
		if (Comp)
		{
			UsedNames.Add(Comp->LiveLinkSubjectName.ToString());
		}
	}

	// First camera gets "DobotCamera"
	if (!UsedNames.Contains(TEXT("DobotCamera")))
	{
		return TEXT("DobotCamera");
	}

	// Find next available number
	for (int32 i = 2; i < 100; i++)
	{
		FString Candidate = FString::Printf(TEXT("DobotCamera_%d"), i);
		if (!UsedNames.Contains(Candidate))
		{
			return Candidate;
		}
	}

	return TEXT("DobotCamera_New");
}

bool UDobotLiveLinkSettings::IsSubjectNameAvailable(const FString& Name, const UDobotLiveLinkCameraComponent* ExcludeComp) const
{
	TArray<ACineCameraActor*> Cameras = FindAllDobotCameras();
	for (ACineCameraActor* Cam : Cameras)
	{
		UDobotLiveLinkCameraComponent* Comp = Cam->FindComponentByClass<UDobotLiveLinkCameraComponent>();
		if (Comp && Comp != ExcludeComp)
		{
			if (Comp->LiveLinkSubjectName.ToString() == Name)
			{
				return false;
			}
		}
	}
	return true;
}

ACineCameraActor* UDobotLiveLinkSettings::SpawnDobotCamera()
{
	UWorld* World = GetEditorWorld();
	if (!World) return nullptr;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = MakeUniqueObjectName(World, ACineCameraActor::StaticClass(), FName(TEXT("DobotTrackedCamera")));
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ACineCameraActor* NewCamera = World->SpawnActor<ACineCameraActor>(ACineCameraActor::StaticClass(), FTransform::Identity, SpawnParams);
	if (!NewCamera) return nullptr;

	// Get next available subject name
	FString NextSubjectName = GetNextAvailableSubjectName();

	// Set actor label to match subject name
	NewCamera->SetActorLabel(NextSubjectName);

	// Add DobotLiveLinkCamera component
	UDobotLiveLinkCameraComponent* DobotComp = NewObject<UDobotLiveLinkCameraComponent>(NewCamera, UDobotLiveLinkCameraComponent::StaticClass(), FName(TEXT("DobotLiveLinkCamera")));
	if (DobotComp)
	{
		DobotComp->RegisterComponent();
		NewCamera->AddInstanceComponent(DobotComp);
		DobotComp->LiveLinkSubjectName = FName(*NextSubjectName);
	}

	SetSelectedCamera(NewCamera);

	UE_LOG(LogTemp, Warning, TEXT("Dobot Settings: Spawned new tracked camera '%s'"), *NextSubjectName);

	return NewCamera;
}