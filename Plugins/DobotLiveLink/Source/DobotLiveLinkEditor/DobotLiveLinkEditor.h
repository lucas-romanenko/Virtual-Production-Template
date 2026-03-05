#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Input/SComboBox.h"

class FDobotLiveLinkEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedRef<class SDockTab> OnSpawnTab(const class FSpawnTabArgs& SpawnTabArgs);
	static const FName TabName;

	// Camera selector
	TArray<TSharedPtr<FString>> CameraOptions;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> CameraComboBox;
	TWeakPtr<class IDetailsView> CachedDetailsView;

	// Auto-connect timer
	FTimerHandle AutoConnectTimerHandle;
};