#include "Patrol/PatrolNodes.h"

#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "HomeworkCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Patrol/PatrolGuard.h"

namespace
{
    const FName TargetKey(TEXT("TargetActor"));
    const FName SearchingKey(TEXT("SearchActive"));
    const FName EvidenceKey(TEXT("EvidenceLocation"));
    const FName RadiusKey(TEXT("SearchRadius"));
    const FName HomeKey(TEXT("Home"));
    const FName DestinationKey(TEXT("Destination"));
    constexpr float PatrolRadius = 1450;
    constexpr float SightRadius = 850;
    constexpr float LoseSightRadius = 1000;
    constexpr float HearingRadius = 1850;
    constexpr float ChaseSpeed = 450;
    constexpr double SightConfirmSeconds = 0.25;
    constexpr double LostSightGrace = 0.65;
    constexpr double ReacquireDelay = 1.4;
    constexpr double LastSeenSearchSeconds = 3.2;
    constexpr double NoiseSearchSeconds = 4.0;
}

UBTService_PatrolAwareness::UBTService_PatrolAwareness()
{
    NodeName = TEXT("Sight, sprint noise and fading evidence");
    bCreateNodeInstance = true;
    Interval = 0.15f;
    RandomDeviation = 0;
    bCallTickOnSearchStart = true;
}

void UBTService_PatrolAwareness::TickNode(
    UBehaviorTreeComponent& Owner, uint8* Memory, float DeltaSeconds)
{
    Super::TickNode(Owner, Memory, DeltaSeconds);
    UpdateAwareness(Owner);
}

void UBTService_PatrolAwareness::UpdateAwareness(UBehaviorTreeComponent& Owner)
{
    APatrolGuardController* Controller = Cast<APatrolGuardController>(Owner.GetAIOwner());
    APatrolGuard* Guard = Controller ? Cast<APatrolGuard>(Controller->GetPawn()) : nullptr;
    UBlackboardComponent* Board = Owner.GetBlackboardComponent();
    if (!Guard || !Board)
    {
        return;
    }

    AHomeworkCharacter* Player = Cast<AHomeworkCharacter>(UGameplayStatics::GetPlayerPawn(Guard, 0));
    const double Now = Guard->GetWorld()->GetTimeSeconds();
    const float Distance = Player ? FVector::Dist(Guard->GetActorLocation(), Player->GetActorLocation()) : BIG_NUMBER;
    const bool bHadTarget = Mode == EMode::Chase;
    const FVector ToPlayer = Player ? (Player->GetActorLocation() - Guard->GetActorLocation()).GetSafeNormal2D() : FVector::ZeroVector;
    const bool bInView = Distance < 170 || FVector::DotProduct(Guard->GetActorForwardVector(), ToPlayer) > 0.42f;
    const bool bVisible = Player && (bHadTarget || bInView)
        && Distance <= (bHadTarget ? LoseSightRadius : SightRadius)
        && Controller->LineOfSightTo(Player);

    if (!bVisible || Now < ReacquireAfter)
    {
        VisibleSince = -1;
    }
    else if (VisibleSince < 0)
    {
        VisibleSince = Now;
    }

    EMode NextMode = EMode::Patrol;
    if (bVisible && (bHadTarget || (VisibleSince >= 0 && Now - VisibleSince >= SightConfirmSeconds)))
    {
        LastVisibleAt = Now;
        Board->SetValueAsVector(EvidenceKey, Player->GetActorLocation());
        Board->SetValueAsFloat(RadiusKey, 420);
        NextMode = EMode::Chase;
    }
    else if (Player && bHadTarget && Now - LastVisibleAt < LostSightGrace)
    {
        // One obstructed sample or a moment at the distance boundary should not abort the chase.
        NextMode = EMode::Chase;
    }
    else if (bHadTarget)
    {
        // Keep the last visible position fixed while the guard investigates it.
        SearchUntil = Now + LastSeenSearchSeconds;
        NextNoiseSample = SearchUntil + 0.8;
        ReacquireAfter = Now + ReacquireDelay;
        NextMode = EMode::Search;
    }
    else
    {
        const bool bExpiredSearch = Mode == EMode::Search && Now >= SearchUntil;
        const bool bMovingSprint = Player && Player->bIsSprinting && Player->GetVelocity().SizeSquared2D() > FMath::Square(560.0);
        if (bExpiredSearch)
        {
            NextNoiseSample = FMath::Max(NextNoiseSample, Now + 0.8);
        }
        else if (bMovingSprint && Distance <= HearingRadius && Now >= NextNoiseSample)
        {
            // A new sound can move the search area, but cannot prolong the current search.
            const float Uncertainty = FMath::GetMappedRangeValueClamped(
                FVector2D(350, HearingRadius), FVector2D(300, 650), Distance);
            const FVector Error = FMath::VRand().GetSafeNormal2D() * Uncertainty * 0.25f;
            Board->SetValueAsVector(EvidenceKey, Player->GetActorLocation() + Error);
            Board->SetValueAsFloat(RadiusKey, Uncertainty);
            if (Mode != EMode::Search)
            {
                SearchUntil = Now + NoiseSearchSeconds;
            }
            NextNoiseSample = Now + 0.75;
        }
        if (Now < SearchUntil)
        {
            NextMode = EMode::Search;
        }
    }

    if (NextMode == EMode::Search)
    {
        Board->SetValueAsBool(SearchingKey, true);
        Board->ClearValue(TargetKey);
    }
    else if (NextMode == EMode::Chase)
    {
        Board->SetValueAsObject(TargetKey, Player);
        Board->SetValueAsBool(SearchingKey, false);
    }
    else
    {
        Board->SetValueAsBool(SearchingKey, false);
        Board->ClearValue(TargetKey);
        Board->ClearValue(EvidenceKey);
    }

    if (Mode != NextMode)
    {
        Mode = NextMode;
        switch (Mode)
        {
        case EMode::Chase:
            Guard->ShowState(TEXT("CHASE"), FColor(255, 75, 60), ChaseSpeed);
            if (Player)
            {
                Player->ShowPatrolNotice(NSLOCTEXT("DepotHUD", "Spotted", "Guard spotted you"), FLinearColor(1, 0.20f, 0.13f));
            }
            break;
        case EMode::Search:
            Guard->ShowState(TEXT("SEARCH"), FColor(255, 185, 50), 360);
            if (Player)
            {
                Player->ShowPatrolNotice(
                    bHadTarget ? NSLOCTEXT("DepotHUD", "LostSight", "Guard lost sight of you")
                               : NSLOCTEXT("DepotHUD", "Heard", "Guard heard your sprint"),
                    FLinearColor(1, 0.57f, 0.10f));
            }
            break;
        case EMode::Patrol:
            Guard->ShowState(TEXT("PATROL"), FColor::Cyan, 245);
            if (Player)
            {
                Player->ShowPatrolNotice(NSLOCTEXT("DepotHUD", "Patrol", "Guard returned to patrol"), FLinearColor(0.16f, 0.86f, 0.70f));
            }
            break;
        }
    }
}

UBTTask_PatrolDestination::UBTTask_PatrolDestination()
{
    NodeName = TEXT("Choose reachable destination");
}

EBTNodeResult::Type UBTTask_PatrolDestination::ExecuteTask(UBehaviorTreeComponent& Owner, uint8* Memory)
{
    const AAIController* Controller = Owner.GetAIOwner();
    const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    UBlackboardComponent* Board = Owner.GetBlackboardComponent();
    const UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(Owner.GetWorld());
    if (!Pawn || !Board || !Nav)
    {
        return EBTNodeResult::Failed;
    }

    const FVector Center = Board->GetValueAsVector(bSearch ? EvidenceKey : HomeKey);
    const float Radius = bSearch ? Board->GetValueAsFloat(RadiusKey) : PatrolRadius;
    FNavLocation Anchor;
    if (!Nav->ProjectPointToNavigation(Center, Anchor, FVector(420, 420, 400)))
    {
        return EBTNodeResult::Failed;
    }

    for (int32 Attempt = 0; Attempt < 8; ++Attempt)
    {
        FNavLocation Destination;
        if (Nav->GetRandomReachablePointInRadius(Anchor.Location, Radius, Destination)
            && FVector::DistSquared2D(Destination.Location, Center) <= FMath::Square(Radius)
            && FVector::DistSquared2D(Destination.Location, Pawn->GetActorLocation()) >= FMath::Square(180.0))
        {
            Board->SetValueAsVector(DestinationKey, Destination.Location);
            return EBTNodeResult::Succeeded;
        }
    }

    if (FVector::DistSquared2D(Anchor.Location, Pawn->GetActorLocation()) > FMath::Square(100.0))
    {
        Board->SetValueAsVector(DestinationKey, Anchor.Location);
        return EBTNodeResult::Succeeded;
    }
    return EBTNodeResult::Failed;
}
