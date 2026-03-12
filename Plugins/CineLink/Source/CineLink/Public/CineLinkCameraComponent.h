#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CineLinkCameraComponent.generated.h"

UENUM(BlueprintType)
enum class ECineLinkConnectionState : uint8
{
	NoConnection	UMETA(DisplayName = "No Connection"),
	Connected		UMETA(DisplayName = "Connected"),
	ConnectionLost	UMETA(DisplayName = "Connection Lost"),
	Reconnecting	UMETA(DisplayName = "Reconnecting")
};

/**
 * Add this component to a CineCameraActor to bind it to a FreeD LiveLink source.
 * Each camera manages its own FreeD connection (IP, port, subject name).
 * Supports multiple cameras with independent FreeD streams simultaneously.
 */
UCLASS(ClassGroup = (CineLink), meta = (BlueprintSpawnableComponent), DisplayName = "CineLink Camera")
class CINELINK_API UCineLinkCameraComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCineLinkCameraComponent();

	// ---- Per-Camera LiveLink Connection Settings ----

	// IP address that Lensmaster is broadcasting FreeD to (this PC's IP)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CineLink")
	FString LensmasterIP = TEXT("192.168.5.30");

	// UDP port Lensmaster broadcasts FreeD on
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CineLink", meta = (ClampMin = "1", ClampMax = "65535"))
	int32 LensmasterPort = 40000;

	// LiveLink subject name produced by the FreeD plugin (e.g. "Camera 1")
	// Must match what appears in the LiveLink window
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CineLink")
	FName LiveLinkSubjectName = TEXT("Camera 1");

	// Automatically create the FreeD source when the CineLink panel opens
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CineLink")
	bool bAutoConnect = true;

	// ---- Connection Control ----

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "CineLink")
	bool Connect();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "CineLink")
	void Disconnect();

	UFUNCTION(BlueprintCallable, Category = "CineLink")
	bool IsConnected() const { return bIsConnected; }

	UFUNCTION(BlueprintCallable, Category = "CineLink")
	ECineLinkConnectionState GetConnectionState() const;

	// Returns true if a FreeD source for our port already exists in LiveLink
	bool HasExistingFreeDSource() const;

private:
	bool CreateFreeDSource();
	void CleanupSource();
	bool IsSubjectActive() const;

	bool bIsConnected = false;
	FGuid ActiveSourceGuid;
};