#include "SDobotLiveLinkSourceFactory.h"
#include "DobotLiveLinkSource.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

int32 SDobotLiveLinkSourceFactory::SourceCounter = 0;

#define LOCTEXT_NAMESPACE "SDobotLiveLinkSourceFactory"

void SDobotLiveLinkSourceFactory::Construct(const FArguments& InArgs)
{
	OnConnectionSettingsAccepted = InArgs._OnConnectionSettingsAccepted;

	IPAddress = TEXT("192.168.5.1");
	Port = 30004;
	bTestMode = true;
	DelayMs = 0.0f;
	SourceCounter++;
	if (SourceCounter == 1)
	{
		SubjectName = TEXT("DobotCamera");
	}
	else
	{
		SubjectName = FString::Printf(TEXT("DobotCamera%d"), SourceCounter);
	}

	ChildSlot
		[
			SNew(SBox)
				.WidthOverride(400)
				[
					SNew(SVerticalBox)

						// IP Address Row
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(2)
						[
							SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.FillWidth(0.4f)
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
										.Text(LOCTEXT("IPAddress", "Robot IP Address"))
								]
								+ SHorizontalBox::Slot()
								.FillWidth(0.6f)
								[
									SNew(SEditableTextBox)
										.Text(FText::FromString(IPAddress))
										.OnTextChanged(this, &SDobotLiveLinkSourceFactory::OnIPAddressChanged)
								]
						]

					// Port Row
					+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(2)
						[
							SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.FillWidth(0.4f)
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
										.Text(LOCTEXT("Port", "Port Number"))
								]
								+ SHorizontalBox::Slot()
								.FillWidth(0.6f)
								[
									SNew(SEditableTextBox)
										.Text(FText::FromString(FString::FromInt(Port)))
										.OnTextChanged(this, &SDobotLiveLinkSourceFactory::OnPortChanged)
								]
						]

					// Test Mode Row
					+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(2)
						[
							SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.FillWidth(0.4f)
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
										.Text(LOCTEXT("TestMode", "Test Mode"))
								]
								+ SHorizontalBox::Slot()
								.FillWidth(0.6f)
								[
									SNew(SCheckBox)
										.IsChecked(bTestMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
										.OnCheckStateChanged(this, &SDobotLiveLinkSourceFactory::OnTestModeChanged)
								]
						]

					// Subject Name Row
					+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(2)
						[
							SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.FillWidth(0.4f)
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
										.Text(LOCTEXT("SubjectName", "Subject Name"))
								]
								+ SHorizontalBox::Slot()
								.FillWidth(0.6f)
								[
									SNew(SEditableTextBox)
										.Text(FText::FromString(SubjectName))
										.OnTextChanged(this, &SDobotLiveLinkSourceFactory::OnSubjectNameChanged)
								]
						]

					// Delay Row
					+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(2)
						[
							SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.FillWidth(0.4f)
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
										.Text(LOCTEXT("Delay", "Tracking Delay (ms)"))
								]
								+ SHorizontalBox::Slot()
								.FillWidth(0.6f)
								[
									SNew(SEditableTextBox)
										.Text(FText::FromString(TEXT("0")))
										.OnTextChanged(this, &SDobotLiveLinkSourceFactory::OnDelayChanged)
								]
						]

					// Create Button
					+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Right)
						.Padding(2, 10, 2, 2)
						[
							SNew(SButton)
								.Text(LOCTEXT("Create", "Create"))
								.OnClicked(this, &SDobotLiveLinkSourceFactory::OnCreateClicked)
						]
				]
		];
}

void SDobotLiveLinkSourceFactory::OnIPAddressChanged(const FText& NewText)
{
	IPAddress = NewText.ToString();
}

void SDobotLiveLinkSourceFactory::OnPortChanged(const FText& NewText)
{
	Port = FCString::Atoi(*NewText.ToString());
	if (Port <= 0) Port = 30004;
}

void SDobotLiveLinkSourceFactory::OnTestModeChanged(ECheckBoxState NewState)
{
	bTestMode = (NewState == ECheckBoxState::Checked);
}

void SDobotLiveLinkSourceFactory::OnDelayChanged(const FText& NewText)
{
	DelayMs = FCString::Atof(*NewText.ToString());
	if (DelayMs < 0.0f) DelayMs = 0.0f;
	if (DelayMs > 10000.0f) DelayMs = 10000.0f;
}

void SDobotLiveLinkSourceFactory::OnSubjectNameChanged(const FText& NewText)
{
	SubjectName = NewText.ToString();
}

FReply SDobotLiveLinkSourceFactory::OnCreateClicked()
{
	if (OnConnectionSettingsAccepted.IsBound())
	{
		TSharedPtr<FDobotLiveLinkSource> Source = MakeShared<FDobotLiveLinkSource>(IPAddress, Port, bTestMode, DelayMs, SubjectName);
		FString ConnectionString = FString::Printf(TEXT("%s:%d:%s:%.0f:%s"), *IPAddress, Port, bTestMode ? TEXT("true") : TEXT("false"), DelayMs, *SubjectName);
		OnConnectionSettingsAccepted.Execute(Source, ConnectionString);
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE