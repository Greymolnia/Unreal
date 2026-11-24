// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HW2GameMode.generated.h"

/**
 *  Simple GameMode for a third person game
 */
UCLASS(abstract)
class AHW2GameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	
	/** Constructor */
	AHW2GameMode();
};



