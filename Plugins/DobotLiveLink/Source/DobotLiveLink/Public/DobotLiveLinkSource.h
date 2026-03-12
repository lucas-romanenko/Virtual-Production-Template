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

/** All decoded data from a single FreeD D1 packet */
struct FFreeDFrameData
{
	// Position & Rotation (decoded to real units)
	float Pan = 0;         // degrees
	float Tilt = 0;        // degrees
	float Roll = 0;        // degrees
	float PosX_mm = 0;     // mm
	float PosY_mm = 0;     // mm
	float PosZ_mm = 0;     // mm

	// Lens (raw 24-bit signed ints from the packet)
	int32 ZoomRaw = 0;     // bytes 20-22
	int32 FocusRaw = 0;    // bytes 23-25
	int32 IrisRaw = 0;     // byte 26

	FTransform Transform = FTransform::Identity;
};

class DOBOTLIVELINK_API FDobotLiveLinkSource : public ILiveLinkSource, public FRunnable
{
public:
	FDobotLiveLinkSource(FString InIPAddress, int32 InPort, float InDelayMs = 0.0f, FString InSubjectName = TEXT("DobotCamera"));
	virtual ~FDobotLiveLinkSource();

	// ILiveLinkSource
	virtual void ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid) override;
	virtual bool IsSourceStillValid() const override;
	virtual bool RequestSourceShutdown() override;
	virtual FText GetSourceType() const override;
	virtual FText GetSourceMachineName() const override;
	virtual FText GetSourceStatus() const override;

	// FRunnable
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;

	/** Latest frame data */
	FFreeDFrameData GetLatestFrame() const { return LatestFrame; }

	void SetFrozen(bool bFreeze) { bIsFrozen = bFreeze; }
	bool IsFrozen() const { return bIsFrozen; }
	float GetPacketsPerSecond() const { return PacketsPerSecond; }
	FString GetConnectionIdentifier() const { return FString::Printf(TEXT("%s:%d"), *IPAddress, Port); }

	/** Camera component pushes mapped lens values here for LiveLink frame data */
	void SetMappedLensData(float FocalLength, float Aperture, float FocusDistance);

private:
	void UpdateLiveMode();
	bool ParseFreeDPacket(const TArray<uint8>& PacketData);
	bool GetDelayedTransform(FTransform& OutTransform) const;
	static int32 DecodeFreeDInt24(const uint8* Data);

	ILiveLinkClient* Client;
	FGuid SourceGuid;
	FString IPAddress;
	int32 Port;
	float DelayMs;
	FName SubjectName;

	FSocket* Socket;
	FRunnableThread* Thread;
	FThreadSafeBool bIsRunning;

	FTransform CurrentTransform;
	FFreeDFrameData LatestFrame;

	float MappedFocalLength;
	float MappedAperture;
	float MappedFocusDistance;

	FThreadSafeBool bIsFrozen;
	float PacketsPerSecond;
	int32 PacketCountThisSecond;
	double PacketRateTimer;
	double LastPacketTime;
	static constexpr double PacketTimeoutSeconds = 5.0;

	TArray<FTimestampedTransform> TransformBuffer;
	static const int32 MaxBufferSize = 2048;
	static constexpr int32 FreeDPacketSize = 29;
	static constexpr uint8 FreeDMessageType = 0xD1;
};