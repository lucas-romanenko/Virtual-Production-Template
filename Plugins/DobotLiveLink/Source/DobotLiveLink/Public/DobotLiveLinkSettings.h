#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "DobotLiveLinkSettings.generated.h"

class ACineCameraActor;
class UMediaCapture;

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

	void ApplyToCamera();
	void LoadFromCamera();

	/** Create a LiveLink source and connect to the robot */
	bool ConnectToRobot();

	/** Disconnect the current LiveLink source */
	void DisconnectFromRobot();

	/** Check if currently connected */
	bool IsConnected() const { return bIsConnected; }

	/** Get all cameras with DobotLiveLinkCamera component in the level */
	TArray<ACineCameraActor*> FindAllDobotCameras() const;

	/** Set which camera to control */
	void SetSelectedCamera(ACineCameraActor* Camera);

	/** Get the currently selected camera */
	ACineCameraActor* GetSelectedCamera() const;

	/** Start DeckLink output from selected camera */
	bool StartDeckLinkOutput();

	/** Stop DeckLink output */
	void StopDeckLinkOutput();

	/** Check if DeckLink output is active */
	bool IsDeckLinkOutputActive() const { return bOutputActive; }

	/** Try to auto-connect using saved settings */
	void TryAutoConnect();

	// ---- Camera Settings ----

	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Camera Settings", meta = (DisplayName = "Focal Length (mm)", ClampMin = "4.0", ClampMax = "1000.0"))
	float FocalLength;

	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Camera Settings", meta = (DisplayName = "Aperture (f-stop)", ClampMin = "1.0", ClampMax = "32.0"))
	float Aperture;

	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Camera Settings", meta = (DisplayName = "Sensor Width (mm)", ClampMin = "1.0", ClampMax = "100.0"))
	float SensorWidth;

	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Camera Settings", meta = (DisplayName = "Sensor Height (mm)", ClampMin = "1.0", ClampMax = "100.0"))
	float SensorHeight;

	// ---- Tracking Settings ----

	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Tracking", meta = (DisplayName = "Robot IP Address"))
	FString RobotIPAddress;

	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Tracking", meta = (DisplayName = "Robot Port", ClampMin = "1", ClampMax = "65535"))
	int32 RobotPort;

	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Tracking", meta = (DisplayName = "Subject Name"))
	FString SubjectName;

	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Tracking", meta = (DisplayName = "Enable Tracking"))
	bool bEnableTracking;

	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Tracking", meta = (DisplayName = "Tracking Delay (ms)", ClampMin = "0.0", ClampMax = "10000.0"))
	float TrackingDelayMs;

	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Tracking", meta = (DisplayName = "Test Mode"))
	bool bTestMode;

	// ---- Output ----

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeckLink Output", meta = (DisplayName = "Output Active"))
	bool bOutputActive;

	/** Auto-connect to robot on startup using saved settings */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Tracking", meta = (DisplayName = "Auto-Connect on Startup"))
	bool bAutoConnect;

private:
	bool bIsConnected;
	TSharedPtr<class FDobotLiveLinkSource> ConnectedSourcePtr;

	UPROPERTY(Transient)
	TWeakObjectPtr<ACineCameraActor> SelectedCamera;

	UPROPERTY(Transient)
	TObjectPtr<UMediaCapture> ActiveMediaCapture;
};