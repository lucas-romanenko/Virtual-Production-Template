#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FDobotLiveLinkEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedRef<class SDockTab> OnSpawnTab(const class FSpawnTabArgs& SpawnTabArgs);
	static const FName TabName;
};