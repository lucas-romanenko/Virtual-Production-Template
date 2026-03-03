#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CineCameraComponent.h"
#include "DobotLiveLinkCameraComponent.generated.h"
UCLASS(ClassGroup = (LiveLink), meta = (BlueprintSpawnableComponent))
class DOBOTLIVELINK_API UDobotLiveLinkCameraComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UDobotLiveLinkCameraComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void BeginPlay() override;
    // LiveLink Settings
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LiveLink")
    FName LiveLinkSubjectName = TEXT("DobotCamera");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LiveLink")
    UCineCameraComponent* CameraToControl;
    // Tracking Control
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracking")
    bool bEnableTracking = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracking")
    bool bShowDebugInfo = true;
    // Start tracking from current position
    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Tracking")
    void StartTracking();
    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Tracking")
    void StopTracking();
    // Find camera in various configurations
    void FindCamera();
protected:
    virtual bool ShouldCreateRenderState() const override { return true; }
private:
    bool bHasRecordedStart = false;
    FTransform CameraStartTransform;
    FTransform RobotStartTransform;
    FTransform LastRobotTransform;
};