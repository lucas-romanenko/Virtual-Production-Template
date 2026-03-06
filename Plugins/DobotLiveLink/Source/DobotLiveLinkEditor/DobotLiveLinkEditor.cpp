#include "DobotLiveLinkEditor.h"
#include "DobotLiveLinkSettings.h"
#include "DobotLiveLinkCameraComponent.h"
#include "CineCameraActor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Images/SImage.h"
#include "Framework/Docking/TabManager.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "FDobotLiveLinkEditorModule"

const FName FDobotLiveLinkEditorModule::TabName(TEXT("DobotLiveLinkPanel"));

void FDobotLiveLinkEditorModule::StartupModule()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TabName,
		FOnSpawnTab::CreateRaw(this, &FDobotLiveLinkEditorModule::OnSpawnTab))
		.SetDisplayName(LOCTEXT("TabTitle", "Dobot LiveLink"))
		.SetTooltipText(LOCTEXT("TabTooltip", "Dobot LiveLink - Camera Tracking & Output Settings"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"))
		.SetAutoGenerateMenuEntry(false);

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([this]()
		{
			FToolMenuOwnerScoped OwnerScoped(this);

			UToolMenu* VPMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window.VirtualProduction");
			if (VPMenu)
			{
				FToolMenuSection& Section = VPMenu->FindOrAddSection("VIRTUALPRODUCTION");
				Section.AddMenuEntry(
					"DobotLiveLink",
					LOCTEXT("MenuEntryTitle", "Dobot LiveLink"),
					LOCTEXT("MenuEntryTooltip", "Open Dobot LiveLink Camera Tracking Settings"),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"),
					FUIAction(FExecuteAction::CreateLambda([this]()
						{
							FGlobalTabmanager::Get()->TryInvokeTab(TabName);
						}))
				);
			}
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
	UDobotLiveLinkSettings* Settings = UDobotLiveLinkSettings::Get();

	CameraOptions.Empty();
	TArray<ACineCameraActor*> Cameras = Settings->FindAllDobotCameras();
	for (ACineCameraActor* Cam : Cameras)
	{
		CameraOptions.Add(MakeShared<FString>(Cam->GetActorLabel()));
	}

	// Auto-select first camera if none selected
	if (Cameras.Num() > 0 && !Settings->GetSelectedCamera())
	{
		Settings->SetSelectedCamera(Cameras[0]);
	}

	if (CameraComboBox.IsValid())
	{
		CameraComboBox->RefreshOptions();
	}
}

TSharedRef<SDockTab> FDobotLiveLinkEditorModule::OnSpawnTab(const FSpawnTabArgs& SpawnTabArgs)
{
	UDobotLiveLinkSettings* Settings = UDobotLiveLinkSettings::Get();

	// Refresh cameras on open
	RefreshCameraList();
	Settings->LoadCameraSettings();

	// Details View for camera settings only
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.bShowOptions = false;
	DetailsViewArgs.bShowPropertyMatrixButton = false;
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.bLockable = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetObject(Settings);
	CachedDetailsView = DetailsView;

	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SScrollBox)
				+ SScrollBox::Slot()
				.Padding(10.0f)
				[
					SNew(SVerticalBox)

						// ========== HEADER ==========
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0, 0, 0, 5)
						[
							SNew(STextBlock)
								.Text(LOCTEXT("PanelHeader", "Dobot LiveLink Settings"))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0, 0, 0, 5)
						[
							SNew(SSeparator)
						]

						// ========== CAMERA SELECTOR ==========
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0, 0, 0, 5)
						[
							SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(0, 0, 10, 0)
								[
									SNew(STextBlock)
										.Text(LOCTEXT("CameraLabel", "Active Camera:"))
										.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
								]
								+ SHorizontalBox::Slot()
								.FillWidth(1.0f)
								.VAlign(VAlign_Center)
								.Padding(0, 0, 5, 0)
								[
									SAssignNew(CameraComboBox, SComboBox<TSharedPtr<FString>>)
										.OptionsSource(&CameraOptions)
										.OnSelectionChanged_Lambda([this, Settings](TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
											{
												if (!NewSelection.IsValid()) return;
												TArray<ACineCameraActor*> CurrentCameras = Settings->FindAllDobotCameras();
												for (ACineCameraActor* Cam : CurrentCameras)
												{
													if (Cam->GetActorLabel() == *NewSelection)
													{
														Settings->SetSelectedCamera(Cam);
														if (CachedDetailsView.IsValid())
														{
															CachedDetailsView.Pin()->SetObject(Settings, true);
														}
														break;
													}
												}
											})
										.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
											{
												return SNew(STextBlock).Text(FText::FromString(*Item));
											})
										.Content()
										[
											SNew(STextBlock)
												.Text_Lambda([Settings]()
													{
														ACineCameraActor* Cam = Settings->GetSelectedCamera();
														if (Cam)
														{
															return FText::FromString(Cam->GetActorLabel());
														}
														return LOCTEXT("NoCameraSelected", "No Camera Selected");
													})
										]
								]
							+ SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(0, 0, 2, 0)
								[
									SNew(SButton)
										.Text(LOCTEXT("RefreshCameras", "Refresh"))
										.OnClicked_Lambda([this]()
											{
												RefreshCameraList();
												return FReply::Handled();
											})
								]
							+ SHorizontalBox::Slot()
								.AutoWidth()
								[
									SNew(SButton)
										.Text(LOCTEXT("AddCamera", "+ Add Camera"))
										.OnClicked_Lambda([this, Settings]()
											{
												Settings->SpawnDobotCamera();
												RefreshCameraList();
												if (CachedDetailsView.IsValid())
												{
													CachedDetailsView.Pin()->SetObject(Settings, true);
												}
												return FReply::Handled();
											})
								]
						]

					// No cameras found message
					+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0, 5, 0, 10)
						[
							SNew(SBox)
								.Visibility_Lambda([Settings]()
									{
										return Settings->FindAllDobotCameras().Num() == 0
											? EVisibility::Visible
											: EVisibility::Collapsed;
									})
								[
									SNew(STextBlock)
										.Text(LOCTEXT("NoCamerasMsg", "No tracked cameras found in the level.\nClick '+ Add Camera' to create one."))
										.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.7f, 0.0f)))
										.Justification(ETextJustify::Center)
								]
						]

					// ========== SUBJECT NAME (under camera selector) ==========
					+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0, 0, 0, 10)
						[
							SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(0, 0, 10, 0)
								[
									SNew(STextBlock)
										.Text(LOCTEXT("SubjectLabel", "Subject Name:"))
										.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
								]
								+ SHorizontalBox::Slot()
								.FillWidth(1.0f)
								.VAlign(VAlign_Center)
								[
									SNew(SEditableTextBox)
										.Text_Lambda([Settings]()
											{
												UDobotLiveLinkCameraComponent* Comp = Settings->GetSelectedDobotComponent();
												return Comp ? FText::FromName(Comp->LiveLinkSubjectName) : FText::GetEmpty();
											})
										.OnTextCommitted_Lambda([Settings](const FText& NewText, ETextCommit::Type CommitType)
											{
												UDobotLiveLinkCameraComponent* Comp = Settings->GetSelectedDobotComponent();
												if (Comp && !Comp->IsRobotConnected())
												{
													Comp->LiveLinkSubjectName = FName(*NewText.ToString());
												}
											})
										.IsEnabled_Lambda([Settings]()
											{
												UDobotLiveLinkCameraComponent* Comp = Settings->GetSelectedDobotComponent();
												return Comp && !Comp->IsRobotConnected();
											})
								]
						]

					+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0, 0, 0, 5)
						[
							SNew(SSeparator)
						]

						// ========== CAMERA SETTINGS (Details View) ==========
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							DetailsView
						]

						// ========== ROBOT CONNECTION SECTION ==========
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0, 10, 0, 5)
						[
							SNew(SSeparator)
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0, 0, 0, 5)
						[
							SNew(STextBlock)
								.Text(LOCTEXT("ConnectionHeader", "Robot Connection"))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
						]

						// "Add Connection" button - visible when no connection configured
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0, 0, 0, 5)
						[
							SNew(SButton)
								.Text(LOCTEXT("AddConnection", "Add Robot Connection"))
								.HAlign(HAlign_Center)
								.OnClicked_Lambda([Settings]()
									{
										UDobotLiveLinkCameraComponent* Comp = Settings->GetSelectedDobotComponent();
										if (Comp)
										{
											Comp->bHasRobotConnection = true;
										}
										return FReply::Handled();
									})
								.Visibility_Lambda([Settings]()
									{
										UDobotLiveLinkCameraComponent* Comp = Settings->GetSelectedDobotComponent();
										if (Comp && !Comp->bHasRobotConnection)
										{
											return EVisibility::Visible;
										}
										return EVisibility::Collapsed;
									})
						]

					// Connection settings - visible when connection is configured
					+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SVerticalBox)
								.Visibility_Lambda([Settings]()
									{
										UDobotLiveLinkCameraComponent* Comp = Settings->GetSelectedDobotComponent();
										if (Comp && Comp->bHasRobotConnection)
										{
											return EVisibility::Visible;
										}
										return EVisibility::Collapsed;
									})

								// IP Address
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(0, 2)
								[
									SNew(SHorizontalBox)
										+ SHorizontalBox::Slot()
										.FillWidth(0.4f)
										.VAlign(VAlign_Center)
										[
											SNew(STextBlock)
												.Text(LOCTEXT("IPLabel", "Robot IP Address"))
										]
										+ SHorizontalBox::Slot()
										.FillWidth(0.6f)
										[
											SNew(SEditableTextBox)
												.Text_Lambda([Settings]()
													{
														UDobotLiveLinkCameraComponent* Comp = Settings->GetSelectedDobotComponent();
														return Comp ? FText::FromString(Comp->RobotIPAddress) : FText::GetEmpty();
													})
												.OnTextCommitted_Lambda([Settings](const FText& NewText, ETextCommit::Type CommitType)
													{
														UDobotLiveLinkCameraComponent* Comp = Settings->GetSelectedDobotComponent();
														if (Comp && !Comp->IsRobotConnected())
														{
															Comp->RobotIPAddress = NewText.ToString();
														}
													})
												.IsEnabled_Lambda([Settings]()
													{
														UDobotLiveLinkCameraComponent* Comp = Settings->GetSelectedDobotComponent();
														return Comp && !Comp->IsRobotConnected();
													})
										]
								]

							// Port
							+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(0, 2)
								[
									SNew(SHorizontalBox)
										+ SHorizontalBox::Slot()
										.FillWidth(0.4f)
										.VAlign(VAlign_Center)
										[
											SNew(STextBlock)
												.Text(LOCTEXT("PortLabel", "Robot Port"))
										]
										+ SHorizontalBox::Slot()
										.FillWidth(0.6f)
										[
											SNew(SSpinBox<int32>)
												.MinValue(1)
												.MaxValue(65535)
												.Value_Lambda([Settings]() -> int32
													{
														UDobotLiveLinkCameraComponent* Comp = Settings->GetSelectedDobotComponent();
														return Comp ? Comp->RobotPort : 30004;
													})
												.OnValueChanged_Lambda([Settings](int32 NewValue)
													{
														UDobotLiveLinkCameraComponent* Comp = Settings->GetSelectedDobotComponent();
														if (Comp && !Comp->IsRobotConnected())
														{
															Comp->RobotPort = NewValue;
														}
													})
												.IsEnabled_Lambda([Settings]()
													{
														UDobotLiveLinkCameraComponent* Comp = Settings->GetSelectedDobotComponent();
														return Comp && !Comp->IsRobotConnected();
													})
										]
								]

							// Tracking Delay
							+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(0, 2)
								[
									SNew(SHorizontalBox)
										+ SHorizontalBox::Slot()
										.FillWidth(0.4f)
										.VAlign(VAlign_Center)
										[
											SNew(STextBlock)
												.Text(LOCTEXT("DelayLabel", "Tracking Delay (ms)"))
										]
										+ SHorizontalBox::Slot()
										.FillWidth(0.6f)
										[
											SNew(SSpinBox<float>)
												.MinValue(0.0f)
												.MaxValue(10000.0f)
												.Value_Lambda([Settings]() -> float
													{
														UDobotLiveLinkCameraComponent* Comp = Settings->GetSelectedDobotComponent();
														return Comp ? Comp->TrackingDelayMs : 0.0f;
													})
												.OnValueChanged_Lambda([Settings](float NewValue)
													{
														UDobotLiveLinkCameraComponent* Comp = Settings->GetSelectedDobotComponent();
														if (Comp)
														{
															Comp->TrackingDelayMs = NewValue;
														}
													})
										]
								]

							// Connection status
							+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(0, 5, 0, 5)
								[
									SNew(SHorizontalBox)
										+ SHorizontalBox::Slot()
										.AutoWidth()
										.VAlign(VAlign_Center)
										.Padding(0, 0, 10, 0)
										[
											SNew(STextBlock)
												.Text(LOCTEXT("ConnStatusLabel", "Status:"))
										]
										+ SHorizontalBox::Slot()
										.AutoWidth()
										.VAlign(VAlign_Center)
										.Padding(0, 0, 5, 0)
										[
											SNew(SBox)
												.WidthOverride(12)
												.HeightOverride(12)
												[
													SNew(SImage)
														.ColorAndOpacity_Lambda([Settings]()
															{
																UDobotLiveLinkCameraComponent* Comp = Settings->GetSelectedDobotComponent();
																return (Comp && Comp->IsRobotConnected())
																	? FLinearColor::Green
																	: FLinearColor::Red;
															})
														.Image(FAppStyle::GetBrush("Icons.FilledCircle"))
												]
										]
									+ SHorizontalBox::Slot()
										.AutoWidth()
										.VAlign(VAlign_Center)
										[
											SNew(STextBlock)
												.Text_Lambda([Settings]()
													{
														UDobotLiveLinkCameraComponent* Comp = Settings->GetSelectedDobotComponent();
														return (Comp && Comp->IsRobotConnected())
															? LOCTEXT("Connected", "Connected")
															: LOCTEXT("Disconnected", "Disconnected");
													})
												.ColorAndOpacity_Lambda([Settings]()
													{
														UDobotLiveLinkCameraComponent* Comp = Settings->GetSelectedDobotComponent();
														return (Comp && Comp->IsRobotConnected())
															? FSlateColor(FLinearColor::Green)
															: FSlateColor(FLinearColor::Red);
													})
										]
								]

							// Connect / Disconnect / Remove buttons
							+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(0, 5, 0, 5)
								[
									SNew(SHorizontalBox)
										+ SHorizontalBox::Slot()
										.AutoWidth()
										.VAlign(VAlign_Center)
										.Padding(0, 0, 10, 0)
										[
											SNew(STextBlock)
												.Text(LOCTEXT("TrackingLabel", "Enable Tracking:"))
										]
										+ SHorizontalBox::Slot()
										.AutoWidth()
										.VAlign(VAlign_Center)
										[
											SNew(SCheckBox)
												.IsChecked_Lambda([Settings]()
													{
														UDobotLiveLinkCameraComponent* Comp = Settings->GetSelectedDobotComponent();
														return (Comp && Comp->bEnableTracking) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
													})
												.OnCheckStateChanged_Lambda([Settings](ECheckBoxState NewState)
													{
														UDobotLiveLinkCameraComponent* Comp = Settings->GetSelectedDobotComponent();
														if (Comp)
														{
															Comp->bEnableTracking = (NewState == ECheckBoxState::Checked);
															if (Comp->bEnableTracking)
															{
																Comp->ResetTrackingOrigin();
															}
														}
													})
										]
								]

							// Buttons row
							+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(0, 5, 0, 0)
								[
									SNew(SHorizontalBox)
										+ SHorizontalBox::Slot()
										.AutoWidth()
										.Padding(0, 0, 5, 0)
										[
											SNew(SButton)
												.Text(LOCTEXT("ConnectBtn", "Connect"))
												.OnClicked_Lambda([Settings]()
													{
														UDobotLiveLinkCameraComponent* Comp = Settings->GetSelectedDobotComponent();
														if (Comp) Comp->ConnectToRobot();
														return FReply::Handled();
													})
												.IsEnabled_Lambda([Settings]()
													{
														UDobotLiveLinkCameraComponent* Comp = Settings->GetSelectedDobotComponent();
														return Comp && !Comp->IsRobotConnected();
													})
										]
									+ SHorizontalBox::Slot()
										.AutoWidth()
										.Padding(0, 0, 5, 0)
										[
											SNew(SButton)
												.Text(LOCTEXT("DisconnectBtn", "Disconnect"))
												.OnClicked_Lambda([Settings]()
													{
														UDobotLiveLinkCameraComponent* Comp = Settings->GetSelectedDobotComponent();
														if (Comp) Comp->DisconnectFromRobot();
														return FReply::Handled();
													})
												.IsEnabled_Lambda([Settings]()
													{
														UDobotLiveLinkCameraComponent* Comp = Settings->GetSelectedDobotComponent();
														return Comp && Comp->IsRobotConnected();
													})
										]
									+ SHorizontalBox::Slot()
										.AutoWidth()
										[
											SNew(SButton)
												.Text(LOCTEXT("RemoveConnBtn", "Remove Connection"))
												.OnClicked_Lambda([Settings]()
													{
														UDobotLiveLinkCameraComponent* Comp = Settings->GetSelectedDobotComponent();
														if (Comp)
														{
															Comp->DisconnectFromRobot();
															Comp->bHasRobotConnection = false;
														}
														return FReply::Handled();
													})
										]
								]
						]

					// ========== DECKLINK OUTPUT ==========
					+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0, 15, 0, 5)
						[
							SNew(SSeparator)
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0, 0, 0, 5)
						[
							SNew(STextBlock)
								.Text(LOCTEXT("DeckLinkHeader", "DeckLink Output"))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0, 0, 0, 5)
						[
							SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(0, 0, 10, 0)
								[
									SNew(STextBlock)
										.Text(LOCTEXT("OutputLabel", "Output:"))
								]
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(0, 0, 5, 0)
								[
									SNew(SBox)
										.WidthOverride(12)
										.HeightOverride(12)
										[
											SNew(SImage)
												.ColorAndOpacity_Lambda([Settings]()
													{
														return Settings->IsDeckLinkOutputActive()
															? FLinearColor::Green
															: FLinearColor::Red;
													})
												.Image(FAppStyle::GetBrush("Icons.FilledCircle"))
										]
								]
							+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
										.Text_Lambda([Settings]()
											{
												return Settings->IsDeckLinkOutputActive()
													? LOCTEXT("OutputActive", "Active")
													: LOCTEXT("OutputInactive", "Inactive");
											})
										.ColorAndOpacity_Lambda([Settings]()
											{
												return Settings->IsDeckLinkOutputActive()
													? FSlateColor(FLinearColor::Green)
													: FSlateColor(FLinearColor::Red);
											})
								]
						]

					+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0, 5, 0, 10)
						[
							SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(0, 0, 5, 0)
								[
									SNew(SButton)
										.Text(LOCTEXT("StartOutput", "Start Output"))
										.OnClicked_Lambda([Settings]()
											{
												Settings->StartDeckLinkOutput();
												return FReply::Handled();
											})
										.IsEnabled_Lambda([Settings]()
											{
												return !Settings->IsDeckLinkOutputActive();
											})
								]
							+ SHorizontalBox::Slot()
								.AutoWidth()
								[
									SNew(SButton)
										.Text(LOCTEXT("StopOutput", "Stop Output"))
										.OnClicked_Lambda([Settings]()
											{
												Settings->StopDeckLinkOutput();
												return FReply::Handled();
											})
										.IsEnabled_Lambda([Settings]()
											{
												return Settings->IsDeckLinkOutputActive();
											})
								]
						]
				]
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FDobotLiveLinkEditorModule, DobotLiveLinkEditor)