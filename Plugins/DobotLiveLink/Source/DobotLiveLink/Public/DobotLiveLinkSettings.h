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

	bool ConnectToRobot();
	void DisconnectFromRobot();
	bool IsConnected() const { return bIsConnected; }

	TArray<ACineCameraActor*> FindAllDobotCameras() const;
	void SetSelectedCamera(ACineCameraActor* Camera);
	ACineCameraActor* GetSelectedCamera() const;

	bool StartDeckLinkOutput();
	void StopDeckLinkOutput();
	bool IsDeckLinkOutputActive() const { return bOutputActive; }

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

	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Tracking", meta = (DisplayName = "Auto-Connect on Startup"))
	bool bAutoConnect;

	// ---- Output ----

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeckLink Output", meta = (DisplayName = "Output Active"))
	bool bOutputActive;

private:
	bool bIsConnected;
	TSharedPtr<class FDobotLiveLinkSource> ConnectedSourcePtr;

	UPROPERTY(Transient)
	TWeakObjectPtr<ACineCameraActor> SelectedCamera;

	UPROPERTY(Transient)
	TObjectPtr<UMediaCapture> ActiveMediaCapture;
};