#include "CineLinkEditor.h"
#include "CineLinkSettings.h"
#include "CineLinkCameraComponent.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"

#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SBoxPanel.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Framework/Docking/TabManager.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateColor.h"

#define LOCTEXT_NAMESPACE "CineLinkEditor"

const FName FCineLinkEditorModule::TabName = FName("CineLinkPanel");

IMPLEMENT_MODULE(FCineLinkEditorModule, CineLinkEditor)

void FCineLinkEditorModule::StartupModule()
{
	const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		TabName,
		FOnSpawnTab::CreateRaw(this, &FCineLinkEditorModule::OnSpawnTab))
		.SetDisplayName(LOCTEXT("TabTitle", "CineLink"))
		.SetMenuType(ETabSpawnerMenuType::Enabled)
		.SetGroup(MenuStructure.GetLevelEditorVirtualProductionCategory())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"));
}

void FCineLinkEditorModule::ShutdownModule()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabName);
}

// ---- Camera List ----

void FCineLinkEditorModule::RefreshCameraList()
{
	UCineLinkSettings* Settings = UCineLinkSettings::Get();
	TArray<ACineCameraActor*> Cameras = Settings->FindAllCineLinkCameras();

	CameraActors.Empty();
	CameraOptions.Empty();

	for (ACineCameraActor* Cam : Cameras)
	{
		CameraActors.Add(Cam);
		CameraOptions.Add(MakeShared<FString>(Cam->GetActorLabel()));
	}

	if (CameraComboBox.IsValid())
		CameraComboBox->RefreshOptions();
}

FReply FCineLinkEditorModule::OnAddCameraClicked()
{
	UCineLinkSettings* Settings = UCineLinkSettings::Get();
	Settings->SpawnCineLinkCamera();
	RefreshCameraList();
	return FReply::Handled();
}

// ---- Connection State Display ----

FSlateColor FCineLinkEditorModule::GetConnectionStateColor() const
{
	UCineLinkCameraComponent* Comp = UCineLinkSettings::Get()->GetSelectedCineLinkComponent();
	if (!Comp) return FSlateColor(FLinearColor::Gray);

	switch (Comp->GetConnectionState())
	{
	case ECineLinkConnectionState::Connected:      return FSlateColor(FLinearColor(0.f, 0.8f, 0.f));
	case ECineLinkConnectionState::ConnectionLost: return FSlateColor(FLinearColor(1.f, 0.5f, 0.f));
	default:                                        return FSlateColor(FLinearColor(0.8f, 0.f, 0.f));
	}
}

FText FCineLinkEditorModule::GetConnectionStateText() const
{
	UCineLinkCameraComponent* Comp = UCineLinkSettings::Get()->GetSelectedCineLinkComponent();
	if (!Comp) return LOCTEXT("NoCamera", "No camera selected");

	switch (Comp->GetConnectionState())
	{
	case ECineLinkConnectionState::Connected:      return LOCTEXT("Connected", "● Connected");
	case ECineLinkConnectionState::ConnectionLost: return LOCTEXT("ConnLost", "● Connection Lost");
	default:                                        return LOCTEXT("NoConn", "● No Connection");
	}
}

// ---- Connect / Disconnect ----

FReply FCineLinkEditorModule::OnConnectClicked()
{
	UCineLinkCameraComponent* Comp = UCineLinkSettings::Get()->GetSelectedCineLinkComponent();
	if (Comp) Comp->Connect();
	return FReply::Handled();
}

FReply FCineLinkEditorModule::OnDisconnectClicked()
{
	UCineLinkCameraComponent* Comp = UCineLinkSettings::Get()->GetSelectedCineLinkComponent();
	if (Comp) Comp->Disconnect();
	return FReply::Handled();
}

// ---- DeckLink Port Row ----

TSharedRef<SVerticalBox> FCineLinkEditorModule::BuildDeckLinkPortRow(int32 PortIndex, UCineLinkSettings* Settings)
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 2)
		[
			SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 8, 0)
				[
					SNew(STextBlock)
						.Text(FText::Format(LOCTEXT("PortLabel", "Port {0}"), FText::AsNumber(PortIndex + 1)))
						.MinDesiredWidth(50)
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SNew(SComboBox<TSharedPtr<FString>>)
						.OptionsSource(&CameraOptions)
						.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
							{
								return SNew(STextBlock).Text(FText::FromString(*Item));
							})
						.OnSelectionChanged_Lambda([Settings, PortIndex](TSharedPtr<FString> Selected, ESelectInfo::Type)
							{
								if (Selected.IsValid())
									Settings->SetPortCamera(PortIndex, *Selected);
							})
						[
							SNew(STextBlock)
								.Text_Lambda([Settings, PortIndex]()
									{
										FString Cam = Settings->GetPortCamera(PortIndex);
										return FText::FromString(Cam.IsEmpty() ? TEXT("None") : Cam);
									})
						]
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4, 0, 0, 0)
				[
					SNew(SButton)
						.Text_Lambda([Settings, PortIndex]()
							{
								return Settings->IsPortActive(PortIndex)
									? LOCTEXT("Stop", "Stop")
									: LOCTEXT("Start", "Start");
							})
						.OnClicked_Lambda([Settings, PortIndex]()
							{
								if (Settings->IsPortActive(PortIndex))
									Settings->StopPortOutput(PortIndex);
								else
									Settings->StartPortOutput(PortIndex);
								return FReply::Handled();
							})
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4, 0, 0, 0)
				[
					SNew(SButton)
						.Text(LOCTEXT("CreateAsset", "Create Asset"))
						.ToolTipText(LOCTEXT("CreateAssetTip", "Create a BlackmagicMediaOutput asset for this port"))
						.OnClicked_Lambda([Settings, PortIndex]()
							{
								Settings->CreateOutputAssetForPort(PortIndex);
								return FReply::Handled();
							})
				]
		];
}

// ---- Tab ----

TSharedRef<SDockTab> FCineLinkEditorModule::OnSpawnTab(const FSpawnTabArgs& SpawnTabArgs)
{
	UCineLinkSettings* Settings = UCineLinkSettings::Get();
	RefreshCameraList();

	// Auto-connect any cameras that have bAutoConnect set
	for (ACineCameraActor* Cam : CameraActors)
	{
		UCineLinkCameraComponent* Comp = Cam->FindComponentByClass<UCineLinkCameraComponent>();
		if (Comp && Comp->bAutoConnect && !Comp->IsConnected())
		{
			Comp->Connect();
		}
	}

	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SNew(SVerticalBox)

						// ── Header ──────────────────────────────────────────────
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(10, 10, 10, 4)
						[
							SNew(STextBlock)
								.Text(LOCTEXT("Title", "CineLink  —  Virtual Production"))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
						]

						+ SVerticalBox::Slot().AutoHeight().Padding(10, 0, 10, 6)[SNew(SSeparator)]

						// ── CAMERA ──────────────────────────────────────────────
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(10, 6, 10, 2)
						[
							SNew(STextBlock)
								.Text(LOCTEXT("CameraSection", "CAMERA"))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(10, 2)
						[
							SNew(SHorizontalBox)

								// Camera dropdown
								+ SHorizontalBox::Slot()
								.FillWidth(1.f)
								[
									SAssignNew(CameraComboBox, SComboBox<TSharedPtr<FString>>)
										.OptionsSource(&CameraOptions)
										.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
											{
												return SNew(STextBlock).Text(FText::FromString(*Item));
											})
										.OnSelectionChanged_Lambda([Settings, this](TSharedPtr<FString> Selected, ESelectInfo::Type)
											{
												if (!Selected.IsValid()) return;
												for (ACineCameraActor* Cam : CameraActors)
												{
													if (Cam->GetActorLabel() == *Selected)
													{
														Settings->SetSelectedCamera(Cam);
														break;
													}
												}
											})
										[
											SNew(STextBlock)
												.Text_Lambda([Settings]()
													{
														ACineCameraActor* Cam = Settings->GetSelectedCamera();
														return Cam
															? FText::FromString(Cam->GetActorLabel())
															: FText::FromString(TEXT("Select a camera..."));
													})
										]
								]

							// Add Camera
							+ SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(4, 0, 0, 0)
								[
									SNew(SButton)
										.Text(LOCTEXT("AddCamera", "+ Add Camera"))
										.OnClicked_Raw(this, &FCineLinkEditorModule::OnAddCameraClicked)
								]

								// Refresh
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(4, 0, 0, 0)
								[
									SNew(SButton)
										.Text(LOCTEXT("Refresh", "↻"))
										.ToolTipText(LOCTEXT("RefreshTip", "Refresh camera list"))
										.OnClicked_Lambda([this]()
											{
												RefreshCameraList();
												return FReply::Handled();
											})
								]
						]

					+ SVerticalBox::Slot().AutoHeight().Padding(10, 8, 10, 0)[SNew(SSeparator)]

						// ── LIVELINK CONNECTION ─────────────────────────────────
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(10, 8, 10, 2)
						[
							SNew(STextBlock)
								.Text(LOCTEXT("ConnectionSection", "LIVELINK CONNECTION"))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
						]

						// IP
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(10, 3)
						[
							SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(0.3f).VAlign(VAlign_Center)
								[
									SNew(STextBlock).Text(LOCTEXT("IP", "IP Address"))
								]
								+ SHorizontalBox::Slot().FillWidth(0.7f)
								[
									SNew(SEditableTextBox)
										.Text_Lambda([Settings]()
											{
												UCineLinkCameraComponent* Comp = Settings->GetSelectedCineLinkComponent();
												return FText::FromString(Comp ? Comp->LensmasterIP : TEXT(""));
											})
										.OnTextCommitted_Lambda([Settings](const FText& Val, ETextCommit::Type)
											{
												UCineLinkCameraComponent* Comp = Settings->GetSelectedCineLinkComponent();
												if (Comp) Comp->LensmasterIP = Val.ToString();
											})
										.IsEnabled_Lambda([Settings]() { return Settings->GetSelectedCineLinkComponent() != nullptr; })
								]
						]

					// Port
					+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(10, 3)
						[
							SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(0.3f).VAlign(VAlign_Center)
								[
									SNew(STextBlock).Text(LOCTEXT("Port", "Port"))
								]
								+ SHorizontalBox::Slot().FillWidth(0.7f)
								[
									SNew(SSpinBox<int32>)
										.MinValue(1).MaxValue(65535)
										.Value_Lambda([Settings]()
											{
												UCineLinkCameraComponent* Comp = Settings->GetSelectedCineLinkComponent();
												return Comp ? Comp->LensmasterPort : 40000;
											})
										.OnValueCommitted_Lambda([Settings](int32 Val, ETextCommit::Type)
											{
												UCineLinkCameraComponent* Comp = Settings->GetSelectedCineLinkComponent();
												if (Comp) Comp->LensmasterPort = Val;
											})
										.IsEnabled_Lambda([Settings]() { return Settings->GetSelectedCineLinkComponent() != nullptr; })
								]
						]

					// Subject Name
					+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(10, 3)
						[
							SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(0.3f).VAlign(VAlign_Center)
								[
									SNew(STextBlock).Text(LOCTEXT("SubjectName", "Subject Name"))
								]
								+ SHorizontalBox::Slot().FillWidth(0.7f)
								[
									SNew(SEditableTextBox)
										.Text_Lambda([Settings]()
											{
												UCineLinkCameraComponent* Comp = Settings->GetSelectedCineLinkComponent();
												return Comp ? FText::FromName(Comp->LiveLinkSubjectName) : FText::GetEmpty();
											})
										.OnTextCommitted_Lambda([Settings](const FText& Val, ETextCommit::Type)
											{
												UCineLinkCameraComponent* Comp = Settings->GetSelectedCineLinkComponent();
												if (Comp) Comp->LiveLinkSubjectName = FName(*Val.ToString());
											})
										.HintText(LOCTEXT("SubjectHint", "e.g. Camera 1"))
										.IsEnabled_Lambda([Settings]() { return Settings->GetSelectedCineLinkComponent() != nullptr; })
								]
						]

					// Status + Connect / Disconnect
					+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(10, 6)
						[
							SNew(SHorizontalBox)

								+ SHorizontalBox::Slot()
								.FillWidth(1.f)
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
										.Text_Lambda([this]() { return GetConnectionStateText(); })
										.ColorAndOpacity_Lambda([this]() { return GetConnectionStateColor(); })
										.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
								]

								+ SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(4, 0)
								[
									SNew(SButton)
										.Text(LOCTEXT("Connect", "Connect"))
										.OnClicked_Raw(this, &FCineLinkEditorModule::OnConnectClicked)
										.IsEnabled_Lambda([Settings]()
											{
												UCineLinkCameraComponent* Comp = Settings->GetSelectedCineLinkComponent();
												return Comp && !Comp->IsConnected();
											})
								]

							+ SHorizontalBox::Slot()
								.AutoWidth()
								[
									SNew(SButton)
										.Text(LOCTEXT("Disconnect", "Disconnect"))
										.OnClicked_Raw(this, &FCineLinkEditorModule::OnDisconnectClicked)
										.IsEnabled_Lambda([Settings]()
											{
												UCineLinkCameraComponent* Comp = Settings->GetSelectedCineLinkComponent();
												return Comp && Comp->IsConnected();
											})
								]
						]

					+ SVerticalBox::Slot().AutoHeight().Padding(10, 4, 10, 0)[SNew(SSeparator)]

						// ── CAMERA SETTINGS ─────────────────────────────────────
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(10, 8, 10, 4)
						[
							SNew(STextBlock)
								.Text(LOCTEXT("CamSettings", "CAMERA SETTINGS"))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
						]

						// Helper macro columns: Label | Input | Tag
						// Sensor Width
						+SVerticalBox::Slot().AutoHeight().Padding(10, 3)
						[
							SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(0.35f).VAlign(VAlign_Center)
								[SNew(STextBlock).Text(LOCTEXT("SensorW", "Sensor Width (mm)"))]
								+ SHorizontalBox::Slot().FillWidth(0.4f)
								[
									SNew(SSpinBox<float>)
										.MinValue(1.f).MaxValue(100.f).Delta(0.1f)
										.Value_Lambda([Settings]() { return Settings->SensorWidth; })
										.OnValueCommitted_Lambda([Settings](float Val, ETextCommit::Type)
											{
												Settings->SensorWidth = Val;
												Settings->ApplyCameraSettings();
											})
										.IsEnabled_Lambda([Settings]() { return Settings->GetSelectedCineLinkComponent() != nullptr; })
								]
							+ SHorizontalBox::Slot().FillWidth(0.25f).VAlign(VAlign_Center).Padding(6, 0, 0, 0)
								[
									SNew(STextBlock)
										.Text(LOCTEXT("ManualTag", "Set manually"))
										.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
										.Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))
								]
						]

					// Sensor Height
					+ SVerticalBox::Slot().AutoHeight().Padding(10, 3)
						[
							SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(0.35f).VAlign(VAlign_Center)
								[SNew(STextBlock).Text(LOCTEXT("SensorH", "Sensor Height (mm)"))]
								+ SHorizontalBox::Slot().FillWidth(0.4f)
								[
									SNew(SSpinBox<float>)
										.MinValue(1.f).MaxValue(100.f).Delta(0.1f)
										.Value_Lambda([Settings]() { return Settings->SensorHeight; })
										.OnValueCommitted_Lambda([Settings](float Val, ETextCommit::Type)
											{
												Settings->SensorHeight = Val;
												Settings->ApplyCameraSettings();
											})
										.IsEnabled_Lambda([Settings]() { return Settings->GetSelectedCineLinkComponent() != nullptr; })
								]
							+ SHorizontalBox::Slot().FillWidth(0.25f).VAlign(VAlign_Center).Padding(6, 0, 0, 0)
								[
									SNew(STextBlock)
										.Text(LOCTEXT("ManualTag2", "Set manually"))
										.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
										.Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))
								]
						]

					// Focal Length (read-only display, driven by FreeD)
					+ SVerticalBox::Slot().AutoHeight().Padding(10, 3)
						[
							SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(0.35f).VAlign(VAlign_Center)
								[SNew(STextBlock).Text(LOCTEXT("FocalLen", "Focal Length (mm)"))]
								+ SHorizontalBox::Slot().FillWidth(0.4f)
								[
									SNew(SSpinBox<float>)
										.MinValue(1.f).MaxValue(1000.f).Delta(0.1f)
										.Value_Lambda([Settings]()
											{
												ACineCameraActor* Cam = Settings->GetSelectedCamera();
												if (!Cam) return 24.0f;
												UCineCameraComponent* C = Cam->GetCineCameraComponent();
												return C ? C->CurrentFocalLength : 24.0f;
											})
										.OnValueCommitted_Lambda([Settings](float Val, ETextCommit::Type)
											{
												// Allow manual override as fallback
												ACineCameraActor* Cam = Settings->GetSelectedCamera();
												if (!Cam) return;
												UCineCameraComponent* C = Cam->GetCineCameraComponent();
												if (C) C->CurrentFocalLength = Val;
											})
										.IsEnabled_Lambda([Settings]() { return Settings->GetSelectedCineLinkComponent() != nullptr; })
								]
							+ SHorizontalBox::Slot().FillWidth(0.25f).VAlign(VAlign_Center).Padding(6, 0, 0, 0)
								[
									SNew(STextBlock)
										.Text(LOCTEXT("FreeDTag", "From FreeD"))
										.ColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.7f, 0.3f)))
										.Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))
								]
						]

					// Aperture (read-only display, driven by FreeD)
					+ SVerticalBox::Slot().AutoHeight().Padding(10, 3)
						[
							SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(0.35f).VAlign(VAlign_Center)
								[SNew(STextBlock).Text(LOCTEXT("Aperture", "Aperture (f-stop)"))]
								+ SHorizontalBox::Slot().FillWidth(0.4f)
								[
									SNew(SSpinBox<float>)
										.MinValue(0.7f).MaxValue(64.f).Delta(0.1f)
										.Value_Lambda([Settings]()
											{
												ACineCameraActor* Cam = Settings->GetSelectedCamera();
												if (!Cam) return 2.8f;
												UCineCameraComponent* C = Cam->GetCineCameraComponent();
												return C ? C->CurrentAperture : 2.8f;
											})
										.OnValueCommitted_Lambda([Settings](float Val, ETextCommit::Type)
											{
												ACineCameraActor* Cam = Settings->GetSelectedCamera();
												if (!Cam) return;
												UCineCameraComponent* C = Cam->GetCineCameraComponent();
												if (C) C->CurrentAperture = Val;
											})
										.IsEnabled_Lambda([Settings]() { return Settings->GetSelectedCineLinkComponent() != nullptr; })
								]
							+ SHorizontalBox::Slot().FillWidth(0.25f).VAlign(VAlign_Center).Padding(6, 0, 0, 0)
								[
									SNew(STextBlock)
										.Text(LOCTEXT("FreeDTag2", "From FreeD"))
										.ColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.7f, 0.3f)))
										.Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))
								]
						]

					// Focus Distance (read-only display, driven by FreeD)
					+ SVerticalBox::Slot().AutoHeight().Padding(10, 3)
						[
							SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(0.35f).VAlign(VAlign_Center)
								[SNew(STextBlock).Text(LOCTEXT("FocusDist", "Focus Distance (cm)"))]
								+ SHorizontalBox::Slot().FillWidth(0.4f)
								[
									SNew(SSpinBox<float>)
										.MinValue(0.f).MaxValue(100000.f).Delta(1.f)
										.Value_Lambda([Settings]()
											{
												ACineCameraActor* Cam = Settings->GetSelectedCamera();
												if (!Cam) return 100.0f;
												UCineCameraComponent* C = Cam->GetCineCameraComponent();
												return C ? C->FocusSettings.ManualFocusDistance : 100.0f;
											})
										.OnValueCommitted_Lambda([Settings](float Val, ETextCommit::Type)
											{
												ACineCameraActor* Cam = Settings->GetSelectedCamera();
												if (!Cam) return;
												UCineCameraComponent* C = Cam->GetCineCameraComponent();
												if (C) C->FocusSettings.ManualFocusDistance = Val;
											})
										.IsEnabled_Lambda([Settings]() { return Settings->GetSelectedCineLinkComponent() != nullptr; })
								]
							+ SHorizontalBox::Slot().FillWidth(0.25f).VAlign(VAlign_Center).Padding(6, 0, 0, 0)
								[
									SNew(STextBlock)
										.Text(LOCTEXT("FreeDTag3", "From FreeD"))
										.ColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.7f, 0.3f)))
										.Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))
								]
						]

					+ SVerticalBox::Slot().AutoHeight().Padding(10, 8, 10, 0)[SNew(SSeparator)]

						// ── DECKLINK OUTPUT ──────────────────────────────────────
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(10, 8, 10, 2)
						[
							SNew(STextBlock)
								.Text(LOCTEXT("DeckLink", "DECKLINK OUTPUT"))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(10, 3)
						[
							SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(0.5f).VAlign(VAlign_Center)
								[
									SNew(STextBlock).Text(LOCTEXT("NumOutputs", "Number of Outputs"))
								]
								+ SHorizontalBox::Slot().FillWidth(0.5f)
								[
									SNew(SSpinBox<int32>)
										.MinValue(1).MaxValue(8)
										.Value_Lambda([Settings]() { return Settings->GetNumOutputPorts(); })
										.OnValueCommitted_Lambda([Settings, this](int32 Val, ETextCommit::Type)
											{
												Settings->SetNumOutputPorts(Val);
											})
								]
						]

					+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(10, 4)
						[
							SAssignNew(DeckLinkPortsBox, SVerticalBox)
						]

						+ SVerticalBox::Slot().AutoHeight().Padding(10, 12)[SNew(SSeparator)]
				]
		];
}

#undef LOCTEXT_NAMESPACE