#include "DobotLiveLinkEditor.h"
#include "DobotLiveLinkSettings.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
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

TSharedRef<SDockTab> FDobotLiveLinkEditorModule::OnSpawnTab(const FSpawnTabArgs& SpawnTabArgs)
{
	UDobotLiveLinkSettings* Settings = UDobotLiveLinkSettings::Get();
	Settings->LoadFromCamera();

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

	// Status text block - shared ptr so we can update it
	TSharedPtr<STextBlock> StatusText;

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
						.Padding(0, 0, 0, 10)
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

						// Details View with all settings
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							DetailsView
						]

						// Connection section separator
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0, 10, 0, 5)
						[
							SNew(SSeparator)
						]

						// Connection header
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0, 0, 0, 5)
						[
							SNew(STextBlock)
								.Text(LOCTEXT("ConnectionHeader", "Robot Connection"))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
						]

						// Status row
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
								[
									SAssignNew(StatusText, STextBlock)
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
				]
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FDobotLiveLinkEditorModule, DobotLiveLinkEditor)