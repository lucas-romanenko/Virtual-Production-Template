#pragma once

#include "CoreMinimal.h"
#include "LiveLinkSourceFactory.h"
#include "DobotLiveLinkSourceFactory.generated.h"

UCLASS()
class UDobotLiveLinkSourceFactory : public ULiveLinkSourceFactory
{
	GENERATED_BODY()

public:
	virtual FText GetSourceDisplayName() const override;
	virtual FText GetSourceTooltip() const override;
	virtual EMenuType GetMenuType() const override { return EMenuType::SubPanel; }
	virtual TSharedPtr<SWidget> BuildCreationPanel(FOnLiveLinkSourceCreated OnLiveLinkSourceCreated) const override;
	virtual TSharedPtr<ILiveLinkSource> CreateSource(const FString& ConnectionString) const override;

private:
	void OnConnectionSettingsAccepted(TSharedPtr<ILiveLinkSource> Source, FString ConnectionString, FOnLiveLinkSourceCreated OnLiveLinkSourceCreated) const;
};