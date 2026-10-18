#include "HomeworkCharacter.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HW1.h"

AHomeworkCharacter::AHomeworkCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AHomeworkCharacter::BeginPlay()
{
    Super::BeginPlay();
    MaxStamina = FMath::Max(1.0f, MaxStamina);
    Stamina = MaxStamina;
    SetSprinting(false);
}

void AHomeworkCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (ensureMsgf(SprintAction, TEXT("Assign IA_Sprint in BP_ThirdPersonCharacter")))
        {
            Input->BindAction(SprintAction, ETriggerEvent::Started, this, &AHomeworkCharacter::StartSprint);
            Input->BindAction(SprintAction, ETriggerEvent::Completed, this, &AHomeworkCharacter::StopSprint);
            Input->BindAction(SprintAction, ETriggerEvent::Canceled, this, &AHomeworkCharacter::StopSprint);
        }
        if (ensureMsgf(InteractAction, TEXT("Assign IA_Interact in BP_ThirdPersonCharacter")))
        {
            Input->BindAction(InteractAction, ETriggerEvent::Started, this, &AHomeworkCharacter::Interact);
        }
    }
}

void AHomeworkCharacter::SetSprinting(bool bNewSprinting)
{
    bIsSprinting = bNewSprinting;
    GetCharacterMovement()->MaxWalkSpeed = bIsSprinting ? SprintSpeed : WalkSpeed;
}

void AHomeworkCharacter::StartSprint()
{
    // Exhaustion requires releasing Shift; regeneration alone never restarts sprint.
    bSprintHeld = true;
    if (!bExhaustedUntilRelease && Stamina > 0.0f
        && Stamina >= FMath::Clamp(MinimumSprintStamina, 0.0f, MaxStamina))
    {
        SetSprinting(true);
        TimeSinceSprint = 0.0f;
    }
}

void AHomeworkCharacter::StopSprint()
{
    bSprintHeld = false;
    bExhaustedUntilRelease = false;
    SetSprinting(false);
}

void AHomeworkCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const float Step = FMath::Max(0.0f, DeltaSeconds);
    if (bIsSprinting)
    {
        Stamina = FMath::Clamp(Stamina - FMath::Max(0.0f, StaminaDrainRate) * Step, 0.0f, MaxStamina);
        TimeSinceSprint = 0.0f;
        if (Stamina <= 0.0f)
        {
            bExhaustedUntilRelease = bSprintHeld;
            SetSprinting(false);
        }
    }
    else
    {
        // Only the portion of a frame after the delay contributes to recovery.
        const float Delay = FMath::Max(0.0f, StaminaRegenDelay);
        const float PreviousRecoveryTime = FMath::Max(0.0f, TimeSinceSprint - Delay);
        TimeSinceSprint += Step;
        const float RecoveryStep = FMath::Max(0.0f, TimeSinceSprint - Delay) - PreviousRecoveryTime;
        Stamina = FMath::Clamp(Stamina + FMath::Max(0.0f, StaminaRegenRate) * RecoveryStep, 0.0f, MaxStamina);
    }
}

void AHomeworkCharacter::UnPossessed()
{
    StopSprint();
    Super::UnPossessed();
}

void AHomeworkCharacter::Interact()
{
    OnInteraction.Broadcast();
}

float AHomeworkCharacter::GetStaminaFraction() const
{
    return MaxStamina > 0.0f ? FMath::Clamp(Stamina / MaxStamina, 0.0f, 1.0f) : 0.0f;
}
