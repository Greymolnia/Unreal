#include "Patrol/PatrolGuard.h"

#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/BlendSpace.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

void APatrolGuardController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    const APatrolGuard* Guard = Cast<APatrolGuard>(InPawn);
    if (!Guard || !Guard->Behavior)
    {
        return;
    }

    UBlackboardComponent* Board = nullptr;
    if (UseBlackboard(Guard->Behavior->GetBlackboardAsset(), Board))
    {
        Board->SetValueAsVector(TEXT("Home"), Guard->GetActorLocation());
        RunBehaviorTree(Guard->Behavior);
    }
}

APatrolGuard::APatrolGuard()
{
    AIControllerClass = APatrolGuardController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    bUseControllerRotationYaw = false;
    PrimaryActorTick.bCanEverTick = true;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0, 480, 0);
    GetMesh()->SetRelativeLocation(FVector(0, 0, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
    GetMesh()->SetRelativeRotation(FRotator(0, -90, 0));

    static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshAsset(
        TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"));
    static ConstructorHelpers::FObjectFinder<UBlendSpace> Locomotion(
        TEXT("/Game/Characters/Mannequins/Anims/Unarmed/BS_Idle_Walk_Run"));
    if (MeshAsset.Succeeded())
    {
        GetMesh()->SetSkeletalMeshAsset(MeshAsset.Object);
    }
    if (Locomotion.Succeeded())
    {
        LocomotionBlendSpace = Locomotion.Object;
    }

    StateLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StateLabel"));
    StateLabel->SetupAttachment(GetRootComponent());
    StateLabel->SetRelativeLocation(FVector(0, 0, 135));
    StateLabel->SetHorizontalAlignment(EHTA_Center);
    StateLabel->SetWorldSize(23);
    ShowState(TEXT("PATROL"), FColor::Cyan, 245);
}

void APatrolGuard::BeginPlay()
{
    Super::BeginPlay();
    if (LocomotionBlendSpace)
    {
        GetMesh()->PlayAnimation(LocomotionBlendSpace, true);
        GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    }
}

void APatrolGuard::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // The guard orients toward its movement, so use the forward samples of the blend space.
    if (UAnimSingleNodeInstance* Animation = GetMesh()->GetSingleNodeInstance())
    {
        const UBlendSpace* Locomotion = Cast<UBlendSpace>(Animation->GetCurrentAsset());
        if (Locomotion)
        {
            const int32 SpeedAxis = Locomotion->GetBlendParameter(0).DisplayName == TEXT("Speed") ? 0 : 1;
            const FBlendParameter& SpeedRange = Locomotion->GetBlendParameter(SpeedAxis);
            FVector BlendPosition = FVector::ZeroVector;
            BlendPosition[SpeedAxis] = FMath::Clamp(GetVelocity().Size2D(), SpeedRange.Min, SpeedRange.Max);
            Animation->SetBlendSpacePosition(BlendPosition);
        }
    }

    const APlayerController* Player = GetWorld()->GetFirstPlayerController();
    if (Player && Player->IsLocalController())
    {
        FVector Camera;
        FRotator View;
        Player->GetPlayerViewPoint(Camera, View);
        StateLabel->SetWorldRotation((Camera - StateLabel->GetComponentLocation()).Rotation());
    }
}

void APatrolGuard::ShowState(const TCHAR* Label, FColor Color, float Speed)
{
    StateLabel->SetText(FText::FromString(Label));
    StateLabel->SetTextRenderColor(Color);
    GetCharacterMovement()->MaxWalkSpeed = Speed;
}
