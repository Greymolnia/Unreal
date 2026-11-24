#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BTTaskNode.h"
#include "PatrolNodes.generated.h"

/** Maintains sight and hearing evidence for the selector. Each guard owns its memory. */
UCLASS()
class HW2_API UBTService_PatrolAwareness : public UBTService
{
    GENERATED_BODY()

public:
    UBTService_PatrolAwareness();

protected:
    virtual void TickNode(UBehaviorTreeComponent& Owner, uint8* Memory, float DeltaSeconds) override;

private:
    void UpdateAwareness(UBehaviorTreeComponent& Owner);

    double SearchUntil = 0;
    double NextNoiseSample = 0;
    double VisibleSince = -1;
    double LastVisibleAt = -1;
    double ReacquireAfter = 0;
    enum class EMode : uint8 { Patrol, Search, Chase };
    EMode Mode = EMode::Patrol;
};

/** Chooses a reachable destination around Home or the most recent evidence. */
UCLASS()
class HW2_API UBTTask_PatrolDestination : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTTask_PatrolDestination();
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& Owner, uint8* Memory) override;

    UPROPERTY(EditAnywhere, Category="Destination")
    bool bSearch = false;
};
