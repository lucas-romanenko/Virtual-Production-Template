#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"

class ILiveLinkSource;

DECLARE_DELEGATE_TwoParams(FOnDobotLiveLinkConnectionSettingsAccepted, TSharedPtr<ILiveLinkSource>, FString);

class SDobotLiveLinkSourceFactory : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDobotLiveLinkSourceFactory) {}
		SLATE_EVENT(FOnDobotLiveLinkConnectionSettingsAccepted, OnConnectionSettingsAccepted)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FString IPAddress;
	int32 Port;
	float DelayMs;
	FString SubjectName;

	FOnDobotLiveLinkConnectionSettingsAccepted OnConnectionSettingsAccepted;

	FReply OnCreateClicked();
	void OnIPAddressChanged(const FText& NewText);
	void OnPortChanged(const FText& NewText);
	void OnDelayChanged(const FText& NewText);
	void OnSubjectNameChanged(const FText& NewText);

	static int32 SourceCounter;
};