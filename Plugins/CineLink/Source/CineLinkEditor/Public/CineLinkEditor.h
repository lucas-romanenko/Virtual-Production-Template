#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Input/SComboBox.h"

class UCineLinkSettings;
class ACineCameraActor;
class SVerticalBox;

class FCineLinkEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedRef<class SDockTab> OnSpawnTab(const class FSpawnTabArgs& SpawnTabArgs);
	void RefreshCameraList();
	TSharedRef<SVerticalBox> BuildDeckLinkPortRow(int32 PortIndex, UCineLinkSettings* Settings);
	FSlateColor GetConnectionStateColor() const;
	FText GetConnectionStateText() const;
	FReply OnConnectClicked();
	FReply OnDisconnectClicked();
	FReply OnAddCameraClicked();

	static const FName TabName;

	// Parallel arrays — index N = same camera
	TArray<ACineCameraActor*> CameraActors;
	TArray<TSharedPtr<FString>> CameraOptions;

	TSharedPtr<SComboBox<TSharedPtr<FString>>> CameraComboBox;
	TWeakPtr<SVerticalBox> DeckLinkPortsBox;
};