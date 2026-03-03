#include "DobotLiveLinkEditor.h"

#define LOCTEXT_NAMESPACE "FDobotLiveLinkEditorModule"

void FDobotLiveLinkEditorModule::StartupModule()
{
	// Nothing needed - factory is registered via .uplugin
}

void FDobotLiveLinkEditorModule::ShutdownModule()
{
	// Cleanup
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FDobotLiveLinkEditorModule, DobotLiveLinkEditor)