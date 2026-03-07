#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "DobotLiveLinkSettings.generated.h"

class ACineCameraActor;
class UMediaCapture;
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

	/** DeckLink output */
	bool StartDeckLinkOutput();
	void StopDeckLinkOutput();
	bool IsDeckLinkOutputActive() const { return bOutputActive; }

	/** Spawn a new CineCameraActor with DobotLiveLinkCamera component in the level */
	ACineCameraActor* SpawnDobotCamera();

	/** Get next available subject name (DobotCamera, DobotCamera_2, etc.) */
	FString GetNextAvailableSubjectName() const;

	/** Check if a subject name is available (not used by another camera) */
	bool IsSubjectNameAvailable(const FString& Name, const UDobotLiveLinkCameraComponent* ExcludeComp = nullptr) const;

	// ---- Camera Settings (synced to selected camera) ----

	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Camera Settings", meta = (DisplayName = "Focal Length (mm)", ClampMin = "4.0", ClampMax = "1000.0"))
	float FocalLength;

	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Camera Settings", meta = (DisplayName = "Aperture (f-stop)", ClampMin = "1.0", ClampMax = "32.0"))
	float Aperture;

	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Camera Settings", meta = (DisplayName = "Sensor Width (mm)", ClampMin = "1.0", ClampMax = "100.0"))
	float SensorWidth;

	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Camera Settings", meta = (DisplayName = "Sensor Height (mm)", ClampMin = "1.0", ClampMax = "100.0"))
	float SensorHeight;

	// ---- Output ----

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeckLink Output", meta = (DisplayName = "Output Active"))
	bool bOutputActive;

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<ACineCameraActor> SelectedCamera;

	UPROPERTY(Transient)
	TObjectPtr<UMediaCapture> ActiveMediaCapture;
};