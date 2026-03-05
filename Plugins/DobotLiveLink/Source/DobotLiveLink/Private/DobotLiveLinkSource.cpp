#include "DobotLiveLinkSource.h"
#include "ILiveLinkClient.h"
#include "LiveLinkTypes.h"
#include "Roles/LiveLinkTransformRole.h"
#include "Roles/LiveLinkTransformTypes.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

#define LOCTEXT_NAMESPACE "DobotLiveLinkSource"

FDobotLiveLinkSource::FDobotLiveLinkSource(FString InIPAddress, int32 InPort, bool bInTestMode, float InDelayMs, FString InSubjectName)
	: Client(nullptr)
	, SourceGuid(FGuid::NewGuid())
	, IPAddress(InIPAddress)
	, Port(InPort)
	, bTestMode(bInTestMode)
	, DelayMs(InDelayMs)
	, SubjectName(*InSubjectName)
	, Socket(nullptr)
	, Thread(nullptr)
	, bIsRunning(false)
	, TestModeTime(0.0)
	, CurrentTransform(FTransform::Identity)
{
	UE_LOG(LogTemp, Warning, TEXT("Dobot LiveLink: Source created - IP:%s Port:%d TestMode:%d Delay:%.0fms Subject:%s"), *IPAddress, Port, bTestMode, DelayMs, *SubjectName.ToString());
	Thread = FRunnableThread::Create(this, TEXT("DobotLiveLinkSourceThread"));
}

FDobotLiveLinkSource::~FDobotLiveLinkSource()
{
	Stop();

	if (Thread)
	{
		Thread->Kill(true);
		delete Thread;
		Thread = nullptr;
	}

	if (Socket)
	{
		Socket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
		Socket = nullptr;
	}
}

void FDobotLiveLinkSource::ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid)
{
	UE_LOG(LogTemp, Warning, TEXT("Dobot LiveLink: ReceiveClient called"));
	Client = InClient;
	SourceGuid = InSourceGuid;

	FLiveLinkStaticDataStruct StaticData(FLiveLinkTransformStaticData::StaticStruct());
	Client->PushSubjectStaticData_AnyThread({ SourceGuid, SubjectName }, ULiveLinkTransformRole::StaticClass(), MoveTemp(StaticData));
	UE_LOG(LogTemp, Warning, TEXT("Dobot LiveLink: Static data pushed"));
}

bool FDobotLiveLinkSource::IsSourceStillValid() const
{
	return bIsRunning;
}

bool FDobotLiveLinkSource::RequestSourceShutdown()
{
	Stop();
	return true;
}

FText FDobotLiveLinkSource::GetSourceType() const
{
	return LOCTEXT("DobotSourceType", "Dobot Robot");
}

FText FDobotLiveLinkSource::GetSourceMachineName() const
{
	return FText::FromString(IPAddress);
}

FText FDobotLiveLinkSource::GetSourceStatus() const
{
	if (bTestMode)
	{
		if (DelayMs > 0.0f)
		{
			return FText::Format(LOCTEXT("SourceStatus_TestDelay", "Test Mode | Delay: {0}ms"), FText::AsNumber((int32)DelayMs));
		}
		return LOCTEXT("SourceStatus_TestMode", "Test Mode Active");
	}
	if (bIsRunning)
	{
		if (DelayMs > 0.0f)
		{
			return FText::Format(LOCTEXT("SourceStatus_ActiveDelay", "Active | Delay: {0}ms"), FText::AsNumber((int32)DelayMs));
		}
		return LOCTEXT("SourceStatus_Active", "Active");
	}
	return LOCTEXT("SourceStatus_Inactive", "Inactive");
}

bool FDobotLiveLinkSource::Init()
{
	UE_LOG(LogTemp, Warning, TEXT("Dobot LiveLink: Init called, bTestMode=%d"), bTestMode);
	bIsRunning = true;

	if (!bTestMode)
	{
		ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		Socket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("DobotSocket"), false);

		if (Socket)
		{
			TSharedRef<FInternetAddr> Addr = SocketSubsystem->CreateInternetAddr();
			bool bIsValid;
			Addr->SetIp(*IPAddress, bIsValid);
			Addr->SetPort(Port);

			if (bIsValid && Socket->Connect(*Addr))
			{
				UE_LOG(LogTemp, Log, TEXT("Dobot LiveLink: Connected to %s:%d"), *IPAddress, Port);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Dobot LiveLink: Failed to connect to %s:%d"), *IPAddress, Port);
				return false;
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Dobot LiveLink: Failed to create socket"));
			return false;
		}
	}

	return true;
}

bool FDobotLiveLinkSource::GetDelayedTransform(FTransform& OutTransform) const
{
	if (TransformBuffer.Num() == 0) return false;
	if (DelayMs <= 0.0f) { OutTransform = TransformBuffer.Last().Transform; return true; }

	double DelaySeconds = DelayMs / 1000.0;
	double TargetTime = FPlatformTime::Seconds() - DelaySeconds;

	if (TargetTime <= TransformBuffer[0].Timestamp) { OutTransform = TransformBuffer[0].Transform; return true; }
	if (TargetTime >= TransformBuffer.Last().Timestamp) { OutTransform = TransformBuffer.Last().Transform; return true; }

	for (int32 i = TransformBuffer.Num() - 1; i > 0; --i)
	{
		if (TransformBuffer[i - 1].Timestamp <= TargetTime && TransformBuffer[i].Timestamp >= TargetTime)
		{
			double TimeRange = TransformBuffer[i].Timestamp - TransformBuffer[i - 1].Timestamp;
			if (TimeRange <= 0.0) { OutTransform = TransformBuffer[i].Transform; return true; }

			float Alpha = FMath::Clamp((float)((TargetTime - TransformBuffer[i - 1].Timestamp) / TimeRange), 0.0f, 1.0f);
			FVector Loc = FMath::Lerp(TransformBuffer[i - 1].Transform.GetLocation(), TransformBuffer[i].Transform.GetLocation(), Alpha);
			FQuat Rot = FQuat::Slerp(TransformBuffer[i - 1].Transform.GetRotation(), TransformBuffer[i].Transform.GetRotation(), Alpha);
			OutTransform = FTransform(Rot, Loc, TransformBuffer[i - 1].Transform.GetScale3D());
			return true;
		}
	}
	OutTransform = TransformBuffer.Last().Transform;
	return true;
}

uint32 FDobotLiveLinkSource::Run()
{
	UE_LOG(LogTemp, Warning, TEXT("Dobot LiveLink: Thread started, bTestMode=%d Delay=%.0fms Subject=%s"), bTestMode, DelayMs, *SubjectName.ToString());
	int32 FrameCount = 0;

	while (bIsRunning)
	{
		float DeltaTime = 0.033f;
		if (bTestMode) { UpdateTestMode(DeltaTime); }
		else { UpdateLiveMode(); }

		double Now = FPlatformTime::Seconds();
		TransformBuffer.Add(FTimestampedTransform(Now, CurrentTransform));

		double MaxHistory = (DelayMs / 1000.0) + 1.0;
		while (TransformBuffer.Num() > 2 && (Now - TransformBuffer[0].Timestamp) > MaxHistory) { TransformBuffer.RemoveAt(0); }
		while (TransformBuffer.Num() > MaxBufferSize) { TransformBuffer.RemoveAt(0); }

		FTransform PushTransform;
		if (!GetDelayedTransform(PushTransform)) { PushTransform = CurrentTransform; }

		if (Client)
		{
			FLiveLinkFrameDataStruct FrameData(FLiveLinkTransformFrameData::StaticStruct());
			FLiveLinkTransformFrameData* TransformData = FrameData.Cast<FLiveLinkTransformFrameData>();
			TransformData->Transform = PushTransform;
			Client->PushSubjectFrameData_AnyThread({ SourceGuid, SubjectName }, MoveTemp(FrameData));

			if (FrameCount < 3)
			{
				UE_LOG(LogTemp, Warning, TEXT("Dobot LiveLink: Frame %d pushed - Loc: %s"), FrameCount, *PushTransform.GetLocation().ToString());
				FrameCount++;
			}
		}
		else
		{
			static bool bLoggedOnce = false;
			if (!bLoggedOnce) { UE_LOG(LogTemp, Error, TEXT("Dobot LiveLink: Client is NULL!")); bLoggedOnce = true; }
		}
		FPlatformProcess::Sleep(DeltaTime);
	}
	UE_LOG(LogTemp, Warning, TEXT("Dobot LiveLink: Thread stopped"));
	return 0;
}

void FDobotLiveLinkSource::Stop()
{
	bIsRunning = false;
}

void FDobotLiveLinkSource::UpdateTestMode(float DeltaTime)
{
	TestModeTime += DeltaTime;

	float X = FMath::Sin(FMath::DegreesToRadians(TestModeTime * 15.0f)) * 30.0f;
	float Y = FMath::Cos(FMath::DegreesToRadians(TestModeTime * 10.0f)) * 10.0f;
	float Z = FMath::Sin(FMath::DegreesToRadians(TestModeTime * 12.0f)) * 15.0f + 150.0f;
	float Yaw = FMath::Sin(FMath::DegreesToRadians(TestModeTime * 8.0f)) * 10.0f;
	float Pitch = FMath::Sin(FMath::DegreesToRadians(TestModeTime * 6.0f)) * 5.0f;

	FVector Location(-Y - 330.0f, X, Z);
	FRotator Rotation(Pitch, Yaw + 180.0f, 0.0f);

	CurrentTransform = FTransform(Rotation, Location);
}

void FDobotLiveLinkSource::UpdateLiveMode()
{
	if (!Socket) return;

	TArray<uint8> ReceivedData;
	ReceivedData.SetNumUninitialized(1440);

	int32 BytesRead = 0;
	if (Socket->Recv(ReceivedData.GetData(), 1440, BytesRead))
	{
		if (BytesRead == 1440)
		{
			ParseDobotPacket(ReceivedData);
		}
		else if (BytesRead > 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("Dobot LiveLink: Partial packet received (%d bytes)"), BytesRead);
		}
	}
}

bool FDobotLiveLinkSource::ParseDobotPacket(const TArray<uint8>& PacketData)
{
	if (PacketData.Num() != 1440)
	{
		UE_LOG(LogTemp, Warning, TEXT("Dobot LiveLink: Invalid packet size: %d bytes (expected 1440)"), PacketData.Num());
		return false;
	}

	const int32 PoseOffset = 624;

	double X, Y, Z, Rx, Ry, Rz;

	FMemory::Memcpy(&X, &PacketData[PoseOffset + 0], 8);
	FMemory::Memcpy(&Y, &PacketData[PoseOffset + 8], 8);
	FMemory::Memcpy(&Z, &PacketData[PoseOffset + 16], 8);
	FMemory::Memcpy(&Rx, &PacketData[PoseOffset + 24], 8);
	FMemory::Memcpy(&Ry, &PacketData[PoseOffset + 32], 8);
	FMemory::Memcpy(&Rz, &PacketData[PoseOffset + 40], 8);

	static int32 PacketCount = 0;
	if (PacketCount < 5)
	{
		UE_LOG(LogTemp, Log, TEXT("Dobot Pose: X=%.2f Y=%.2f Z=%.2f Rx=%.2f Ry=%.2f Rz=%.2f"),
			X, Y, Z, Rx, Ry, Rz);
		PacketCount++;
	}

	X /= 10.0;
	Y /= 10.0;
	Z /= 10.0;

	FVector Location(-Y, X, Z);
	FRotator Rotation(Ry, Rz, Rx);

	CurrentTransform = FTransform(Rotation, Location);

	return true;
}

#undef LOCTEXT_NAMESPACE