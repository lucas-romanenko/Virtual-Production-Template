#include "DobotLiveLinkSourceFactory.h"
#include "DobotLiveLinkSource.h"
#include "SDobotLiveLinkSourceFactory.h"

#define LOCTEXT_NAMESPACE "DobotLiveLinkSourceFactory"

FText UDobotLiveLinkSourceFactory::GetSourceDisplayName() const
{
	return LOCTEXT("SourceDisplayName", "Dobot Robot");
}

FText UDobotLiveLinkSourceFactory::GetSourceTooltip() const
{
	return LOCTEXT("SourceTooltip", "Creates a connection to Dobot Nova 5 robot for camera tracking");
}

TSharedPtr<SWidget> UDobotLiveLinkSourceFactory::BuildCreationPanel(FOnLiveLinkSourceCreated OnLiveLinkSourceCreated) const
{
	return SNew(SDobotLiveLinkSourceFactory)
		.OnConnectionSettingsAccepted(FOnDobotLiveLinkConnectionSettingsAccepted::CreateUObject(this, &UDobotLiveLinkSourceFactory::OnConnectionSettingsAccepted, OnLiveLinkSourceCreated));
}

TSharedPtr<ILiveLinkSource> UDobotLiveLinkSourceFactory::CreateSource(const FString& ConnectionString) const
{
	FString IPAddress = TEXT("192.168.5.1");
	int32 Port = 30004;
	bool bTestMode = true;
	float DelayMs = 0.0f;
	FString SubjectName = TEXT("DobotCamera1");

	if (!ConnectionString.IsEmpty())
	{
		TArray<FString> Parts;
		ConnectionString.ParseIntoArray(Parts, TEXT(":"));

		if (Parts.Num() >= 1 && !Parts[0].IsEmpty())
		{
			IPAddress = Parts[0];
		}

		if (Parts.Num() >= 2)
		{
			Port = FCString::Atoi(*Parts[1]);
			if (Port <= 0) Port = 30004;
		}

		if (Parts.Num() >= 3)
		{
			bTestMode = Parts[2].Equals(TEXT("true"), ESearchCase::IgnoreCase);
		}

		if (Parts.Num() >= 4)
		{
			DelayMs = FCString::Atof(*Parts[3]);
			if (DelayMs < 0.0f) DelayMs = 0.0f;
		}

		if (Parts.Num() >= 5 && !Parts[4].IsEmpty())
		{
			SubjectName = Parts[4];
		}
	}

	return MakeShared<FDobotLiveLinkSource>(IPAddress, Port, bTestMode, DelayMs, SubjectName);
}

void UDobotLiveLinkSourceFactory::OnConnectionSettingsAccepted(TSharedPtr<ILiveLinkSource> Source, FString ConnectionString, FOnLiveLinkSourceCreated OnLiveLinkSourceCreated) const
{
	OnLiveLinkSourceCreated.ExecuteIfBound(Source, MoveTemp(ConnectionString));
}

#undef LOCTEXT_NAMESPACE