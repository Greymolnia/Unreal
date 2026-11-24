#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/BlendSpace.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HomeworkCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Patrol/PatrolGuard.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPatrolDepotTest, "HW2.PatrolDepot.Play",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

namespace
{
    bool HasDetourAroundProp(UWorld* World, const UInstancedStaticMeshComponent* Mesh)
    {
        const UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(World);
        if (!Nav)
        {
            return false;
        }

        for (int32 Index = 0; Index < Mesh->GetInstanceCount(); ++Index)
        {
            FTransform Prop;
            Mesh->GetInstanceTransform(Index, Prop, true);
            const FVector Center(Prop.GetLocation().X, Prop.GetLocation().Y, 95);
            for (const FVector Axis : { FVector::ForwardVector, FVector::RightVector })
            {
                const FVector Start = Center - Axis * 220;
                const FVector End = Center + Axis * 220;
                FHitResult Hit;
                if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility)
                    || Hit.GetComponent() != Mesh || Hit.Item != Index)
                {
                    continue;
                }
                FNavLocation StartOnNav;
                FNavLocation EndOnNav;
                const FVector Extent(24, 24, 160);
                if (!Nav->ProjectPointToNavigation(Start, StartOnNav, Extent)
                    || !Nav->ProjectPointToNavigation(End, EndOnNav, Extent))
                {
                    continue;
                }

                const UNavigationPath* Route = UNavigationSystemV1::FindPathToLocationSynchronously(
                    World, StartOnNav.Location, EndOnNav.Location);
                if (Route && Route->IsValid() && !Route->IsPartial() && Route->GetPathLength() > 470)
                {
                    return true;
                }
            }
        }
        return false;
    }

    class FCheckPatrolDepot : public IAutomationLatentCommand
    {
    public:
        explicit FCheckPatrolDepot(FAutomationTestBase& InTest) : Test(InTest) {}

        bool Update() override
        {
            UWorld* World = nullptr;
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
            {
                if (Context.World() && Context.World()->IsGameWorld())
                {
                    World = Context.World();
                    break;
                }
            }
            if (!World || !World->GetMapName().EndsWith(TEXT("Lvl_PatrolDepot")))
            {
                Test.AddError(TEXT("Run the test on Lvl_PatrolDepot."));
                return true;
            }

            APatrolGuard* Guard = nullptr;
            for (TActorIterator<APatrolGuard> It(World); It; ++It)
            {
                Guard = *It;
                break;
            }
            AHomeworkCharacter* Player = Cast<AHomeworkCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
            APatrolGuardController* Controller = Guard ? Cast<APatrolGuardController>(Guard->GetController()) : nullptr;
            UBlackboardComponent* Board = Controller ? Controller->GetBlackboardComponent() : nullptr;
            if (!Guard || !Player || !Board)
            {
                Test.AddError(TEXT("Expected the HW2 player, possessed guard and running Blackboard."));
                return true;
            }
            if (FPlatformTime::Seconds() - Started > 24)
            {
                const UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(World);
                FNavLocation Projected;
                const bool bFoundNav = Nav && Nav->ProjectPointToNavigation(
                    Guard->GetActorLocation(), Projected, FVector(420, 420, 400));
                Test.AddInfo(FString::Printf(TEXT("Navigation world %d, guard projection %d."),
                    Nav != nullptr, bFoundNav));
                UAnimSingleNodeInstance* State = Guard->GetMesh()->GetSingleNodeInstance();
                FVector BlendPosition = FVector::ZeroVector;
                FVector Filtered = FVector::ZeroVector;
                if (State)
                {
                    State->GetBlendSpaceState(BlendPosition, Filtered);
                }
                Test.AddError(FString::Printf(
                    TEXT("Patrol check timed out in stage %d. Guard at %s, speed %.1f, blend %s, time %.3f, walk pose changed %d."),
                    Stage, *Guard->GetActorLocation().ToCompactString(), Guard->GetVelocity().Size2D(),
                    *BlendPosition.ToCompactString(), State ? State->GetCurrentTime() : 0.f, bWalkPoseChanged));
                return true;
            }

            UAnimSingleNodeInstance* Playback = Guard->GetMesh()->GetSingleNodeInstance();
            UBlendSpace* Blend = Playback ? Cast<UBlendSpace>(Playback->GetCurrentAsset()) : nullptr;
            if (!Blend || !Playback->IsPlaying())
            {
                Test.AddError(TEXT("Guard must play its locomotion blend space."));
                return true;
            }

            FVector BlendInput;
            FVector FilteredInput;
            Playback->GetBlendSpaceState(BlendInput, FilteredInput);
            const int32 SpeedAxis = Blend->GetBlendParameter(0).DisplayName == TEXT("Speed") ? 0 : 1;
            const FVector Foot = Guard->GetMesh()->GetSocketTransform(TEXT("foot_l"), RTS_Component).GetLocation();

            const FVector Home = Board->GetValueAsVector(TEXT("Home"));
            const bool bSearching = Board->GetValueAsBool(TEXT("SearchActive"));
            const bool bChasing = Board->GetValueAsObject(TEXT("TargetActor")) == Player;
            if (Stage == 0)
            {
                Player->SetActorLocation(FVector(2750, -1700, 100));
                if (BlendInput[SpeedAxis] > 150)
                {
                    if (!WalkFoot.IsSet())
                    {
                        WalkFoot = Foot;
                    }
                    bWalkPoseChanged = bWalkPoseChanged || !Foot.Equals(WalkFoot.GetValue(), 1.0);
                }
                if (!bWalkPoseChanged || FVector::Dist2D(Home, Guard->GetActorLocation()) < 150)
                {
                    return false;
                }
                int32 Generated = 0;
                bool bHasDetour = false;
                TSet<FName> VisiblePropTypes;
                for (TActorIterator<AActor> It(World); It; ++It)
                {
                    TInlineComponentArray<UInstancedStaticMeshComponent*> Meshes(*It);
                    for (const UInstancedStaticMeshComponent* Mesh : Meshes)
                    {
                        if (Mesh->ComponentHasTag(TEXT("PCG Generated Component")))
                        {
                            Generated += Mesh->GetInstanceCount();
                            bHasDetour = bHasDetour || HasDetourAroundProp(World, Mesh);
                            const UStaticMesh* Prop = Mesh->GetStaticMesh();
                            if (!Prop)
                            {
                                Test.AddError(TEXT("A PCG component has no static mesh."));
                                continue;
                            }

                            VisiblePropTypes.Add(Prop->GetFName());
                            for (int32 Index = 0; Index < Mesh->GetInstanceCount(); ++Index)
                            {
                                FTransform Transform;
                                Mesh->GetInstanceTransform(Index, Transform, true);
                                const FBox WorldBox = Prop->GetBoundingBox().TransformBy(Transform);
                                if (!FMath::IsNearlyZero(WorldBox.Min.Z, 0.5))
                                {
                                    Test.AddError(FString::Printf(TEXT("%s instance %d has base at Z %.2f."),
                                        *Prop->GetName(), Index, WorldBox.Min.Z));
                                }
                            }
                        }
                    }
                }
                Test.TestEqual(TEXT("PCG generated instance count"), Generated, 72);
                Test.TestTrue(TEXT("PCG uses the custom cargo crate and steel drum"),
                    VisiblePropTypes.Contains(TEXT("SM_DepotCargoCrate")) && VisiblePropTypes.Contains(TEXT("SM_DepotSteelDrum")));
                Test.TestTrue(TEXT("NavMesh detours around collidable PCG geometry"), bHasDetour);
                Test.TestFalse(TEXT("Guard patrols without a target"), bChasing || bSearching);
                Guard->SetActorLocation(Home);
                Player->SetActorLocation(FVector(-2700, -250, 100));
                Player->StartSprint();
                Stage = 1;
            }
            else if (Stage == 1)
            {
                if (!bSearching)
                {
                    Player->AddMovementInput(FVector(0, 1, 0), 1.0f, true);
                    return false;
                }

                Test.TestFalse(TEXT("Noise creates suspicion without a live target"), bChasing);
                const float Radius = Board->GetValueAsFloat(TEXT("SearchRadius"));
                Test.TestTrue(TEXT("Noise yields a bounded search radius"), Radius >= 300 && Radius <= 650);

                Player->StopSprint();
                Player->GetCharacterMovement()->StopMovementImmediately();
                // Use the open space left of the scatter to guarantee direct line of sight.
                Controller->StopMovement();
                Guard->GetCharacterMovement()->StopMovementImmediately();
                Guard->SetActorLocation(FVector(-2740, -390, 100));
                Guard->SetActorRotation(FRotator(0, 90, 0));
                Player->SetActorLocation(FVector(-2740, 260, 100));
                Stage = 2;
            }
            else if (Stage == 2)
            {
                if (!bChasing)
                {
                    Guard->SetActorRotation(FRotator(0, 90, 0));
                    return false;
                }
                if (BlendInput[SpeedAxis] <= 400)
                {
                    return false;
                }
                if (!RunFoot.IsSet())
                {
                    RunFoot = Foot;
                }
                const bool bRunPoseChanged = !Foot.Equals(RunFoot.GetValue(), 1.0);
                if (!bRunPoseChanged || Guard->GetActorLocation().Y <= -240)
                {
                    return false;
                }
                Test.TestFalse(TEXT("Sight replaces suspicion with a chase"), bSearching);
                Test.TestTrue(TEXT("The guard is slower than the player sprint"),
                    Guard->GetCharacterMovement()->MaxWalkSpeed < Player->SprintSpeed);
                // Both actors are outside the old home radius, with the same clear sight.
                Controller->StopMovement();
                Guard->GetCharacterMovement()->StopMovementImmediately();
                Guard->SetActorLocation(FVector(2600, 1080, 100));
                Guard->SetActorRotation(FRotator(0, 90, 0));
                Player->SetActorLocation(FVector(2600, 1570, 100));
                StationaryStarted = World->GetTimeSeconds();
                Stage = 3;
            }
            else if (Stage == 3)
            {
                if (!bChasing || bSearching)
                {
                    Test.AddError(TEXT("A stationary visible player lost its chase outside the old home radius."));
                    return true;
                }
                if (World->GetTimeSeconds() - StationaryStarted < 1.1)
                {
                    return false;
                }

                Controller->StopMovement();
                Guard->GetCharacterMovement()->StopMovementImmediately();
                Guard->SetActorLocation(FVector(-2740, -240, 100));
                Guard->SetActorRotation(FRotator(0, 90, 0));
                Player->SetActorLocation(FVector(-2740, 260, 100));
                Player->StartSprint();
                EscapeStarted = World->GetTimeSeconds();
                Stage = 4;
            }
            else if (Stage == 4)
            {
                if (bChasing && World->GetTimeSeconds() - EscapeStarted < 3.0)
                {
                    Player->AddMovementInput(FVector::RightVector, 1.0f, true);
                    return false;
                }
                Test.TestFalse(TEXT("The player can break chase using normal sprint movement"), bChasing);
                Test.TestTrue(TEXT("Escaping sight/retention range changes chase to a short search"), bSearching);
                Player->StopSprint();
                Player->GetCharacterMovement()->StopMovementImmediately();
                // Re-enter clear sight briefly. A single glimpse cannot immediately renew the chase.
                Guard->SetActorRotation(FRotator(0, 90, 0));
                Player->SetActorLocation(Guard->GetActorLocation() + FVector(0, 350, 0));
                ReacquireStarted = World->GetTimeSeconds();
                Stage = 5;
            }
            else if (Stage == 5)
            {
                if (bChasing)
                {
                    Test.AddError(TEXT("The guard reacquired the player before the grace period expired."));
                    return true;
                }
                if (World->GetTimeSeconds() - ReacquireStarted < 0.6)
                {
                    Guard->SetActorRotation(FRotator(0, 90, 0));
                    return false;
                }
                Player->SetActorLocation(FVector(2750, -1700, 100));
                Stage = 6;
            }
            else if (Stage == 6 && !bChasing && !bSearching)
            {
                Test.AddInfo(TEXT("Grounded PCG props and animated patrol and chase verified. Guard retains a stationary visible target, investigates sprint noise, allows a sprint escape, and returns to patrol."));
                return true;
            }
            return false;
        }

    private:
        FAutomationTestBase& Test;
        double Started = FPlatformTime::Seconds();
        int32 Stage = 0;
        double StationaryStarted = 0;
        double EscapeStarted = 0;
        double ReacquireStarted = 0;
        TOptional<FVector> WalkFoot;
        TOptional<FVector> RunFoot;
        bool bWalkPoseChanged = false;
    };
}

bool FPatrolDepotTest::RunTest(const FString& Parameters)
{
    ADD_LATENT_AUTOMATION_COMMAND(FCheckPatrolDepot(*this));
    return true;
}

#endif
