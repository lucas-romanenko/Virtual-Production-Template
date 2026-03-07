#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Input/SComboBox.h"

class UDobotLiveLinkSettings;
class SVerticalBox;

class FDobotLiveLinkEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedRef<class SDockTab> OnSpawnTab(const class FSpawnTabArgs& SpawnTabArgs);
	void RefreshCameraList();
	TSharedRef<SVerticalBox> BuildDeckLinkPortRow(int32 PortIndex, UDobotLiveLinkSettings* Settings);

	static const FName TabName;

	// Camera selector
	TArray<TSharedPtr<FString>> CameraOptions;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> CameraComboBox;
	TWeakPtr<class IDetailsView> CachedDetailsView;

	// DeckLink output
	TArray<TSharedPtr<FString>> DeckLinkCameraOptions;
	TWeakPtr<SVerticalBox> DeckLinkPortsBox;
};