#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameFramework/Character.h"
#include "PatrolGuard.generated.h"

class UBehaviorTree;
class UBlendSpace;
class UTextRenderComponent;

UCLASS()
class HW2_API APatrolGuardController : public AAIController
{
    GENERATED_BODY()

protected:
    virtual void OnPossess(APawn* InPawn) override;
};

UCLASS()
class HW2_API APatrolGuard : public ACharacter
{
    GENERATED_BODY()

public:
    APatrolGuard();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(EditAnywhere, Category="Patrol")
    TObjectPtr<UBehaviorTree> Behavior;

    void ShowState(const TCHAR* Label, FColor Color, float Speed);

private:
    UPROPERTY(EditDefaultsOnly, Category="Patrol|Animation")
    TObjectPtr<UBlendSpace> LocomotionBlendSpace;

    UPROPERTY(VisibleAnywhere, Category="Patrol")
    TObjectPtr<UTextRenderComponent> StateLabel;
};
