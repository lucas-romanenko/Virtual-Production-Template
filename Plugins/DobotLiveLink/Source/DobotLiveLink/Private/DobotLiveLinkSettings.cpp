#include "DobotLiveLinkSettings.h"
#include "DobotLiveLinkCameraComponent.h"
#include "CineCameraComponent.h"
#include "CineCameraActor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "MediaCapture.h"
#include "MediaOutput.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#endif

UDobotLiveLinkSettings::UDobotLiveLinkSettings()
	: FocalLength(24.0f)
	, Aperture(2.8f)
	, SensorWidth(36.0f)
	, SensorHeight(24.0f)
	, NumOutputPorts(1)
{
	OutputPortCameraAssignments.SetNum(NumOutputPorts);
	OutputPortAssetPaths.SetNum(NumOutputPorts);
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

// ---- Auto-Connect ----

bool UDobotLiveLinkSettings::ShouldAutoConnect(const FString& SubjectName) const
{
	return AutoConnectSubjects.Contains(SubjectName);
}

void UDobotLiveLinkSettings::SetAutoConnect(const FString& SubjectName, bool bEnable)
{
	if (bEnable)
	{
		AutoConnectSubjects.AddUnique(SubjectName);
	}
	else
	{
		AutoConnectSubjects.Remove(SubjectName);
	}
	SaveConfig();
}

// ---- Subject Name Helpers ----

FString UDobotLiveLinkSettings::GetNextAvailableSubjectName() const
{
	TArray<ACineCameraActor*> Cameras = FindAllDobotCameras();

	TSet<FString> UsedNames;
	for (ACineCameraActor* Cam : Cameras)
	{
		UDobotLiveLinkCameraComponent* Comp = Cam->FindComponentByClass<UDobotLiveLinkCameraComponent>();
		if (Comp)
		{
			UsedNames.Add(Comp->LiveLinkSubjectName.ToString());
		}
	}

	if (!UsedNames.Contains(TEXT("DobotCamera")))
	{
		return TEXT("DobotCamera");
	}

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

// ---- Spawn Camera ----

ACineCameraActor* UDobotLiveLinkSettings::SpawnDobotCamera()
{
	UWorld* World = GetEditorWorld();
	if (!World) return nullptr;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = MakeUniqueObjectName(World, ACineCameraActor::StaticClass(), FName(TEXT("DobotTrackedCamera")));
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ACineCameraActor* NewCamera = World->SpawnActor<ACineCameraActor>(ACineCameraActor::StaticClass(), FTransform::Identity, SpawnParams);
	if (!NewCamera) return nullptr;

	FString NextSubjectName = GetNextAvailableSubjectName();
	NewCamera->SetActorLabel(NextSubjectName);

	UDobotLiveLinkCameraComponent* DobotComp = NewObject<UDobotLiveLinkCameraComponent>(NewCamera, UDobotLiveLinkCameraComponent::StaticClass(), FName(TEXT("DobotLiveLinkCamera")));
	if (DobotComp)
	{
		DobotComp->RegisterComponent();
		NewCamera->AddInstanceComponent(DobotComp);
		DobotComp->LiveLinkSubjectName = FName(*NextSubjectName);
	}

	SetSelectedCamera(NewCamera);

	UE_LOG(LogTemp, Warning, TEXT("LiveLink Settings: Spawned new tracked camera '%s'"), *NextSubjectName);

	return NewCamera;
}

// ---- DeckLink Output Port Routing ----

void UDobotLiveLinkSettings::SetNumOutputPorts(int32 Num)
{
	Num = FMath::Clamp(Num, 1, 8);

	// Stop any active outputs on ports being removed
	for (int32 i = Num; i < NumOutputPorts; i++)
	{
		StopPortOutput(i);
	}

	NumOutputPorts = Num;
	OutputPortCameraAssignments.SetNum(Num);
	OutputPortAssetPaths.SetNum(Num);
	ActiveCaptures.SetNum(Num);

	SaveConfig();
}

void UDobotLiveLinkSettings::SetPortCamera(int32 PortIndex, const FString& SubjectName)
{
	if (!OutputPortCameraAssignments.IsValidIndex(PortIndex)) return;

	// Stop output if changing camera while active
	if (IsPortActive(PortIndex))
	{
		StopPortOutput(PortIndex);
	}

	OutputPortCameraAssignments[PortIndex] = SubjectName;
	SaveConfig();
}

FString UDobotLiveLinkSettings::GetPortCamera(int32 PortIndex) const
{
	if (OutputPortCameraAssignments.IsValidIndex(PortIndex))
	{
		return OutputPortCameraAssignments[PortIndex];
	}
	return FString();
}

TArray<UMediaOutput*> UDobotLiveLinkSettings::FindAllMediaOutputAssets() const
{
	TArray<UMediaOutput*> Result;

#if WITH_EDITOR
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/BlackmagicMediaOutput"), TEXT("BlackmagicMediaOutput")), AssetDataList);

	for (const FAssetData& AssetData : AssetDataList)
	{
		UMediaOutput* Output = Cast<UMediaOutput>(AssetData.GetAsset());
		if (Output)
		{
			Result.Add(Output);
		}
	}
#endif

	return Result;
}

UMediaOutput* UDobotLiveLinkSettings::GetOutputAssetForPort(int32 PortIndex) const
{
	if (!OutputPortAssetPaths.IsValidIndex(PortIndex)) return nullptr;

	const FString& AssetPath = OutputPortAssetPaths[PortIndex];
	if (AssetPath.IsEmpty()) return nullptr;

	return Cast<UMediaOutput>(StaticLoadObject(UMediaOutput::StaticClass(), nullptr, *AssetPath));
}

UMediaOutput* UDobotLiveLinkSettings::CreateOutputAssetForPort(int32 PortIndex)
{
#if WITH_EDITOR
	if (!OutputPortAssetPaths.IsValidIndex(PortIndex)) return nullptr;

	// Find the BlackmagicMediaOutput class
	UClass* BMDOutputClass = LoadClass<UMediaOutput>(nullptr, TEXT("/Script/BlackmagicMediaOutput.BlackmagicMediaOutput"));
	if (!BMDOutputClass)
	{
		UE_LOG(LogTemp, Error, TEXT("LiveLink Settings: BlackmagicMediaOutput class not found. Is the Blackmagic plugin enabled?"));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("Blackmagic plugin not found. Enable it in Edit > Plugins."));
		}
		return nullptr;
	}

	FString AssetName = FString::Printf(TEXT("BMD_Output_Port%d"), PortIndex + 1);
	FString PackagePath = TEXT("/Game/DobotLiveLink");
	FString FullPath = PackagePath / AssetName;

	UPackage* Package = CreatePackage(*FullPath);
	if (!Package) return nullptr;

	UMediaOutput* NewOutput = NewObject<UMediaOutput>(Package, BMDOutputClass, FName(*AssetName), RF_Public | RF_Standalone);
	if (!NewOutput) return nullptr;

	FAssetRegistryModule::AssetCreated(NewOutput);
	NewOutput->MarkPackageDirty();

	// Save the asset
	FString FilePath = FPackageName::LongPackageNameToFilename(FullPath, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(Package, NewOutput, *FilePath, SaveArgs);

	// Store path
	OutputPortAssetPaths[PortIndex] = NewOutput->GetPathName();
	SaveConfig();

	UE_LOG(LogTemp, Warning, TEXT("LiveLink Settings: Created output asset '%s' for Port %d"), *AssetName, PortIndex + 1);

	return NewOutput;
#else
	return nullptr;
#endif
}

bool UDobotLiveLinkSettings::StartPortOutput(int32 PortIndex)
{
	if (!OutputPortCameraAssignments.IsValidIndex(PortIndex)) return false;
	if (IsPortActive(PortIndex)) return true;

	// Ensure ActiveCaptures array is sized
	if (ActiveCaptures.Num() <= PortIndex)
	{
		ActiveCaptures.SetNum(PortIndex + 1);
	}

	UMediaOutput* MediaOutput = GetOutputAssetForPort(PortIndex);
	if (!MediaOutput)
	{
		UE_LOG(LogTemp, Error, TEXT("LiveLink Settings: No output asset configured for Port %d"), PortIndex + 1);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red,
				FString::Printf(TEXT("No output asset for Port %d. Click 'Create' first."), PortIndex + 1));
		}
		return false;
	}

	UMediaCapture* Capture = MediaOutput->CreateMediaCapture();
	if (!Capture)
	{
		UE_LOG(LogTemp, Error, TEXT("LiveLink Settings: Failed to create media capture for Port %d"), PortIndex + 1);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red,
				FString::Printf(TEXT("Failed to start output on Port %d. Check DeckLink configuration."), PortIndex + 1));
		}
		return false;
	}

	FMediaCaptureOptions CaptureOptions;
	Capture->CaptureActiveSceneViewport(CaptureOptions);

	ActiveCaptures[PortIndex] = Capture;

	UE_LOG(LogTemp, Warning, TEXT("LiveLink Settings: Started output on Port %d"), PortIndex + 1);
	return true;
}

void UDobotLiveLinkSettings::StopPortOutput(int32 PortIndex)
{
	if (!ActiveCaptures.IsValidIndex(PortIndex)) return;

	if (ActiveCaptures[PortIndex])
	{
		ActiveCaptures[PortIndex]->StopCapture(false);
		ActiveCaptures[PortIndex] = nullptr;
	}

	UE_LOG(LogTemp, Warning, TEXT("LiveLink Settings: Stopped output on Port %d"), PortIndex + 1);
}

bool UDobotLiveLinkSettings::IsPortActive(int32 PortIndex) const
{
	if (!ActiveCaptures.IsValidIndex(PortIndex)) return false;
	return ActiveCaptures[PortIndex] != nullptr;
}

void UDobotLiveLinkSettings::StopAllOutputs()
{
	for (int32 i = 0; i < ActiveCaptures.Num(); i++)
	{
		StopPortOutput(i);
	}
}