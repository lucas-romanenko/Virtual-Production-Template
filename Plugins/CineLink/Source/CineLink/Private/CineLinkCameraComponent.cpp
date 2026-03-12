#include "CineLinkCameraComponent.h"
#include "ILiveLinkClient.h"
#include "LiveLinkSourceFactory.h"
#include "Modules/ModuleManager.h"

UCineLinkCameraComponent::UCineLinkCameraComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

ECineLinkConnectionState UCineLinkCameraComponent::GetConnectionState() const
{
	if (!bIsConnected)     return ECineLinkConnectionState::NoConnection;
	if (IsSubjectActive()) return ECineLinkConnectionState::Connected;
	return ECineLinkConnectionState::ConnectionLost;
}

bool UCineLinkCameraComponent::IsSubjectActive() const
{
	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	if (!ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
		return false;

	ILiveLinkClient& Client = ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);

	// Check our source GUID is still registered
	TArray<FGuid> Sources = Client.GetSources();
	if (!Sources.Contains(ActiveSourceGuid))
		return false;

	// Check subject is actively sending data
	TArray<FLiveLinkSubjectKey> Subjects = Client.GetSubjects(false, true);
	for (const FLiveLinkSubjectKey& Key : Subjects)
	{
		if (Key.SubjectName == LiveLinkSubjectName)
			return true;
	}
	return false;
}

bool UCineLinkCameraComponent::HasExistingFreeDSource() const
{
	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	if (!ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
		return false;

	ILiveLinkClient& Client = ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);

	// Check if subject is already active (e.g. from a saved Live Link preset)
	TArray<FLiveLinkSubjectKey> Subjects = Client.GetSubjects(false, true);
	for (const FLiveLinkSubjectKey& Key : Subjects)
	{
		if (Key.SubjectName == LiveLinkSubjectName)
			return true;
	}
	return false;
}

bool UCineLinkCameraComponent::Connect()
{
	// If subject is already active (e.g. from a preset), just mark connected
	if (HasExistingFreeDSource())
	{
		bIsConnected = true;
		UE_LOG(LogTemp, Warning, TEXT("CineLink: Subject '%s' already active, reusing existing source"),
			*LiveLinkSubjectName.ToString());
		return true;
	}

	return CreateFreeDSource();
}

void UCineLinkCameraComponent::Disconnect()
{
	CleanupSource();
	bIsConnected = false;
}

bool UCineLinkCameraComponent::CreateFreeDSource()
{
	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	if (!ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
	{
		UE_LOG(LogTemp, Error, TEXT("CineLink: LiveLink client not available"));
		return false;
	}

	ILiveLinkClient& Client = ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);

	// Find the LiveLinkFreeD source factory by scanning registered UClass subclasses
	ULiveLinkSourceFactory* FreeDFactory = nullptr;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (It->IsChildOf(ULiveLinkSourceFactory::StaticClass()) && !It->HasAnyClassFlags(CLASS_Abstract))
		{
			if (It->GetName().Contains(TEXT("FreeD")))
			{
				FreeDFactory = It->GetDefaultObject<ULiveLinkSourceFactory>();
				UE_LOG(LogTemp, Warning, TEXT("CineLink: Found FreeD factory: %s"), *It->GetName());
				break;
			}
		}
	}

	if (!FreeDFactory)
	{
		UE_LOG(LogTemp, Error, TEXT("CineLink: LiveLinkFreeD factory not found. Enable the LiveLinkFreeD plugin in Edit > Plugins > Virtual Production."));
		return false;
	}

	// FreeD listens on a local port. IP 0.0.0.0 = accept from any sender.
	FString ConnectionString = FString::Printf(TEXT("0.0.0.0:%d"), LensmasterPort);

	TSharedPtr<ILiveLinkSource> Source = FreeDFactory->CreateSource(ConnectionString);
	if (!Source.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("CineLink: Failed to create FreeD source on port %d"), LensmasterPort);
		return false;
	}

	Client.AddSource(Source);

	// Grab the GUID of the most recently added source
	TArray<FGuid> Sources = Client.GetSources();
	if (Sources.Num() > 0)
	{
		ActiveSourceGuid = Sources.Last();
	}

	bIsConnected = true;
	UE_LOG(LogTemp, Warning, TEXT("CineLink: FreeD source created on port %d, expecting subject '%s'"),
		LensmasterPort, *LiveLinkSubjectName.ToString());

	return true;
}

void UCineLinkCameraComponent::CleanupSource()
{
	if (!ActiveSourceGuid.IsValid()) return;

	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	if (!ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName)) return;

	ILiveLinkClient& Client = ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
	Client.RemoveSource(ActiveSourceGuid);
	ActiveSourceGuid.Invalidate();
}