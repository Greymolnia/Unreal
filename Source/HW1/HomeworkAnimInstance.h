#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "HomeworkAnimInstance.generated.h"

/** Parent for the standard ABP_Unarmed. Updated on the game thread before graph evaluation. */
UCLASS(Transient, Blueprintable)
class HW1_API UHomeworkAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    UPROPERTY(Transient, BlueprintReadOnly, Category="Homework|Animation")
    bool bIsSprinting = false;
};
