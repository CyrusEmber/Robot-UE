// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RobotBuilderFunctionLibrary.generated.h"

class UInputAction;
class UInputMappingContext;

/**
 *  Helpers for editor scripts building the robot builder assets.
 */
UCLASS()
class URobotBuilderFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 *  Maps an action to a key inside a mapping context.
	 *  @param KeyName  FKey name, e.g. "LeftMouseButton", "W", "MouseWheelAxis"
	 *  @param bNegate  adds a negate modifier, e.g. for S/A in a WASD pair
	 *  @return true when the key name is valid and the mapping was added
	 */
	UFUNCTION(BlueprintCallable, Category="RobotBuilder")
	static bool MapKeyByName(UInputMappingContext* Context, UInputAction* Action, const FName& KeyName, bool bNegate);

	/**
	 *  Creates every asset the robot builder sandbox needs:
	 *  7 input actions, IMC_Builder, IMC_Drive and 4 part definitions.
	 *  Existing assets are reused and their values overwritten.
	 *  @return number of assets created or updated
	 */
	UFUNCTION(BlueprintCallable, Category="RobotBuilder")
	static int32 CreateBuilderAssets();
};
