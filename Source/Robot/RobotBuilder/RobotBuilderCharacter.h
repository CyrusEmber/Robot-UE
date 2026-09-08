// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RobotCharacter.h"
#include "RobotBuilderCharacter.generated.h"

/**
 *  Concrete player pawn for the robot builder sandbox.
 *  Wires the template input action assets so the pawn moves out of the box.
 */
UCLASS()
class ARobotBuilderCharacter : public ARobotCharacter
{
	GENERATED_BODY()

public:
	/** Constructor */
	ARobotBuilderCharacter();
};
