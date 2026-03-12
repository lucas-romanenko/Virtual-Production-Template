#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "CineLinkSettings.generated.h"

class ACineCameraActor;
class UCineLinkCameraComponent;
class UMediaOutput;
class UMediaCapture;

/**
 * Editor-only settings object for the CineLink panel.
 * Holds selected camera reference and DeckLink output configuration.
 * Per-camera LiveLink settings (IP, port, subject name) live on UCineLinkCameraComponent.
 */
UCLASS(config = CineLink, defaultconfig)
class CINELINK_API UCineLinkSettings : public UObject
{
	GENERATED_BODY()

public:
	UCineLinkSettings();

	static UCineLinkSettings* Get();

	// ---- Camera Management ----

	TArray<ACineCameraActor*> FindAllCineLinkCameras() const;
	ACineCameraActor* SpawnCineLinkCamera();

	void SetSelectedCamera(ACineCameraActor* Camera);
	ACineCameraActor* GetSelectedCamera() const;
	UCineLinkCameraComponent* GetSelectedCineLinkComponent() const;

	// ---- Camera Settings (applied to selected camera's CineCameraComponent) ----

	UPROPERTY(EditAnywhere, Category = "Camera Settings", meta = (ClampMin = "1.0"))
	float FocalLength = 24.0f;

	UPROPERTY(EditAnywhere, Category = "Camera Settings", meta = (ClampMin = "1.0"))
	float Aperture = 2.8f;

	UPROPERTY(EditAnywhere, Category = "Camera Settings")
	float SensorWidth = 35.9f;

	UPROPERTY(EditAnywhere, Category = "Camera Settings")
	float SensorHeight = 24.0f;

	void ApplyCameraSettings();
	void LoadCameraSettings();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// ---- DeckLink Output ----

	int32 GetNumOutputPorts() const { return NumOutputPorts; }
	void SetNumOutputPorts(int32 Num);

	void SetPortCamera(int32 PortIndex, const FString& SubjectName);
	FString GetPortCamera(int32 PortIndex) const;

	bool StartPortOutput(int32 PortIndex);
	void StopPortOutput(int32 PortIndex);
	bool IsPortActive(int32 PortIndex) const;
	void StopAllOutputs();

	UMediaOutput* GetOutputAssetForPort(int32 PortIndex) const;
	UMediaOutput* CreateOutputAssetForPort(int32 PortIndex);
	TArray<UMediaOutput*> FindAllMediaOutputAssets() const;

private:
	TWeakObjectPtr<ACineCameraActor> SelectedCamera;

	UPROPERTY(config)
	int32 NumOutputPorts = 1;

	UPROPERTY(config)
	TArray<FString> OutputPortCameraAssignments;

	UPROPERTY(config)
	TArray<FString> OutputPortAssetPaths;

	TArray<TObjectPtr<UMediaCapture>> ActiveCaptures;
};