#include "DobotLiveLinkEditor.h"
#include "DobotLiveLinkSettings.h"
#include "DobotLiveLinkCameraComponent.h"
#include "DobotLiveLinkSource.h"
#include "CineCameraActor.h"
#include "MediaOutput.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Images/SImage.h"
#include "Framework/Docking/TabManager.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"

#if WITH_EDITOR
#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor.h"
#endif

#define LOCTEXT_NAMESPACE "FDobotLiveLinkEditorModule"

const FName FDobotLiveLinkEditorModule::TabName(TEXT("DobotLiveLinkPanel"));

static const FLinearColor LiveColor(0.0f, 1.0f, 0.0f);
static const FLinearColor DimColor(0.5f, 0.5f, 0.5f);

static FLinearColor GetStateColor(EDobotConnectionState S)
{
	switch (S) {
	case EDobotConnectionState::Connected: return FLinearColor::Green;
	case EDobotConnectionState::ConnectionLost: return FLinearColor(1, 0.5f, 0);
	case EDobotConnectionState::Reconnecting: return FLinearColor(1, 0.8f, 0);
	default: return FLinearColor::Red;
	}
}

static FText GetStateText(EDobotConnectionState S)
{
	switch (S) {
	case EDobotConnectionState::Connected: return LOCTEXT("SC", "Connected");
	case EDobotConnectionState::ConnectionLost: return LOCTEXT("SL", "Connection Lost");
	case EDobotConnectionState::Reconnecting: return LOCTEXT("SR", "Reconnecting...");
	default: return LOCTEXT("SN", "Disconnected");
	}
}

// Read-only data row: label left, green mono value right
static TSharedRef<SHorizontalBox> DataRow(const FText& Label, TFunction<FText()> Val, TFunction<bool()> Live)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.4f).VAlign(VAlign_Center)[SNew(STextBlock).Text(Label)]
		+ SHorizontalBox::Slot().FillWidth(0.6f).VAlign(VAlign_Center)
		[SNew(STextBlock).Text_Lambda(MoveTemp(Val)).Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
		.ColorAndOpacity_Lambda([Live]() { return FSlateColor(Live() ? LiveColor : DimColor); })];
}

void FDobotLiveLinkEditorModule::StartupModule()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TabName,
		FOnSpawnTab::CreateRaw(this, &FDobotLiveLinkEditorModule::OnSpawnTab))
		.SetDisplayName(LOCTEXT("Tab", "LiveLink Data"))
		.SetTooltipText(LOCTEXT("Tip", "FreeD Camera Tracking"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"))
		.SetAutoGenerateMenuEntry(false);

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([this]() {
		FToolMenuOwnerScoped Own(this);
		if (UToolMenu* M = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window.VirtualProduction"))
			M->FindOrAddSection("VIRTUALPRODUCTION").AddMenuEntry("DobotLiveLink",
				LOCTEXT("MT", "LiveLink Data"), LOCTEXT("MTT", "FreeD Camera Tracking"),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"),
				FUIAction(FExecuteAction::CreateLambda([this]() { FGlobalTabmanager::Get()->TryInvokeTab(TabName); })));
		}));
}

void FDobotLiveLinkEditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabName);
}

void FDobotLiveLinkEditorModule::RefreshCameraList()
{
	UDobotLiveLinkSettings* S = UDobotLiveLinkSettings::Get();
	CameraOptions.Empty();
	auto Cams = S->FindAllDobotCameras();
	for (auto* C : Cams) CameraOptions.Add(MakeShared<FString>(C->GetActorLabel()));
	if (Cams.Num() > 0 && !S->GetSelectedCamera()) S->SetSelectedCamera(Cams[0]);
	if (CameraComboBox.IsValid()) CameraComboBox->RefreshOptions();
	DeckLinkCameraOptions.Empty();
	DeckLinkCameraOptions.Add(MakeShared<FString>(TEXT("None")));
	for (auto* C : Cams) { auto* Comp = C->FindComponentByClass<UDobotLiveLinkCameraComponent>(); if (Comp) DeckLinkCameraOptions.Add(MakeShared<FString>(Comp->LiveLinkSubjectName.ToString())); }
}

TSharedRef<SVerticalBox> FDobotLiveLinkEditorModule::BuildDeckLinkPortRow(int32 I, UDobotLiveLinkSettings* S)
{
	return SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight().Padding(0, 3)
		[
			SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
				[SNew(STextBlock).Text(FText::Format(LOCTEXT("PL", "Port {0}:"), FText::AsNumber(I + 1))).Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))]
				+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(0, 0, 5, 0)
				[SNew(SComboBox<TSharedPtr<FString>>).OptionsSource(&DeckLinkCameraOptions)
				.OnSelectionChanged_Lambda([S, I](TSharedPtr<FString> Sel, ESelectInfo::Type) { if (Sel.IsValid()) S->SetPortCamera(I, *Sel == TEXT("None") ? FString() : *Sel); })
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> It) { return SNew(STextBlock).Text(FText::FromString(*It)); })
				.Content()[SNew(STextBlock).Text_Lambda([S, I]() { FString N = S->GetPortCamera(I); return N.IsEmpty() ? LOCTEXT("Nn", "None") : FText::FromString(N); })]]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 5, 0)
				[SNew(SBox).WidthOverride(10).HeightOverride(10)[SNew(SImage).ColorAndOpacity_Lambda([S, I]() { return S->IsPortActive(I) ? FLinearColor::Green : FLinearColor(0.3f, 0.3f, 0.3f); }).Image(FAppStyle::GetBrush("Icons.FilledCircle"))]]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 2, 0)[SNew(SButton).Text(LOCTEXT("Go", "Start")).OnClicked_Lambda([S, I]() { S->StartPortOutput(I); return FReply::Handled(); }).IsEnabled_Lambda([S, I]() { return !S->IsPortActive(I) && !S->GetPortCamera(I).IsEmpty(); })]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 2, 0)[SNew(SButton).Text(LOCTEXT("No", "Stop")).OnClicked_Lambda([S, I]() { S->StopPortOutput(I); return FReply::Handled(); }).IsEnabled_Lambda([S, I]() { return S->IsPortActive(I); })]
				+ SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(LOCTEXT("Cg", "Settings")).OnClicked_Lambda([S, I]() {
#if WITH_EDITOR
				UMediaOutput* O = S->GetOutputAssetForPort(I); if (!O) O = S->CreateOutputAssetForPort(I);
				if (O && GEditor) GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Cast<UObject>(O));
#endif
				return FReply::Handled(); })]
		];
}

TSharedRef<SDockTab> FDobotLiveLinkEditorModule::OnSpawnTab(const FSpawnTabArgs& Args)
{
	UDobotLiveLinkSettings* Settings = UDobotLiveLinkSettings::Get();
	RefreshCameraList();
	Settings->LoadCameraSettings();
	Settings->SetNumOutputPorts(Settings->GetNumOutputPorts());

	TSharedRef<SVerticalBox> DLPorts = SNew(SVerticalBox);
	DeckLinkPortsBox = DLPorts;
	for (int32 i = 0; i < Settings->GetNumOutputPorts(); i++)
		DLPorts->AddSlot().AutoHeight()[BuildDeckLinkPortRow(i, Settings)];

	auto GC = [Settings]() -> UDobotLiveLinkCameraComponent* { return Settings->GetSelectedDobotComponent(); };
	auto Live = [Settings]() -> bool { auto* C = Settings->GetSelectedDobotComponent(); return C && C->IsRobotConnected() && C->GetConnectedSource().IsValid(); };
	auto GF = [Settings]() -> FFreeDFrameData { auto* C = Settings->GetSelectedDobotComponent(); if (C) { auto S = C->GetConnectedSource(); if (S.IsValid()) return S->GetLatestFrame(); } return FFreeDFrameData(); };

	return SNew(SDockTab).TabRole(ETabRole::NomadTab)
		[SNew(SScrollBox) + SScrollBox::Slot().Padding(10)
		[SNew(SVerticalBox)

		// ===== HEADER =====
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 5)
		[SNew(STextBlock).Text(LOCTEXT("H", "LiveLink Data")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[SNew(SSeparator)]

		// ===== CAMERA =====
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 5)
		[SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
		[SNew(STextBlock).Text(LOCTEXT("CL", "Camera:")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))]
		+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(0, 0, 5, 0)
		[SAssignNew(CameraComboBox, SComboBox<TSharedPtr<FString>>).OptionsSource(&CameraOptions)
		.OnSelectionChanged_Lambda([this, Settings](TSharedPtr<FString> Sel, ESelectInfo::Type) { if (!Sel.IsValid()) return; for (auto* Cam : Settings->FindAllDobotCameras()) if (Cam->GetActorLabel() == *Sel) { Settings->SetSelectedCamera(Cam); break; } })
		.OnGenerateWidget_Lambda([](TSharedPtr<FString> I) { return SNew(STextBlock).Text(FText::FromString(*I)); })
		.Content()[SNew(STextBlock).Text_Lambda([Settings]() { auto* C = Settings->GetSelectedCamera(); return C ? FText::FromString(C->GetActorLabel()) : LOCTEXT("NC", "No Camera"); })]]
		+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 2, 0)
		[SNew(SButton).Text(LOCTEXT("Rf", "Refresh")).OnClicked_Lambda([this]() { RefreshCameraList(); return FReply::Handled(); })]
		+ SHorizontalBox::Slot().AutoWidth()
		[SNew(SButton).Text(LOCTEXT("Ad", "+ Add Camera")).OnClicked_Lambda([this, Settings]() { Settings->SpawnDobotCamera(); RefreshCameraList(); return FReply::Handled(); })]]

	+ SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 10)
		[SNew(SBox).Visibility_Lambda([Settings]() { return Settings->FindAllDobotCameras().Num() == 0 ? EVisibility::Visible : EVisibility::Collapsed; })
		[SNew(STextBlock).Text(LOCTEXT("Hint", "No tracked cameras. Click '+ Add Camera'.")).ColorAndOpacity(FSlateColor(FLinearColor(1, 0.7f, 0))).Justification(ETextJustify::Center)]]

		// Subject
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
		[SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
		[SNew(STextBlock).Text(LOCTEXT("SN", "Subject:")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))]
		+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
		[SNew(SEditableTextBox)
		.Text_Lambda([GC]() { auto* C = GC(); return C ? FText::FromName(C->LiveLinkSubjectName) : FText::GetEmpty(); })
		.OnTextCommitted_Lambda([Settings, GC](const FText& T, ETextCommit::Type) { auto* C = GC(); if (C && !C->IsRobotConnected()) { FString N = T.ToString(); if (Settings->IsSubjectNameAvailable(N, C)) C->LiveLinkSubjectName = FName(*N); } })
		.IsEnabled_Lambda([GC]() { auto* C = GC(); return C && !C->IsRobotConnected(); })]]

	// ===== FREED SOURCE =====
	+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 5)[SNew(SSeparator)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 5)
		[SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 5, 0)
		[SNew(STextBlock).Text(LOCTEXT("FH", "FreeD Source")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 5, 0)
		[SNew(SBox).WidthOverride(10).HeightOverride(10)[SNew(SImage)
		.ColorAndOpacity_Lambda([GC]() { auto* C = GC(); return GetStateColor(C ? C->GetConnectionState() : EDobotConnectionState::NoConnection); })
		.Image(FAppStyle::GetBrush("Icons.FilledCircle"))]]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[SNew(STextBlock)
		.Text_Lambda([GC]() { auto* C = GC(); return GetStateText(C ? C->GetConnectionState() : EDobotConnectionState::NoConnection); })
		.ColorAndOpacity_Lambda([GC]() { auto* C = GC(); return FSlateColor(GetStateColor(C ? C->GetConnectionState() : EDobotConnectionState::NoConnection)); })
		.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))]
		+ SHorizontalBox::Slot().FillWidth(1.0f)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[SNew(STextBlock)
		.Text_Lambda([Live, GC]() { if (!Live()) return FText::GetEmpty(); auto S = GC()->GetConnectedSource(); return S.IsValid() ? FText::FromString(FString::Printf(TEXT("%.0f pkt/s"), S->GetPacketsPerSecond())) : FText::GetEmpty(); })
		.Font(FCoreStyle::GetDefaultFontStyle("Mono", 8)).ColorAndOpacity(FSlateColor(LiveColor))]
		]

	+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 5)
		[SNew(SButton).Text(LOCTEXT("AS", "Add FreeD Source")).HAlign(HAlign_Center)
		.OnClicked_Lambda([GC]() { auto* C = GC(); if (C) C->bHasRobotConnection = true; return FReply::Handled(); })
		.Visibility_Lambda([GC]() { auto* C = GC(); return (C && !C->bHasRobotConnection) ? EVisibility::Visible : EVisibility::Collapsed; })]

		// Connection panel
		+ SVerticalBox::Slot().AutoHeight()
		[SNew(SVerticalBox).Visibility_Lambda([GC]() { auto* C = GC(); return (C && C->bHasRobotConnection) ? EVisibility::Visible : EVisibility::Collapsed; })

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
		[SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.35f).VAlign(VAlign_Center)[SNew(STextBlock).Text(LOCTEXT("IP", "Source IP"))]
		+ SHorizontalBox::Slot().FillWidth(0.65f)
		[SNew(SEditableTextBox)
		.Text_Lambda([GC]() { auto* C = GC(); return C ? FText::FromString(C->RobotIPAddress) : FText::GetEmpty(); })
		.OnTextCommitted_Lambda([GC](const FText& T, ETextCommit::Type) { auto* C = GC(); if (C && !C->IsRobotConnected()) C->RobotIPAddress = T.ToString(); })
		.IsEnabled_Lambda([GC]() { auto* C = GC(); return C && !C->IsRobotConnected(); })]]

	+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
		[SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.35f).VAlign(VAlign_Center)[SNew(STextBlock).Text(LOCTEXT("PT", "UDP Port"))]
		+ SHorizontalBox::Slot().FillWidth(0.65f)
		[SNew(SSpinBox<int32>).MinValue(1).MaxValue(65535)
		.Value_Lambda([GC]() -> int32 { auto* C = GC(); return C ? C->RobotPort : 40000; })
		.OnValueChanged_Lambda([GC](int32 V) { auto* C = GC(); if (C && !C->IsRobotConnected()) C->RobotPort = V; })
		.IsEnabled_Lambda([GC]() { auto* C = GC(); return C && !C->IsRobotConnected(); })]]

	+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
		[SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.35f).VAlign(VAlign_Center)[SNew(STextBlock).Text(LOCTEXT("DL", "Tracking Delay (ms)"))]
		+ SHorizontalBox::Slot().FillWidth(0.65f)
		[SNew(SSpinBox<float>).MinValue(0).MaxValue(10000)
		.Value_Lambda([GC]() -> float { auto* C = GC(); return C ? C->TrackingDelayMs : 0; })
		.OnValueChanged_Lambda([GC](float V) { auto* C = GC(); if (C) C->TrackingDelayMs = V; })]]

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 2)
		[SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)[SNew(STextBlock).Text(LOCTEXT("AC", "Auto-Connect:"))]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 14, 0)
		[SNew(SCheckBox).IsChecked_Lambda([Settings, GC]() { auto* C = GC(); return (C && Settings->ShouldAutoConnect(C->LiveLinkSubjectName.ToString())) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }).OnCheckStateChanged_Lambda([Settings, GC](ECheckBoxState S) { auto* C = GC(); if (C) Settings->SetAutoConnect(C->LiveLinkSubjectName.ToString(), S == ECheckBoxState::Checked); })]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)[SNew(STextBlock).Text(LOCTEXT("Tk", "Tracking:"))]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 14, 0)
		[SNew(SCheckBox).IsChecked_Lambda([GC]() { auto* C = GC(); return (C && C->bEnableTracking) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }).OnCheckStateChanged_Lambda([GC](ECheckBoxState S) { auto* C = GC(); if (C) { C->bEnableTracking = (S == ECheckBoxState::Checked); if (C->bEnableTracking) C->ResetTrackingOrigin(); } })]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)[SNew(STextBlock).Text(LOCTEXT("Fz", "Freeze:"))]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[SNew(SCheckBox).IsChecked_Lambda([GC]() { auto* C = GC(); return (C && C->bFreezeTracking) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }).OnCheckStateChanged_Lambda([GC](ECheckBoxState S) { auto* C = GC(); if (C) C->bFreezeTracking = (S == ECheckBoxState::Checked); }).IsEnabled_Lambda([GC]() { auto* C = GC(); return C && C->IsRobotConnected(); })]]

	+ SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 0)
		[SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 5, 0)
		[SNew(SButton).Text(LOCTEXT("Cn", "Connect")).OnClicked_Lambda([GC]() { auto* C = GC(); if (C) C->ConnectToRobot(); return FReply::Handled(); }).IsEnabled_Lambda([GC]() { auto* C = GC(); return C && C->GetConnectionState() != EDobotConnectionState::Connected; })]
		+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 5, 0)
		[SNew(SButton).Text(LOCTEXT("Dc", "Disconnect")).OnClicked_Lambda([GC]() { auto* C = GC(); if (C) C->DisconnectFromRobot(); return FReply::Handled(); }).IsEnabled_Lambda([GC]() { auto* C = GC(); return C && (C->GetConnectionState() == EDobotConnectionState::Connected || C->GetConnectionState() == EDobotConnectionState::Reconnecting); })]
		+ SHorizontalBox::Slot().AutoWidth()
		[SNew(SButton).Text(LOCTEXT("Rm", "Remove Source")).OnClicked_Lambda([GC]() { auto* C = GC(); if (C) { C->DisconnectFromRobot(); C->bHasRobotConnection = false; } return FReply::Handled(); })]]]

	// ===== POSITION & ROTATION (read-only from FreeD) =====
	+ SVerticalBox::Slot().AutoHeight().Padding(0, 15, 0, 5)[SNew(SSeparator)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 5)
		[SNew(STextBlock).Text(LOCTEXT("PR", "Position & Rotation")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))]

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)[DataRow(LOCTEXT("Pan", "Pan (Yaw)"), [GF, Live]() { return Live() ? FText::FromString(FString::Printf(TEXT("%.3f\xB0"), GF().Pan)) : LOCTEXT("D", "--"); }, Live)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)[DataRow(LOCTEXT("Tilt", "Tilt (Pitch)"), [GF, Live]() { return Live() ? FText::FromString(FString::Printf(TEXT("%.3f\xB0"), GF().Tilt)) : LOCTEXT("D", "--"); }, Live)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)[DataRow(LOCTEXT("Roll", "Roll"), [GF, Live]() { return Live() ? FText::FromString(FString::Printf(TEXT("%.3f\xB0"), GF().Roll)) : LOCTEXT("D", "--"); }, Live)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)[DataRow(LOCTEXT("X", "X (mm)"), [GF, Live]() { return Live() ? FText::FromString(FString::Printf(TEXT("%.1f"), GF().PosX_mm)) : LOCTEXT("D", "--"); }, Live)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)[DataRow(LOCTEXT("Y", "Y (mm)"), [GF, Live]() { return Live() ? FText::FromString(FString::Printf(TEXT("%.1f"), GF().PosY_mm)) : LOCTEXT("D", "--"); }, Live)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)[DataRow(LOCTEXT("Z", "Z (mm)"), [GF, Live]() { return Live() ? FText::FromString(FString::Printf(TEXT("%.1f"), GF().PosZ_mm)) : LOCTEXT("D", "--"); }, Live)]

		// ===== LENS DATA (read-only from FreeD) =====
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 15, 0, 5)[SNew(SSeparator)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 5)
		[SNew(STextBlock).Text(LOCTEXT("LD", "Lens Data")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))]

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)[DataRow(LOCTEXT("FL", "Focal Length"), [GF, Live]() { return Live() ? FText::FromString(FString::Printf(TEXT("%d"), GF().ZoomRaw)) : LOCTEXT("D", "--"); }, Live)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)[DataRow(LOCTEXT("Ap", "Aperture"), [GF, Live]() { return Live() ? FText::FromString(FString::Printf(TEXT("%d"), GF().IrisRaw)) : LOCTEXT("D", "--"); }, Live)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)[DataRow(LOCTEXT("FD", "Focus Distance"), [GF, Live]() { return Live() ? FText::FromString(FString::Printf(TEXT("%d"), GF().FocusRaw)) : LOCTEXT("D", "--"); }, Live)]

		// ===== SENSOR (editable) =====
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 15, 0, 5)[SNew(SSeparator)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 5)
		[SNew(STextBlock).Text(LOCTEXT("SH", "Sensor")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))]

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
		[SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.4f).VAlign(VAlign_Center)[SNew(STextBlock).Text(LOCTEXT("SW", "Width (mm)"))]
		+ SHorizontalBox::Slot().FillWidth(0.6f)
		[SNew(SSpinBox<float>).MinValue(1).MaxValue(100).Delta(0.01f).MinFractionalDigits(2).MaxFractionalDigits(3)
		.Value_Lambda([GC]() -> float { auto* C = GC(); return (C && C->CameraToControl) ? C->CameraToControl->Filmback.SensorWidth : 35.6f; })
		.OnValueChanged_Lambda([GC](float V) { auto* C = GC(); if (C && C->CameraToControl) { auto F = C->CameraToControl->Filmback; F.SensorWidth = V; C->CameraToControl->Filmback = F; } })]]

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
		[SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.4f).VAlign(VAlign_Center)[SNew(STextBlock).Text(LOCTEXT("SHt", "Height (mm)"))]
		+ SHorizontalBox::Slot().FillWidth(0.6f)
		[SNew(SSpinBox<float>).MinValue(1).MaxValue(100).Delta(0.01f).MinFractionalDigits(2).MaxFractionalDigits(3)
		.Value_Lambda([GC]() -> float { auto* C = GC(); return (C && C->CameraToControl) ? C->CameraToControl->Filmback.SensorHeight : 23.8f; })
		.OnValueChanged_Lambda([GC](float V) { auto* C = GC(); if (C && C->CameraToControl) { auto F = C->CameraToControl->Filmback; F.SensorHeight = V; C->CameraToControl->Filmback = F; } })]]

		// ===== DECKLINK =====
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 15, 0, 5)[SNew(SSeparator)]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 5)
		[SNew(STextBlock).Text(LOCTEXT("DH", "DeckLink Output")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 5)
		[SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)[SNew(STextBlock).Text(LOCTEXT("NO", "Outputs:"))]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[SNew(SSpinBox<int32>).MinValue(1).MaxValue(8).MinDesiredWidth(60)
		.Value_Lambda([Settings]() -> int32 { return Settings->GetNumOutputPorts(); })
		.OnValueCommitted_Lambda([this, Settings](int32 V, ETextCommit::Type) { Settings->SetNumOutputPorts(V); if (DeckLinkPortsBox.IsValid()) { auto B = DeckLinkPortsBox.Pin(); B->ClearChildren(); for (int32 i = 0; i < V; i++) B->AddSlot().AutoHeight()[BuildDeckLinkPortRow(i, Settings)]; } })]]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 10)[DLPorts]
		]];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FDobotLiveLinkEditorModule, DobotLiveLinkEditor)