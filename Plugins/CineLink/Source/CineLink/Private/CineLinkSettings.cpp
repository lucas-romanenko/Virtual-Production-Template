#include "CineLinkSettings.h"
#include "CineLinkCameraComponent.h"
#include "CineCameraComponent.h"
#include "CineCameraActor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "MediaCapture.h"
#include "MediaOutput.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "LiveLinkComponentController.h"
#include "Roles/LiveLinkCameraRole.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

UCineLinkSettings::UCineLinkSettings()
{
	OutputPortCameraAssignments.SetNum(NumOutputPorts);
	OutputPortAssetPaths.SetNum(NumOutputPorts);
}

UCineLinkSettings* UCineLinkSettings::Get()
{
	return GetMutableDefault<UCineLinkSettings>();
}

#if WITH_EDITOR
void UCineLinkSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	ApplyCameraSettings();
}
#endif

static UWorld* GetEditorWorld()
{
#if WITH_EDITOR
	if (GEditor) return GEditor->GetEditorWorldContext().World();
#endif
	return nullptr;
}

// ---- Camera Management ----

TArray<ACineCameraActor*> UCineLinkSettings::FindAllCineLinkCameras() const
{
	TArray<ACineCameraActor*> Result;
	UWorld* World = GetEditorWorld();
	if (!World) return Result;

	// Find any actor with UCineLinkCameraComponent — includes BP subclasses of CineCameraActor
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor->FindComponentByClass<UCineLinkCameraComponent>())
		{
			// Cast to CineCameraActor — BP_VirtualProductionStage inherits from it
			if (ACineCameraActor* CineCam = Cast<ACineCameraActor>(Actor))
			{
				Result.Add(CineCam);
			}
		}
	}
	return Result;
}

void UCineLinkSettings::SetSelectedCamera(ACineCameraActor* Camera)
{
	SelectedCamera = Camera;
	if (Camera) LoadCameraSettings();
}

ACineCameraActor* UCineLinkSettings::GetSelectedCamera() const
{
	return SelectedCamera.Get();
}

UCineLinkCameraComponent* UCineLinkSettings::GetSelectedCineLinkComponent() const
{
	ACineCameraActor* Cam = SelectedCamera.Get();
	if (!Cam) return nullptr;
	return Cam->FindComponentByClass<UCineLinkCameraComponent>();
}

void UCineLinkSettings::ApplyCameraSettings()
{
	ACineCameraActor* Cam = SelectedCamera.Get();
	if (!Cam) return;
	UCineCameraComponent* CineComp = Cam->GetCineCameraComponent();
	if (!CineComp) return;

	CineComp->CurrentFocalLength = FocalLength;
	CineComp->CurrentAperture = Aperture;
	CineComp->Filmback.SensorWidth = SensorWidth;
	CineComp->Filmback.SensorHeight = SensorHeight;
}

void UCineLinkSettings::LoadCameraSettings()
{
	ACineCameraActor* Cam = SelectedCamera.Get();
	if (!Cam) return;
	UCineCameraComponent* CineComp = Cam->GetCineCameraComponent();
	if (!CineComp) return;

	FocalLength = CineComp->CurrentFocalLength;
	Aperture = CineComp->CurrentAperture;
	SensorWidth = CineComp->Filmback.SensorWidth;
	SensorHeight = CineComp->Filmback.SensorHeight;
}

ACineCameraActor* UCineLinkSettings::SpawnCineLinkCamera()
{
	UWorld* World = GetEditorWorld();
	if (!World) return nullptr;

	// Find next available camera number
	TArray<ACineCameraActor*> Existing = FindAllCineLinkCameras();
	int32 NextNum = Existing.Num() + 1;
	FString CamName = FString::Printf(TEXT("CineLink_Camera_%d"), NextNum);
	FName SubjectName = FName(*FString::Printf(TEXT("Camera %d"), NextNum));

	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = MakeUniqueObjectName(World, ACineCameraActor::StaticClass(), FName(*CamName));
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ACineCameraActor* NewCam = World->SpawnActor<ACineCameraActor>(
		ACineCameraActor::StaticClass(), FTransform::Identity, SpawnParams);
	if (!NewCam) return nullptr;

	NewCam->SetActorLabel(CamName);

	// 1. Sony FX6 sensor defaults on the CineCameraComponent
	if (UCineCameraComponent* CineComp = NewCam->GetCineCameraComponent())
	{
		CineComp->CurrentFocalLength = 24.0f;
		CineComp->CurrentAperture = 2.8f;
		CineComp->Filmback.SensorWidth = 35.9f;
		CineComp->Filmback.SensorHeight = 24.0f;
	}

	// 2. CineLinkCameraComponent — stores IP/port/subject name
	UCineLinkCameraComponent* CineLinkComp = NewObject<UCineLinkCameraComponent>(
		NewCam, UCineLinkCameraComponent::StaticClass(), FName(TEXT("CineLinkCamera")));
	if (CineLinkComp)
	{
		CineLinkComp->LiveLinkSubjectName = SubjectName;
		CineLinkComp->RegisterComponent();
		NewCam->AddInstanceComponent(CineLinkComp);
	}

	// 3. LiveLinkComponentController — drives the camera transform from LiveLink data
	ULiveLinkComponentController* LiveLinkCtrl = NewObject<ULiveLinkComponentController>(
		NewCam, ULiveLinkComponentController::StaticClass(), FName(TEXT("LiveLinkController")));
	if (LiveLinkCtrl)
	{
		// Set the subject to match our CineLink component's subject name
		FLiveLinkSubjectRepresentation SubjectRep;
		SubjectRep.Subject = SubjectName;
		SubjectRep.Role = ULiveLinkCameraRole::StaticClass();
		LiveLinkCtrl->SetSubjectRepresentation(SubjectRep);

		LiveLinkCtrl->RegisterComponent();
		NewCam->AddInstanceComponent(LiveLinkCtrl);
	}

	SetSelectedCamera(NewCam);
	UE_LOG(LogTemp, Warning, TEXT("CineLink: Spawned '%s' with LiveLink subject '%s'"),
		*CamName, *SubjectName.ToString());
	return NewCam;
}

// ---- DeckLink Output ----

void UCineLinkSettings::SetNumOutputPorts(int32 Num)
{
	Num = FMath::Clamp(Num, 1, 8);
	for (int32 i = Num; i < NumOutputPorts; i++) StopPortOutput(i);
	NumOutputPorts = Num;
	OutputPortCameraAssignments.SetNum(Num);
	OutputPortAssetPaths.SetNum(Num);
	ActiveCaptures.SetNum(Num);
	SaveConfig();
}

void UCineLinkSettings::SetPortCamera(int32 PortIndex, const FString& CameraName)
{
	if (!OutputPortCameraAssignments.IsValidIndex(PortIndex)) return;
	if (IsPortActive(PortIndex)) StopPortOutput(PortIndex);
	OutputPortCameraAssignments[PortIndex] = CameraName;
	SaveConfig();
}

FString UCineLinkSettings::GetPortCamera(int32 PortIndex) const
{
	if (OutputPortCameraAssignments.IsValidIndex(PortIndex))
		return OutputPortCameraAssignments[PortIndex];
	return FString();
}

TArray<UMediaOutput*> UCineLinkSettings::FindAllMediaOutputAssets() const
{
	TArray<UMediaOutput*> Result;
#if WITH_EDITOR
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssetsByClass(
		FTopLevelAssetPath(TEXT("/Script/BlackmagicMediaOutput"), TEXT("BlackmagicMediaOutput")), AssetDataList);
	for (const FAssetData& AssetData : AssetDataList)
	{
		if (UMediaOutput* Output = Cast<UMediaOutput>(AssetData.GetAsset()))
			Result.Add(Output);
	}
#endif
	return Result;
}

UMediaOutput* UCineLinkSettings::GetOutputAssetForPort(int32 PortIndex) const
{
	if (!OutputPortAssetPaths.IsValidIndex(PortIndex)) return nullptr;
	const FString& Path = OutputPortAssetPaths[PortIndex];
	if (Path.IsEmpty()) return nullptr;
	return Cast<UMediaOutput>(StaticLoadObject(UMediaOutput::StaticClass(), nullptr, *Path));
}

UMediaOutput* UCineLinkSettings::CreateOutputAssetForPort(int32 PortIndex)
{
#if WITH_EDITOR
	if (!OutputPortAssetPaths.IsValidIndex(PortIndex)) return nullptr;

	UClass* BMDClass = LoadClass<UMediaOutput>(
		nullptr, TEXT("/Script/BlackmagicMediaOutput.BlackmagicMediaOutput"));
	if (!BMDClass)
	{
		UE_LOG(LogTemp, Error, TEXT("CineLink: BlackmagicMediaOutput class not found. Enable the Blackmagic plugin."));
		return nullptr;
	}

	FString AssetName = FString::Printf(TEXT("BMD_Output_Port%d"), PortIndex + 1);
	FString PackagePath = TEXT("/Game/CineLink");
	FString FullPath = PackagePath / AssetName;

	UPackage* Package = CreatePackage(*FullPath);
	if (!Package) return nullptr;

	UMediaOutput* NewOutput = NewObject<UMediaOutput>(Package, BMDClass, FName(*AssetName), RF_Public | RF_Standalone);
	if (!NewOutput) return nullptr;

	FAssetRegistryModule::AssetCreated(NewOutput);
	NewOutput->MarkPackageDirty();

	FString FilePath = FPackageName::LongPackageNameToFilename(FullPath, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(Package, NewOutput, *FilePath, SaveArgs);

	OutputPortAssetPaths[PortIndex] = NewOutput->GetPathName();
	SaveConfig();
	return NewOutput;
#else
	return nullptr;
#endif
}

bool UCineLinkSettings::StartPortOutput(int32 PortIndex)
{
	if (!OutputPortCameraAssignments.IsValidIndex(PortIndex)) return false;
	if (IsPortActive(PortIndex)) return true;
	if (ActiveCaptures.Num() <= PortIndex) ActiveCaptures.SetNum(PortIndex + 1);

	UMediaOutput* MediaOutput = GetOutputAssetForPort(PortIndex);
	if (!MediaOutput)
	{
		UE_LOG(LogTemp, Error, TEXT("CineLink: No output asset for Port %d. Click 'Create Asset' first."), PortIndex + 1);
		return false;
	}

	UMediaCapture* Capture = MediaOutput->CreateMediaCapture();
	if (!Capture)
	{
		UE_LOG(LogTemp, Error, TEXT("CineLink: Failed to create media capture for Port %d"), PortIndex + 1);
		return false;
	}

	FMediaCaptureOptions CaptureOptions;
	Capture->CaptureActiveSceneViewport(CaptureOptions);
	ActiveCaptures[PortIndex] = Capture;

	UE_LOG(LogTemp, Warning, TEXT("CineLink: Started DeckLink output on Port %d"), PortIndex + 1);
	return true;
}

void UCineLinkSettings::StopPortOutput(int32 PortIndex)
{
	if (!ActiveCaptures.IsValidIndex(PortIndex)) return;
	if (ActiveCaptures[PortIndex])
	{
		ActiveCaptures[PortIndex]->StopCapture(false);
		ActiveCaptures[PortIndex] = nullptr;
	}
}

bool UCineLinkSettings::IsPortActive(int32 PortIndex) const
{
	if (!ActiveCaptures.IsValidIndex(PortIndex)) return false;
	return ActiveCaptures[PortIndex] != nullptr;
}

void UCineLinkSettings::StopAllOutputs()
{
	for (int32 i = 0; i < ActiveCaptures.Num(); i++) StopPortOutput(i);
}