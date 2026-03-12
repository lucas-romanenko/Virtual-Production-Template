#include "DobotLiveLinkSource.h"
#include "ILiveLinkClient.h"
#include "LiveLinkTypes.h"
#include "Roles/LiveLinkCameraRole.h"
#include "Roles/LiveLinkCameraTypes.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

#define LOCTEXT_NAMESPACE "DobotLiveLinkSource"

// ---------------------------------------------------------------------------
// Construction / Destruction
// ---------------------------------------------------------------------------

FDobotLiveLinkSource::FDobotLiveLinkSource(FString InIPAddress, int32 InPort, float InDelayMs, FString InSubjectName)
	: Client(nullptr)
	, SourceGuid(FGuid::NewGuid())
	, IPAddress(InIPAddress)
	, Port(InPort)
	, DelayMs(InDelayMs)
	, SubjectName(*InSubjectName)
	, Socket(nullptr)
	, Thread(nullptr)
	, bIsRunning(false)
	, CurrentTransform(FTransform::Identity)
	, CurrentFocalLength(24.0f)
	, CurrentAperture(2.8f)
	, CurrentFocusDistance(200.0f)
	, LastPacketTime(0.0)
	, FocalLengthMin(14.0f)
	, FocalLengthMax(200.0f)
	, FocusDistanceMin(30.0f)
	, FocusDistanceMax(10000.0f)
{
	UE_LOG(LogTemp, Warning, TEXT("Dobot LiveLink (FreeD): Source created - ListenPort:%d Delay:%.0fms Subject:%s"),
		Port, DelayMs, *SubjectName.ToString());

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

// ---------------------------------------------------------------------------
// ILiveLinkSource
// ---------------------------------------------------------------------------

void FDobotLiveLinkSource::ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid)
{
	UE_LOG(LogTemp, Warning, TEXT("Dobot LiveLink (FreeD): ReceiveClient called"));
	Client = InClient;
	SourceGuid = InSourceGuid;

	FLiveLinkStaticDataStruct StaticData(FLiveLinkCameraStaticData::StaticStruct());
	Client->PushSubjectStaticData_AnyThread({ SourceGuid, SubjectName }, ULiveLinkCameraRole::StaticClass(), MoveTemp(StaticData));
	UE_LOG(LogTemp, Warning, TEXT("Dobot LiveLink (FreeD): Static data pushed"));
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
	return LOCTEXT("SourceType", "FreeD (Lensmaster)");
}

FText FDobotLiveLinkSource::GetSourceMachineName() const
{
	// Display the local listen port so it appears in the LiveLink panel
	return FText::FromString(FString::Printf(TEXT("UDP :%d"), Port));
}

FText FDobotLiveLinkSource::GetSourceStatus() const
{
	if (bIsRunning)
	{
		double SecondsSincePacket = FPlatformTime::Seconds() - LastPacketTime;
		if (LastPacketTime == 0.0)
		{
			return LOCTEXT("SourceStatus_Waiting", "Waiting for packets...");
		}
		if (SecondsSincePacket > PacketTimeoutSeconds)
		{
			return FText::Format(LOCTEXT("SourceStatus_Stale", "No data ({0}s)"),
				FText::AsNumber((int32)SecondsSincePacket));
		}
		if (DelayMs > 0.0f)
		{
			return FText::Format(LOCTEXT("SourceStatus_ActiveDelay", "Active | Delay: {0}ms"),
				FText::AsNumber((int32)DelayMs));
		}
		return LOCTEXT("SourceStatus_Active", "Active");
	}
	return LOCTEXT("SourceStatus_Inactive", "Inactive");
}

// ---------------------------------------------------------------------------
// FRunnable
// ---------------------------------------------------------------------------

bool FDobotLiveLinkSource::Init()
{
	UE_LOG(LogTemp, Warning, TEXT("Dobot LiveLink (FreeD): Init - binding UDP on port %d"), Port);

	bIsRunning = true;

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	// Create a UDP (datagram) socket
	Socket = SocketSubsystem->CreateSocket(NAME_DGram, TEXT("DobotFreeDSocket"), false);
	if (!Socket)
	{
		UE_LOG(LogTemp, Error, TEXT("Dobot LiveLink (FreeD): Failed to create UDP socket"));
		bIsRunning = false;
		return false;
	}

	// Allow multiple listeners on same port (in case other software also listens)
	Socket->SetReuseAddr(true);

	// Non-blocking so we can honour the stop flag promptly
	Socket->SetNonBlocking(true);

	// Bind to all interfaces on the configured port
	TSharedRef<FInternetAddr> BindAddr = SocketSubsystem->CreateInternetAddr();
	BindAddr->SetAnyAddress();
	BindAddr->SetPort(Port);

	if (!Socket->Bind(*BindAddr))
	{
		UE_LOG(LogTemp, Error, TEXT("Dobot LiveLink (FreeD): Failed to bind UDP socket on port %d"), Port);
		bIsRunning = false;
		return false;
	}

	// Increase receive buffer size to reduce packet loss under burst traffic
	int32 NewSize = 0;
	Socket->SetReceiveBufferSize(256 * 1024, NewSize);

	UE_LOG(LogTemp, Warning, TEXT("Dobot LiveLink (FreeD): Listening on UDP port %d"), Port);
	return true;
}

uint32 FDobotLiveLinkSource::Run()
{
	UE_LOG(LogTemp, Warning, TEXT("Dobot LiveLink (FreeD): Thread started, Delay=%.0fms Subject=%s"),
		DelayMs, *SubjectName.ToString());

	int32 FrameCount = 0;
	LastPacketTime = 0.0;

	while (bIsRunning)
	{
		// Poll for incoming FreeD packets
		UpdateLiveMode();

		if (!bIsRunning) break;

		// Buffer the latest transform with a timestamp for the delay system
		double Now = FPlatformTime::Seconds();
		TransformBuffer.Add(FTimestampedTransform(Now, CurrentTransform));

		double MaxHistory = (DelayMs / 1000.0) + 1.0;
		while (TransformBuffer.Num() > 2 && (Now - TransformBuffer[0].Timestamp) > MaxHistory)
		{
			TransformBuffer.RemoveAt(0);
		}
		while (TransformBuffer.Num() > MaxBufferSize)
		{
			TransformBuffer.RemoveAt(0);
		}

		FTransform PushTransform;
		if (!GetDelayedTransform(PushTransform))
		{
			PushTransform = CurrentTransform;
		}

		if (Client)
		{
			FLiveLinkFrameDataStruct FrameData(FLiveLinkCameraFrameData::StaticStruct());
			FLiveLinkCameraFrameData* CameraData = FrameData.Cast<FLiveLinkCameraFrameData>();
			CameraData->Transform = PushTransform;
			CameraData->FocalLength = CurrentFocalLength;
			CameraData->Aperture = CurrentAperture;
			CameraData->FocusDistance = CurrentFocusDistance;
			Client->PushSubjectFrameData_AnyThread({ SourceGuid, SubjectName }, MoveTemp(FrameData));

			if (FrameCount < 3)
			{
				UE_LOG(LogTemp, Warning, TEXT("Dobot LiveLink (FreeD): Frame %d pushed - Loc: %s Rot: %s"),
					FrameCount,
					*PushTransform.GetLocation().ToString(),
					*PushTransform.Rotator().ToString());
				FrameCount++;
			}
		}
		else
		{
			static bool bLoggedOnce = false;
			if (!bLoggedOnce)
			{
				UE_LOG(LogTemp, Error, TEXT("Dobot LiveLink (FreeD): Client is NULL!"));
				bLoggedOnce = true;
			}
		}

		// ~30Hz loop; non-blocking recv means we spin but don't block on missing packets
		FPlatformProcess::Sleep(0.033f);
	}

	UE_LOG(LogTemp, Warning, TEXT("Dobot LiveLink (FreeD): Thread stopped"));
	return 0;
}

void FDobotLiveLinkSource::Stop()
{
	bIsRunning = false;
}

// ---------------------------------------------------------------------------
// UDP receive loop
// ---------------------------------------------------------------------------

void FDobotLiveLinkSource::UpdateLiveMode()
{
	if (!Socket)
	{
		bIsRunning = false;
		return;
	}

	// Timeout check -- if Lensmaster stops sending, mark source dead
	if (LastPacketTime > 0.0)
	{
		double SecondsSincePacket = FPlatformTime::Seconds() - LastPacketTime;
		if (SecondsSincePacket > PacketTimeoutSeconds)
		{
			UE_LOG(LogTemp, Warning, TEXT("Dobot LiveLink (FreeD): No packet received for %.1fs - connection lost"), SecondsSincePacket);
			bIsRunning = false;
			return;
		}
	}

	// Drain all available packets this tick (Lensmaster typically sends at 30-100 Hz)
	TArray<uint8> ReceivedData;
	ReceivedData.SetNumUninitialized(FreeDPacketSize * 4); // small buffer, FreeD packets are tiny

	TSharedRef<FInternetAddr> SenderAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();

	bool bGotPacket = false;
	int32 BytesRead = 0;

	while (true)
	{
		BytesRead = 0;
		bool bRecvOk = Socket->RecvFrom(ReceivedData.GetData(), ReceivedData.Num(), BytesRead, *SenderAddr);

		if (!bRecvOk || BytesRead == 0)
		{
			// EWOULDBLOCK / no data available -- nothing left to read this tick
			break;
		}

		if (BytesRead == FreeDPacketSize)
		{
			// Trim to actual packet size before parsing
			TArray<uint8> Packet(ReceivedData.GetData(), FreeDPacketSize);
			if (ParseFreeDPacket(Packet))
			{
				bGotPacket = true;
			}
		}
		else
		{
			UE_LOG(LogTemp, Verbose, TEXT("Dobot LiveLink (FreeD): Unexpected packet size %d (expected %d)"), BytesRead, FreeDPacketSize);
		}
	}

	if (bGotPacket)
	{
		LastPacketTime = FPlatformTime::Seconds();
	}
}

// ---------------------------------------------------------------------------
// FreeD D1 packet parser
// ---------------------------------------------------------------------------

int32 FDobotLiveLinkSource::DecodeFreeDInt24(const uint8* Data)
{
	// FreeD uses big-endian 24-bit signed integers
	int32 Value = ((int32)Data[0] << 16) | ((int32)Data[1] << 8) | (int32)Data[2];

	// Sign-extend from 24 bits to 32 bits
	if (Value & 0x800000)
	{
		Value |= 0xFF000000;
	}
	return Value;
}

bool FDobotLiveLinkSource::ParseFreeDPacket(const TArray<uint8>& PacketData)
{
	if (PacketData.Num() != FreeDPacketSize)
	{
		return false;
	}

	// Byte 0: message type -- must be 0xD1 for position/orientation
	if (PacketData[0] != FreeDMessageType)
	{
		UE_LOG(LogTemp, Verbose, TEXT("Dobot LiveLink (FreeD): Ignoring non-D1 message type 0x%02X"), PacketData[0]);
		return false;
	}

	// ---- Validate checksum ----
	// FreeD checksum: 0x40 minus the sum of bytes 0..26, result stored in byte 27
	// (byte 28 is spare / padding)
	uint8 Checksum = 0x40;
	for (int32 i = 0; i < 27; ++i)
	{
		Checksum -= PacketData[i];
	}
	if (Checksum != PacketData[27])
	{
		UE_LOG(LogTemp, Warning, TEXT("Dobot LiveLink (FreeD): Checksum mismatch (got 0x%02X, expected 0x%02X) -- packet dropped"),
			PacketData[27], Checksum);
		return false;
	}

	// ---- Decode rotation (Pan / Tilt / Roll) ----
	// Fixed-point: value / 2^15 = degrees
	// Bytes 2-4: Pan (camera heading / yaw)
	// Bytes 5-7: Tilt (camera pitch)
	// Bytes 8-10: Roll
	const float RotScale = 1.0f / 32768.0f; // 2^15

	float Pan = (float)DecodeFreeDInt24(&PacketData[2]) * RotScale; // Yaw   (degrees)
	float Tilt = (float)DecodeFreeDInt24(&PacketData[5]) * RotScale; // Pitch (degrees)
	float Roll = (float)DecodeFreeDInt24(&PacketData[8]) * RotScale; // Roll  (degrees)

	// ---- Decode position (X / Y / Z) ----
	// Fixed-point: value / 64 = millimetres
	// Bytes 11-13: X, Bytes 14-16: Y, Bytes 17-19: Z (height)
	const float PosScale = 1.0f / 64.0f; // mm

	float PosX_mm = (float)DecodeFreeDInt24(&PacketData[11]) * PosScale;
	float PosY_mm = (float)DecodeFreeDInt24(&PacketData[14]) * PosScale;
	float PosZ_mm = (float)DecodeFreeDInt24(&PacketData[17]) * PosScale;

	// ---- Decode zoom and focus ----
	// FreeD zoom/focus are 24-bit signed fixed-point, scale = 2^15 (same as rotation)
	// Lensmaster outputs calibrated values: zoom in mm*32768, focus in mm*32768
	// We convert focus from mm to cm for UE.
	int32 ZoomRaw = DecodeFreeDInt24(&PacketData[20]);
	int32 FocusRaw = DecodeFreeDInt24(&PacketData[23]);

	float ZoomMm = (float)ZoomRaw / 32768.0f;  // focal length in mm
	float FocusMm = (float)FocusRaw / 32768.0f;  // focus distance in mm

	// Only apply if Lensmaster is outputting valid calibrated data (non-zero)
	if (ZoomMm > 0.0f)
	{
		CurrentFocalLength = ZoomMm;
	}
	if (FocusMm > 0.0f)
	{
		CurrentFocusDistance = FocusMm / 10.0f;  // mm -> cm for UE
	}

	// ---- Debug logging (first few packets) ----
	static int32 PacketCount = 0;
	if (PacketCount < 5)
	{
		UE_LOG(LogTemp, Log, TEXT("Dobot FreeD: Pan=%.3f Tilt=%.3f Roll=%.3f  X=%.1fmm Y=%.1fmm Z=%.1fmm  FL=%.1fmm Focus=%.1fcm"),
			Pan, Tilt, Roll, PosX_mm, PosY_mm, PosZ_mm, CurrentFocalLength, CurrentFocusDistance);
		PacketCount++;
	}

	// ---- Convert to Unreal coordinate space ----
	//
	// FreeD convention (from Lensmaster / broadcast camera tracking):
	//   Pan  = rotation around vertical axis (yaw), positive = right
	//   Tilt = rotation around camera-right axis (pitch), positive = up
	//   Roll = rotation around camera-forward axis, positive = clockwise
	//   X    = camera position along horizontal axis (mm)
	//   Y    = camera position along depth / forward axis (mm)
	//   Z    = camera height (mm)
	//
	// Unreal Engine coordinate space (left-handed, Z-up):
	//   X = forward, Y = right, Z = up
	//   Pitch = rotation around Y, Yaw = rotation around Z, Roll = around X
	//
	// Mapping:
	//   UE X (forward) <- FreeD Y
	//   UE Y (right)   <- FreeD X
	//   UE Z (up)      <- FreeD Z
	//   Pitch          <- -Tilt  (FreeD tilt positive = up, UE pitch positive = down)
	//   Yaw            <- Pan
	//   Roll           <- -Roll  (handedness flip)
	//
	// Position: convert mm -> UE units (1 UE unit = 1 cm), so divide by 10
	// (Lensmaster outputs mm; Unreal default is cm)

	FVector Location(
		PosY_mm / 10.0f,   // UE X = forward = FreeD Y
		PosX_mm / 10.0f,   // UE Y = right   = FreeD X
		PosZ_mm / 10.0f    // UE Z = up       = FreeD Z
	);

	FRotator Rotation(
		-Tilt,  // Pitch
		Pan,   // Yaw
		-Roll   // Roll
	);

	CurrentTransform = FTransform(Rotation, Location);
	return true;
}

// ---------------------------------------------------------------------------
// Delay buffer interpolation (unchanged from original)
// ---------------------------------------------------------------------------

bool FDobotLiveLinkSource::GetDelayedTransform(FTransform& OutTransform) const
{
	if (TransformBuffer.Num() == 0) return false;
	if (DelayMs <= 0.0f) { OutTransform = TransformBuffer.Last().Transform; return true; }

	double DelaySeconds = DelayMs / 1000.0;
	double TargetTime = FPlatformTime::Seconds() - DelaySeconds;

	if (TargetTime <= TransformBuffer[0].Timestamp) { OutTransform = TransformBuffer[0].Transform;    return true; }
	if (TargetTime >= TransformBuffer.Last().Timestamp) { OutTransform = TransformBuffer.Last().Transform; return true; }

	for (int32 i = TransformBuffer.Num() - 1; i > 0; --i)
	{
		if (TransformBuffer[i - 1].Timestamp <= TargetTime && TransformBuffer[i].Timestamp >= TargetTime)
		{
			double TimeRange = TransformBuffer[i].Timestamp - TransformBuffer[i - 1].Timestamp;
			if (TimeRange <= 0.0) { OutTransform = TransformBuffer[i].Transform; return true; }

			float Alpha = FMath::Clamp((float)((TargetTime - TransformBuffer[i - 1].Timestamp) / TimeRange), 0.0f, 1.0f);
			FVector Loc = FMath::Lerp(TransformBuffer[i - 1].Transform.GetLocation(), TransformBuffer[i].Transform.GetLocation(), Alpha);
			FQuat   Rot = FQuat::Slerp(TransformBuffer[i - 1].Transform.GetRotation(), TransformBuffer[i].Transform.GetRotation(), Alpha);
			OutTransform = FTransform(Rot, Loc, TransformBuffer[i - 1].Transform.GetScale3D());
			return true;
		}
	}

	OutTransform = TransformBuffer.Last().Transform;
	return true;
}

#undef LOCTEXT_NAMESPACE