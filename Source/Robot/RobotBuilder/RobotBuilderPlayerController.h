// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RobotPlayerController.h"
#include "RobotBuilderPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class URobotBuilderComponent;

/**
 *  Concrete player controller for the robot builder sandbox.
 *  Owns the builder component and binds its handlers to the Enhanced Input
 *  actions assigned as assets.
 */
UCLASS()
class ARobotBuilderPlayerController : public ARobotPlayerController
{
	GENERATED_BODY()

public:
	/** Constructor */
	ARobotBuilderPlayerController();

protected:
	/** Input mapping context and action bindings */
	virtual void SetupInputComponent() override;

protected:
	/** Build interaction component */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<URobotBuilderComponent> BuilderComponent;

	/** Left click: place, remove or select drive target */
	UPROPERTY(EditAnywhere, Category="Input|Actions")
	TObjectPtr<UInputAction> RobotClickAction;

	/** Right click: attack the traced part */
	UPROPERTY(EditAnywhere, Category="Input|Actions")
	TObjectPtr<UInputAction> RobotAttackAction;

	/** Cycle build/demolish/drive mode */
	UPROPERTY(EditAnywhere, Category="Input|Actions")
	TObjectPtr<UInputAction> RobotCycleModeAction;

	/** Scroll wheel: yaw rotate the pending part */
	UPROPERTY(EditAnywhere, Category="Input|Actions")
	TObjectPtr<UInputAction> RobotRotateYawAction;

	/** Pitch step the pending part */
	UPROPERTY(EditAnywhere, Category="Input|Actions")
	TObjectPtr<UInputAction> RobotRotatePitchAction;

	/** WASD while driving */
	UPROPERTY(EditAnywhere, Category="Input|Actions")
	TObjectPtr<UInputAction> RobotDriveMoveAction;

	/** Space while driving */
	UPROPERTY(EditAnywhere, Category="Input|Actions")
	TObjectPtr<UInputAction> RobotThrustAction;
};
