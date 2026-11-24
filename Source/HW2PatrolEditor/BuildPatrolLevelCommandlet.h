#pragma once

#include "Commandlets/Commandlet.h"
#include "BuildPatrolLevelCommandlet.generated.h"

UCLASS()
class UBuildPatrolLevelCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
