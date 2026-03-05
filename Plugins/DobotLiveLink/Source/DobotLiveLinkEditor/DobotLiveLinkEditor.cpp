#include "DobotLiveLinkEditor.h"
#include "DobotLiveLinkSettings.h"
#include "CineCameraActor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Images/SImage.h"
#include "Framework/Docking/TabManager.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "TimerManager.h"
#include "Engine/World.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

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

	// Auto-connect after editor is fully loaded
#if WITH_EDITOR
	if (GEditor)
	{
		GEditor->GetTimerManager()->SetTimerForNextTick([this]()
			{
				UDobotLiveLinkSettings::Get()->TryAutoConnect();
			});
	}
	else
	{
		// Editor not ready yet, defer
		FCoreDelegates::OnPostEngineInit.AddLambda([this]()
			{
				// Give the editor a moment to fully initialize
				if (GEditor)
				{
					GEditor->GetTimerManager()->SetTimer(AutoConnectTimerHandle, [this]()
						{
							UDobotLiveLinkSettings::Get()->TryAutoConnect();
						}, 2.0f, false);
				}
			});
	}
#endif
}

void FDobotLiveLinkEditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabName);
}

TSharedRef<SDockTab> FDobotLiveLinkEditorModule::OnSpawnTab(const FSpawnTabArgs& SpawnTabArgs)
{
	UDobotLiveLinkSettings* Settings = UDobotLiveLinkSettings::Get();

	// Find cameras and populate list
	CameraOptions.Empty();
	TArray<ACineCameraActor*> Cameras = Settings->FindAllDobotCameras();
	for (ACineCameraActor* Cam : Cameras)
	{
		CameraOptions.Add(MakeShared<FString>(Cam->GetActorLabel()));
	}

	// Auto-select first camera
	if (Cameras.Num() > 0 && !Settings->GetSelectedCamera())
	{
		Settings->SetSelectedCamera(Cameras[0]);
	}

	Settings->LoadFromCamera();

	// Details View
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = true;
	DetailsViewArgs.bShowOptions = false;
	DetailsViewArgs.bShowPropertyMatrixButton = false;
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.bLockable = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetObject(Settings);

	// Store for refresh
	CachedDetailsView = DetailsView;

	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SScrollBox)
				+ SScrollBox::Slot()
				.Padding(10.0f)
				[
					SNew(SVerticalBox)

						// Header
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

						// Camera selector row
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
										.OnSelectionChanged_Lambda([this, Settings, Cameras](TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
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
												.Text_Lambda([this, Settings]()
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
								[
									SNew(SButton)
										.Text(LOCTEXT("RefreshCameras", "Refresh"))
										.OnClicked_Lambda([this, Settings]()
											{
												CameraOptions.Empty();
												TArray<ACineCameraActor*> Cameras = Settings->FindAllDobotCameras();
												for (ACineCameraActor* Cam : Cameras)
												{
													CameraOptions.Add(MakeShared<FString>(Cam->GetActorLabel()));
												}
												if (CameraComboBox.IsValid())
												{
													CameraComboBox->RefreshOptions();
												}
												return FReply::Handled();
											})
								]
						]

					+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0, 0, 0, 5)
						[
							SNew(SSeparator)
						]

						// Details View with all settings
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							DetailsView
						]

						// Connection section
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

						// Connection status
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
										.Text(LOCTEXT("StatusLabel", "Status:"))
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
														return Settings->IsConnected()
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
												return Settings->IsConnected()
													? LOCTEXT("StatusConnected", "Connected")
													: LOCTEXT("StatusDisconnected", "Disconnected");
											})
										.ColorAndOpacity_Lambda([Settings]()
											{
												return Settings->IsConnected()
													? FSlateColor(FLinearColor::Green)
													: FSlateColor(FLinearColor::Red);
											})
								]
						]

					// Connect / Disconnect buttons
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
										.Text(LOCTEXT("ConnectButton", "Connect"))
										.OnClicked_Lambda([Settings]()
											{
												Settings->ConnectToRobot();
												return FReply::Handled();
											})
										.IsEnabled_Lambda([Settings]()
											{
												return !Settings->IsConnected();
											})
								]
							+ SHorizontalBox::Slot()
								.AutoWidth()
								[
									SNew(SButton)
										.Text(LOCTEXT("DisconnectButton", "Disconnect"))
										.OnClicked_Lambda([Settings]()
											{
												Settings->DisconnectFromRobot();
												return FReply::Handled();
											})
										.IsEnabled_Lambda([Settings]()
											{
												return Settings->IsConnected();
											})
								]
						]

					// DeckLink Output section
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

						// DeckLink status
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
										.Text(LOCTEXT("OutputStatusLabel", "Output:"))
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

					// DeckLink Start / Stop buttons
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
										.Text(LOCTEXT("StartOutputButton", "Start Output"))
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
										.Text(LOCTEXT("StopOutputButton", "Stop Output"))
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