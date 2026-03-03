#pragma once

#include "CoreMinimal.h"
#include "ILiveLinkSource.h"
#include "HAL/Runnable.h"
#include "HAL/ThreadSafeBool.h"
#include "Networking.h"

class DOBOTLIVELINK_API FDobotLiveLinkSource : public ILiveLinkSource, public FRunnable
{
public:
	FDobotLiveLinkSource(FString InIPAddress, int32 InPort, bool bInTestMode);
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
	void UpdateTestMode(float DeltaTime);
	void UpdateLiveMode();
	bool ParseDobotPacket(const TArray<uint8>& PacketData);

	ILiveLinkClient* Client;
	FGuid SourceGuid;

	FString IPAddress;
	int32 Port;
	bool bTestMode;

	FSocket* Socket;
	FRunnableThread* Thread;
	FThreadSafeBool bIsRunning;

	double TestModeTime;
	FTransform CurrentTransform;
};