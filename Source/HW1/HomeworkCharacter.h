#pragma once

#include "CoreMinimal.h"
#include "HW1Character.h"
#include "HomeworkCharacter.generated.h"

class UInputAction;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHomeworkInteractionSignature);

/** Homework extension of the template character; camera and basic input stay in the base. */
UCLASS()
class HW1_API AHomeworkCharacter : public AHW1Character
{
    GENERATED_BODY()

public:
    AHomeworkCharacter();
    virtual void Tick(float DeltaSeconds) override;
    virtual void UnPossessed() override;

    UPROPERTY(BlueprintAssignable, Category="Homework|Interaction")
    FHomeworkInteractionSignature OnInteraction;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Homework|Sprint")
    bool bIsSprinting = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Homework|Stamina")
    float Stamina = 100.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Homework|Sprint", meta=(ClampMin="1"))
    float WalkSpeed = 500.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Homework|Sprint", meta=(ClampMin="1"))
    float SprintSpeed = 900.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Homework|Stamina", meta=(ClampMin="1"))
    float MaxStamina = 100.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Homework|Stamina", meta=(ClampMin="0"))
    float StaminaDrainRate = 25.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Homework|Stamina", meta=(ClampMin="0"))
    float StaminaRegenRate = 20.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Homework|Stamina", meta=(ClampMin="0"))
    float StaminaRegenDelay = 0.8f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Homework|Stamina", meta=(ClampMin="0"))
    float MinimumSprintStamina = 20.0f;

    UFUNCTION(BlueprintPure, Category="Homework|Stamina")
    float GetStaminaFraction() const;

    UFUNCTION(BlueprintPure, Category="Homework|Stamina")
    bool IsExhaustedUntilRelease() const { return bExhaustedUntilRelease; }

    UFUNCTION(BlueprintCallable, Category="Homework|Input")
    void StartSprint();

    UFUNCTION(BlueprintCallable, Category="Homework|Input")
    void StopSprint();

    UFUNCTION(BlueprintCallable, Category="Homework|Input")
    void Interact();

protected:
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Homework|Input")
    TObjectPtr<UInputAction> SprintAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Homework|Input")
    TObjectPtr<UInputAction> InteractAction;

private:
    void SetSprinting(bool bNewSprinting);
    bool bSprintHeld = false;
    bool bExhaustedUntilRelease = false;
    float TimeSinceSprint = 0.0f;
};
