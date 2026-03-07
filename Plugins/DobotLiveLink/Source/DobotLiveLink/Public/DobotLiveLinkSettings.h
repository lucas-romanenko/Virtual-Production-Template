#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "DobotLiveLinkSettings.generated.h"

class ACineCameraActor;
class UMediaCapture;
class UMediaOutput;
class UDobotLiveLinkCameraComponent;

UCLASS(config = DobotLiveLink, defaultconfig)
class DOBOTLIVELINK_API UDobotLiveLinkSettings : public UObject
{
	GENERATED_BODY()

public:
	UDobotLiveLinkSettings();

	static UDobotLiveLinkSettings* Get();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** Push camera settings to selected camera */
	void ApplyCameraSettings();

	/** Load camera settings from selected camera */
	void LoadCameraSettings();

	/** Get all cameras with DobotLiveLinkCamera component in the level */
	TArray<ACineCameraActor*> FindAllDobotCameras() const;

	/** Set which camera to control */
	void SetSelectedCamera(ACineCameraActor* Camera);

	/** Get the currently selected camera */
	ACineCameraActor* GetSelectedCamera() const;

	/** Get the DobotLiveLinkCamera component from the selected camera */
	UDobotLiveLinkCameraComponent* GetSelectedDobotComponent() const;

	/** Spawn a new CineCameraActor with DobotLiveLinkCamera component */
	ACineCameraActor* SpawnDobotCamera();

	/** Get next available subject name */
	FString GetNextAvailableSubjectName() const;

	/** Check if a subject name is available */
	bool IsSubjectNameAvailable(const FString& Name, const UDobotLiveLinkCameraComponent* ExcludeComp = nullptr) const;

	/** Auto-connect management */
	bool ShouldAutoConnect(const FString& SubjectName) const;
	void SetAutoConnect(const FString& SubjectName, bool bEnable);

	// ---- DeckLink Output Port Routing ----

	/** Set number of output ports */
	void SetNumOutputPorts(int32 Num);
	int32 GetNumOutputPorts() const { return NumOutputPorts; }

	/** Assign a camera (by subject name) to a port */
	void SetPortCamera(int32 PortIndex, const FString& SubjectName);
	FString GetPortCamera(int32 PortIndex) const;

	/** Start/Stop output for a specific port */
	bool StartPortOutput(int32 PortIndex);
	void StopPortOutput(int32 PortIndex);
	bool IsPortActive(int32 PortIndex) const;

	/** Stop all outputs */
	void StopAllOutputs();

	/** Find all BlackmagicMediaOutput assets in the project */
	TArray<UMediaOutput*> FindAllMediaOutputAssets() const;

	/** Create a new BlackmagicMediaOutput asset for a port */
	UMediaOutput* CreateOutputAssetForPort(int32 PortIndex);

	/** Get or create the output asset for a port */
	UMediaOutput* GetOutputAssetForPort(int32 PortIndex) const;

	// ---- Camera Settings (synced to selected camera) ----

	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Camera Settings", meta = (DisplayName = "Focal Length (mm)", ClampMin = "4.0", ClampMax = "1000.0"))
	float FocalLength;

	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Camera Settings", meta = (DisplayName = "Aperture (f-stop)", ClampMin = "1.0", ClampMax = "32.0"))
	float Aperture;

	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Camera Settings", meta = (DisplayName = "Sensor Width (mm)", ClampMin = "1.0", ClampMax = "100.0"))
	float SensorWidth;

	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Camera Settings", meta = (DisplayName = "Sensor Height (mm)", ClampMin = "1.0", ClampMax = "100.0"))
	float SensorHeight;

	// ---- Persisted Config ----

	UPROPERTY(config)
	TArray<FString> AutoConnectSubjects;

	UPROPERTY(config)
	int32 NumOutputPorts;

	UPROPERTY(config)
	TArray<FString> OutputPortCameraAssignments;

	UPROPERTY(config)
	TArray<FString> OutputPortAssetPaths;

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<ACineCameraActor> SelectedCamera;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMediaCapture>> ActiveCaptures;
};