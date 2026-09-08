// Copyright Epic Games, Inc. All Rights Reserved.

#include "RobotBuilderGameMode.h"
#include "RobotBuilderCharacter.h"
#include "RobotBuilderPlayerController.h"

ARobotBuilderGameMode::ARobotBuilderGameMode()
{
	DefaultPawnClass = ARobotBuilderCharacter::StaticClass();
	PlayerControllerClass = ARobotBuilderPlayerController::StaticClass();
}
