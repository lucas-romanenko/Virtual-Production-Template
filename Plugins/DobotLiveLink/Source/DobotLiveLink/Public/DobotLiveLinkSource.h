#pragma once

#include "CoreMinimal.h"
#include "ILiveLinkSource.h"
#include "HAL/Runnable.h"
#include "HAL/ThreadSafeBool.h"
#include "Networking.h"

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
	bool ParseDobotPacket(const TArray<uint8>& PacketData);
	bool GetDelayedTransform(FTransform& OutTransform) const;

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

	TArray<FTimestampedTransform> TransformBuffer;
	static const int32 MaxBufferSize = 2048;
};