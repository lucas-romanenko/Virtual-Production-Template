#include "DobotLiveLinkSource.h"
#include "ILiveLinkClient.h"
#include "LiveLinkTypes.h"
#include "Roles/LiveLinkCameraRole.h"
#include "Roles/LiveLinkCameraTypes.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

#define LOCTEXT_NAMESPACE "DobotLiveLinkSource"

FDobotLiveLinkSource::FDobotLiveLinkSource(FString InIPAddress, int32 InPort, float InDelayMs, FString InSubjectName)
	: Client(nullptr), SourceGuid(FGuid::NewGuid()), IPAddress(InIPAddress), Port(InPort)
	, DelayMs(InDelayMs), SubjectName(*InSubjectName), Socket(nullptr), Thread(nullptr)
	, bIsRunning(false), CurrentTransform(FTransform::Identity)
	, MappedFocalLength(0), MappedAperture(2.8f), MappedFocusDistance(200)
	, bIsFrozen(false), PacketsPerSecond(0), PacketCountThisSecond(0)
	, PacketRateTimer(0), LastPacketTime(0)
{
	UE_LOG(LogTemp, Warning, TEXT("LiveLink FreeD: Source created - Port:%d Subject:%s"), Port, *SubjectName.ToString());
	Thread = FRunnableThread::Create(this, TEXT("FreeDSourceThread"));
}

FDobotLiveLinkSource::~FDobotLiveLinkSource()
{
	Stop();
	if (Thread) { Thread->Kill(true); delete Thread; Thread = nullptr; }
	if (Socket) { Socket->Close(); ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket); Socket = nullptr; }
}

void FDobotLiveLinkSource::ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid)
{
	Client = InClient;
	SourceGuid = InSourceGuid;
	FLiveLinkStaticDataStruct StaticData(FLiveLinkCameraStaticData::StaticStruct());
	Client->PushSubjectStaticData_AnyThread({ SourceGuid, SubjectName }, ULiveLinkCameraRole::StaticClass(), MoveTemp(StaticData));
}

bool FDobotLiveLinkSource::IsSourceStillValid() const { return bIsRunning; }
bool FDobotLiveLinkSource::RequestSourceShutdown() { Stop(); return true; }
FText FDobotLiveLinkSource::GetSourceType() const { return LOCTEXT("Type", "FreeD (Lensmaster)"); }
FText FDobotLiveLinkSource::GetSourceMachineName() const { return FText::FromString(FString::Printf(TEXT("%s:%d"), *IPAddress, Port)); }

FText FDobotLiveLinkSource::GetSourceStatus() const
{
	if (!bIsRunning) return LOCTEXT("Off", "Inactive");
	if (LastPacketTime == 0.0) return LOCTEXT("Wait", "Waiting for packets...");
	double Sec = FPlatformTime::Seconds() - LastPacketTime;
	if (Sec > PacketTimeoutSeconds) return FText::Format(LOCTEXT("Stale", "No data ({0}s)"), FText::AsNumber((int32)Sec));
	FString S = TEXT("Active");
	if (bIsFrozen) S += TEXT(" | FROZEN");
	if (DelayMs > 0) S += FString::Printf(TEXT(" | Delay: %dms"), (int32)DelayMs);
	return FText::FromString(S);
}

void FDobotLiveLinkSource::SetMappedLensData(float FocalLength, float Aperture, float FocusDistance)
{
	MappedFocalLength = FocalLength;
	MappedAperture = Aperture;
	MappedFocusDistance = FocusDistance;
}

bool FDobotLiveLinkSource::Init()
{
	bIsRunning = true;
	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	Socket = SS->CreateSocket(NAME_DGram, TEXT("FreeDSocket"), false);
	if (!Socket) { bIsRunning = false; return false; }
	Socket->SetReuseAddr(true);
	Socket->SetNonBlocking(true);
	TSharedRef<FInternetAddr> Addr = SS->CreateInternetAddr();
	Addr->SetAnyAddress();
	Addr->SetPort(Port);
	if (!Socket->Bind(*Addr)) { UE_LOG(LogTemp, Error, TEXT("LiveLink FreeD: Bind failed on port %d"), Port); bIsRunning = false; return false; }
	int32 Sz = 0; Socket->SetReceiveBufferSize(256 * 1024, Sz);
	UE_LOG(LogTemp, Warning, TEXT("LiveLink FreeD: Listening on UDP port %d"), Port);
	return true;
}

uint32 FDobotLiveLinkSource::Run()
{
	int32 FrameCount = 0;
	LastPacketTime = 0;
	PacketRateTimer = FPlatformTime::Seconds();

	while (bIsRunning)
	{
		UpdateLiveMode();
		if (!bIsRunning) break;

		double Now = FPlatformTime::Seconds();
		if (Now - PacketRateTimer >= 1.0) { PacketsPerSecond = (float)PacketCountThisSecond / (float)(Now - PacketRateTimer); PacketCountThisSecond = 0; PacketRateTimer = Now; }
		if (bIsFrozen) { FPlatformProcess::Sleep(0.033f); continue; }

		TransformBuffer.Add(FTimestampedTransform(Now, CurrentTransform));
		double MaxHist = (DelayMs / 1000.0) + 1.0;
		while (TransformBuffer.Num() > 2 && (Now - TransformBuffer[0].Timestamp) > MaxHist) TransformBuffer.RemoveAt(0);
		while (TransformBuffer.Num() > MaxBufferSize) TransformBuffer.RemoveAt(0);

		FTransform Push;
		if (!GetDelayedTransform(Push)) Push = CurrentTransform;

		if (Client)
		{
			FLiveLinkFrameDataStruct FD(FLiveLinkCameraFrameData::StaticStruct());
			FLiveLinkCameraFrameData* CD = FD.Cast<FLiveLinkCameraFrameData>();
			CD->Transform = Push;
			CD->FocalLength = MappedFocalLength;
			CD->Aperture = MappedAperture;
			CD->FocusDistance = MappedFocusDistance;
			Client->PushSubjectFrameData_AnyThread({ SourceGuid, SubjectName }, MoveTemp(FD));

			if (FrameCount < 3)
			{
				UE_LOG(LogTemp, Warning, TEXT("LiveLink FreeD: Frame %d - Loc:%s Rot:%s ZoomRaw:%d FocusRaw:%d IrisRaw:%d"),
					FrameCount, *Push.GetLocation().ToString(), *Push.Rotator().ToString(),
					LatestFrame.ZoomRaw, LatestFrame.FocusRaw, LatestFrame.IrisRaw);
				FrameCount++;
			}
		}
		FPlatformProcess::Sleep(0.033f);
	}
	return 0;
}

void FDobotLiveLinkSource::Stop() { bIsRunning = false; }

void FDobotLiveLinkSource::UpdateLiveMode()
{
	if (!Socket) { bIsRunning = false; return; }
	if (LastPacketTime > 0 && (FPlatformTime::Seconds() - LastPacketTime) > PacketTimeoutSeconds) { bIsRunning = false; return; }

	TArray<uint8> Buf; Buf.SetNumUninitialized(FreeDPacketSize * 4);
	TSharedRef<FInternetAddr> Sender = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
	bool bGot = false;
	while (true)
	{
		int32 Read = 0;
		if (!Socket->RecvFrom(Buf.GetData(), Buf.Num(), Read, *Sender) || Read == 0) break;
		if (Read == FreeDPacketSize) { TArray<uint8> Pkt(Buf.GetData(), FreeDPacketSize); if (ParseFreeDPacket(Pkt)) bGot = true; }
	}
	if (bGot) LastPacketTime = FPlatformTime::Seconds();
}

int32 FDobotLiveLinkSource::DecodeFreeDInt24(const uint8* D)
{
	int32 V = ((int32)D[0] << 16) | ((int32)D[1] << 8) | (int32)D[2];
	if (V & 0x800000) V |= 0xFF000000;
	return V;
}

bool FDobotLiveLinkSource::ParseFreeDPacket(const TArray<uint8>& P)
{
	if (P.Num() != FreeDPacketSize || P[0] != FreeDMessageType) return false;

	uint8 CS = 0x40;
	for (int32 i = 0; i < 27; ++i) CS -= P[i];
	if (CS != P[27]) return false;

	const float RS = 1.0f / 32768.0f;
	const float PS = 1.0f / 64.0f;

	LatestFrame.Pan = (float)DecodeFreeDInt24(&P[2]) * RS;
	LatestFrame.Tilt = (float)DecodeFreeDInt24(&P[5]) * RS;
	LatestFrame.Roll = (float)DecodeFreeDInt24(&P[8]) * RS;
	LatestFrame.PosX_mm = (float)DecodeFreeDInt24(&P[11]) * PS;
	LatestFrame.PosY_mm = (float)DecodeFreeDInt24(&P[14]) * PS;
	LatestFrame.PosZ_mm = (float)DecodeFreeDInt24(&P[17]) * PS;
	LatestFrame.ZoomRaw = DecodeFreeDInt24(&P[20]);
	LatestFrame.FocusRaw = DecodeFreeDInt24(&P[23]);
	LatestFrame.IrisRaw = (int32)P[26];

	// Decode lens values (True Value mode: same fixed-point as rotation, /32768)
	// Zoom: focal length in mm (0 = no zoom motor / prime lens)
	float ZoomDecoded = (float)LatestFrame.ZoomRaw * RS;  // RS = 1/32768
	LatestFrame.FocalLength_mm = (ZoomDecoded > 0.0f) ? ZoomDecoded : 0.0f;

	// Focus: distance in meters from Lensmaster (Metric mode), convert to cm for UE
	float FocusDecoded = (float)LatestFrame.FocusRaw * RS;  // meters
	LatestFrame.FocusDistance_cm = (FocusDecoded > 0.0f) ? FocusDecoded * 100.0f : 0.0f;  // m -> cm

	// Iris: byte 26. In True Value mode, likely t-stop * some scale.
	// Common encodings: raw 0-255 where value/10 = t-stop, or value directly.
	// We store the decoded value; if it's in a plausible f-stop range, use it.
	if (LatestFrame.IrisRaw > 0)
	{
		// Try /10 first (t-stop * 10 in a single byte)
		float IrisTry = (float)LatestFrame.IrisRaw / 10.0f;
		if (IrisTry >= 0.7f && IrisTry <= 64.0f)
			LatestFrame.Aperture = IrisTry;
		else
			LatestFrame.Aperture = (float)LatestFrame.IrisRaw;  // store raw, let user interpret
	}
	else
	{
		LatestFrame.Aperture = 0.0f;
	}

	PacketCountThisSecond++;

	// Debug first packets with decoded values
	static int32 DbgCount = 0;
	if (DbgCount < 5)
	{
		UE_LOG(LogTemp, Log, TEXT("FreeD: Pan=%.2f Tilt=%.2f Roll=%.2f | X=%.0f Y=%.0f Z=%.0f mm | FL=%.1fmm Focus=%.0fcm Iris=%.1f (raw Z=%d F=%d I=%d)"),
			LatestFrame.Pan, LatestFrame.Tilt, LatestFrame.Roll,
			LatestFrame.PosX_mm, LatestFrame.PosY_mm, LatestFrame.PosZ_mm,
			LatestFrame.FocalLength_mm, LatestFrame.FocusDistance_cm, LatestFrame.Aperture,
			LatestFrame.ZoomRaw, LatestFrame.FocusRaw, LatestFrame.IrisRaw);
		DbgCount++;
	}

	FVector Loc(LatestFrame.PosY_mm / 10.0f, LatestFrame.PosX_mm / 10.0f, LatestFrame.PosZ_mm / 10.0f);
	FRotator Rot(-LatestFrame.Tilt, LatestFrame.Pan, -LatestFrame.Roll);
	CurrentTransform = FTransform(Rot, Loc);
	LatestFrame.Transform = CurrentTransform;
	return true;
}

bool FDobotLiveLinkSource::GetDelayedTransform(FTransform& Out) const
{
	if (TransformBuffer.Num() == 0) return false;
	if (DelayMs <= 0) { Out = TransformBuffer.Last().Transform; return true; }
	double Target = FPlatformTime::Seconds() - DelayMs / 1000.0;
	if (Target <= TransformBuffer[0].Timestamp) { Out = TransformBuffer[0].Transform; return true; }
	if (Target >= TransformBuffer.Last().Timestamp) { Out = TransformBuffer.Last().Transform; return true; }
	for (int32 i = TransformBuffer.Num() - 1; i > 0; --i)
	{
		if (TransformBuffer[i - 1].Timestamp <= Target && TransformBuffer[i].Timestamp >= Target)
		{
			double R = TransformBuffer[i].Timestamp - TransformBuffer[i - 1].Timestamp;
			if (R <= 0) { Out = TransformBuffer[i].Transform; return true; }
			float A = FMath::Clamp((float)((Target - TransformBuffer[i - 1].Timestamp) / R), 0.0f, 1.0f);
			Out = FTransform(
				FQuat::Slerp(TransformBuffer[i - 1].Transform.GetRotation(), TransformBuffer[i].Transform.GetRotation(), A),
				FMath::Lerp(TransformBuffer[i - 1].Transform.GetLocation(), TransformBuffer[i].Transform.GetLocation(), A),
				TransformBuffer[i - 1].Transform.GetScale3D());
			return true;
		}
	}
	Out = TransformBuffer.Last().Transform;
	return true;
}

#undef LOCTEXT_NAMESPACE