#pragma once

#include "CoreMinimal.h"
#include "ILiveLinkSource.h"
#include "HAL/Runnable.h"
#include "HAL/ThreadSafeBool.h"
#include "Networking.h"
#include "Roles/LiveLinkCameraRole.h"
#include "Roles/LiveLinkCameraTypes.h"

struct FTimestampedTransform
{
	double Timestamp;
	FTransform Transform;

	FTimestampedTransform() : Timestamp(0.0), Transform(FTransform::Identity) {}
	FTimestampedTransform(double InTime, const FTransform& InTransform) : Timestamp(InTime), Transform(InTransform) {}
};

class DOBOTLIVELINK_API FDobotLiveLinkSource : public ILiveLinkSource, public FRunnable
{
public:
	// Port is now the LOCAL UDP port we listen on (default 40000, Lensmaster default)
	FDobotLiveLinkSource(FString InIPAddress, int32 InPort, float InDelayMs = 0.0f, FString InSubjectName = TEXT("DobotCamera"));
	virtual ~FDobotLiveLinkSource();

	// ILiveLinkSource interface
	virtual void ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid) override;
	virtual bool IsSourceStillValid() const override;
	virtual bool RequestSourceShutdown() override;
	virtual FText GetSourceType() const override;
	virtual FText GetSourceMachineName() const override;
	virtual FText GetSourceStatus() const override;

	// FRunnable interface
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;

private:
	void UpdateLiveMode();
	bool ParseFreeDPacket(const TArray<uint8>& PacketData);
	bool GetDelayedTransform(FTransform& OutTransform) const;

	// Decode a 3-byte big-endian signed value from FreeD packet
	static int32 DecodeFreeDInt24(const uint8* Data);

	ILiveLinkClient* Client;
	FGuid SourceGuid;

	FString IPAddress;   // Kept for display / source machine name
	int32 Port;          // Local UDP listen port
	float DelayMs;
	FName SubjectName;

	FSocket* Socket;
	FRunnableThread* Thread;
	FThreadSafeBool bIsRunning;

	FTransform CurrentTransform;

	// Lens data decoded from FreeD packet
	float CurrentFocalLength;    // mm
	float CurrentAperture;       // f-stop
	float CurrentFocusDistance;  // cm (UE units)

	// Timeout: if no packet received within this many seconds, mark source as dead
	double LastPacketTime;
	static constexpr double PacketTimeoutSeconds = 5.0;

	TArray<FTimestampedTransform> TransformBuffer;
	static const int32 MaxBufferSize = 2048;

	// FreeD D1 packet constants
	static constexpr int32 FreeDPacketSize = 29;
	static constexpr uint8  FreeDMessageType = 0xD1;

	// Lens mapping: raw FreeD encoder range -> real units
	// These match what you configure in Lensmaster's lens data table.
	// Zoom raw range 0..16383 maps to FocalLengthMin..FocalLengthMax (mm)
	// Focus raw range 0..16383 maps to FocusDistanceMin..FocusDistanceMax (cm)
	float FocalLengthMin;
	float FocalLengthMax;
	float FocusDistanceMin;  // cm
	float FocusDistanceMax;  // cm
};