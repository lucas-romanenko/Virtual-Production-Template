#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CineCameraComponent.h"
#include "DobotLiveLinkCameraComponent.generated.h"

class FDobotLiveLinkSource;

UENUM(BlueprintType)
enum class EDobotConnectionState : uint8
{
	NoConnection		UMETA(DisplayName = "No Connection"),
	Connected			UMETA(DisplayName = "Connected"),
	ConnectionLost		UMETA(DisplayName = "Connection Lost"),
	Reconnecting		UMETA(DisplayName = "Reconnecting")
};

UCLASS(ClassGroup = (LiveLink), meta = (BlueprintSpawnableComponent))
class DOBOTLIVELINK_API UDobotLiveLinkCameraComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDobotLiveLinkCameraComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void BeginPlay() override;

	// ---- LiveLink ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LiveLink")
	FName LiveLinkSubjectName = TEXT("DobotCamera");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LiveLink")
	UCineCameraComponent* CameraToControl;

	// ---- FreeD Source ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FreeD Source")
	bool bHasRobotConnection = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FreeD Source", meta = (EditCondition = "bHasRobotConnection"))
	FString RobotIPAddress = TEXT("192.168.5.1");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FreeD Source", meta = (EditCondition = "bHasRobotConnection", ClampMin = "1", ClampMax = "65535"))
	int32 RobotPort = 40000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FreeD Source", meta = (EditCondition = "bHasRobotConnection", ClampMin = "0.0", ClampMax = "10000.0"))
	float TrackingDelayMs = 0.0f;

	// ---- Tracking ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracking")
	bool bEnableTracking = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracking")
	bool bShowDebugInfo = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracking")
	bool bFreezeTracking = false;

	// ---- Connection ----

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "FreeD Source")
	bool ConnectToRobot();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "FreeD Source")
	void DisconnectFromRobot();

	UFUNCTION(BlueprintCallable, Category = "FreeD Source")
	bool IsRobotConnected() const { return bIsRobotConnected; }

	UFUNCTION(BlueprintCallable, Category = "FreeD Source")
	EDobotConnectionState GetConnectionState() const;

	TSharedPtr<FDobotLiveLinkSource> GetConnectedSource() const { return ConnectedSource; }

	void FindCamera();
	void ResetTrackingOrigin();

protected:
	virtual bool ShouldCreateRenderState() const override { return true; }

private:
	void CleanupDeadSource();
	void AttemptReconnect();

	bool bHasRecordedStart = false;
	FTransform CameraStartTransform;
	FTransform RobotStartTransform;

	bool bIsRobotConnected = false;
	TSharedPtr<FDobotLiveLinkSource> ConnectedSource;

	bool bIsReconnecting = false;
	float ReconnectTimer = 0.0f;
	float ReconnectLogTimer = 0.0f;
	static constexpr float ReconnectInterval = 3.0f;
	static constexpr float ReconnectLogInterval = 15.0f;
	bool bHasAttemptedStartupAutoConnect = false;
};