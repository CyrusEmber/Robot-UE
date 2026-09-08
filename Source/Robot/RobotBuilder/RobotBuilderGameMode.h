// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RobotGameMode.h"
#include "RobotBuilderGameMode.generated.h"

/**
 *  GameMode for the robot builder sandbox.
 *  Uses the builder pawn and player controller.
 */
UCLASS()
class ARobotBuilderGameMode : public ARobotGameMode
{
	GENERATED_BODY()

public:
	/** Constructor */
	ARobotBuilderGameMode();
};
