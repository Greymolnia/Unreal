#include "HomeworkCharacter.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/HUD.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HW2.h"

AHomeworkCharacter::AHomeworkCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AHomeworkCharacter::BeginPlay()
{
    Super::BeginPlay();
    MaxStamina = FMath::Max(1.0f, MaxStamina);
    Stamina = MaxStamina;
    AHUD::OnHUDPostRender.AddUObject(this, &AHomeworkCharacter::DrawNotices);
    SetSprinting(false);
}

void AHomeworkCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
    AHUD::OnHUDPostRender.RemoveAll(this);
    Super::EndPlay(Reason);
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
    InteractionNotice = {
        NSLOCTEXT("DepotHUD", "Interaction", "Interaction triggered"),
        FLinearColor(0.16f, 0.86f, 0.70f), GetWorld()->GetTimeSeconds() + 2.5
    };
}

void AHomeworkCharacter::ShowPatrolNotice(const FText& Message, FLinearColor Accent)
{
    PatrolNotice = { Message, Accent, GetWorld()->GetTimeSeconds() + 3.2 };
}

void AHomeworkCharacter::DrawNotices(AHUD* HUD, UCanvas* Canvas)
{
    if (!Canvas || HUD->GetOwningPawn() != this)
    {
        return;
    }

    constexpr float Margin = 20.0f;
    constexpr float CardHeight = 42.0f;
    float Y = Canvas->ClipX >= 780.0f ? Margin : 240.0f;
    const double Now = GetWorld()->GetTimeSeconds();
    for (const FHudNotice* Notice : { &PatrolNotice, &InteractionNotice })
    {
        if (Now >= Notice->ExpiresAt)
        {
            continue;
        }

        UFont* Font = GEngine->GetMediumFont();
        float TextWidth = 0;
        float TextHeight = 0;
        Canvas->StrLen(Font, Notice->Message.ToString(), TextWidth, TextHeight);
        const float Width = TextWidth + 38.0f;
        const float X = FMath::Max(10.0f, Canvas->ClipX - Margin - Width);

        FCanvasTileItem Panel(FVector2D(X, Y), FVector2D(Width, CardHeight),
            FLinearColor(0.018f, 0.041f, 0.057f, 0.94f));
        Panel.BlendMode = SE_BLEND_Translucent;
        Canvas->DrawItem(Panel);
        FCanvasTileItem Stripe(FVector2D(X, Y), FVector2D(4, CardHeight), Notice->Accent);
        Canvas->DrawItem(Stripe);
        FCanvasTextItem Text(FVector2D(X + 17, Y + (CardHeight - TextHeight) * 0.5f),
            Notice->Message, Font, FLinearColor::White);
        Canvas->DrawItem(Text);
        Y += CardHeight + 8;
    }
}

float AHomeworkCharacter::GetStaminaFraction() const
{
    return MaxStamina > 0.0f ? FMath::Clamp(Stamina / MaxStamina, 0.0f, 1.0f) : 0.0f;
}
