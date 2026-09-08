// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RobotTypes.generated.h"

class ARobotPart;
class UPhysicsConstraintComponent;

/** Categories of robot parts recognized by the builder system */
UENUM(BlueprintType)
enum class ERobotPartCategory : uint8
{
	/** Root node of a robot part graph. Every robot needs exactly one. */
	Core,
	/** Free-spinning wheel driven by torque around its local X axis */
	Wheel,
	/** Applies thrust force along its local +X axis while driving */
	Thruster,
	/** Dead weight with extra hit points */
	Armor
};

/**
 *  One edge of the robot part graph.
 *  Realized physically by a physics constraint between two simulating
 *  part bodies. The edge carries its own health so heavy impacts can
 *  shake a part loose even when the part body itself survives.
 */
USTRUCT(BlueprintType)
struct FRobotLink
{
	GENERATED_BODY()

	/** Unique id of this edge */
	UPROPERTY(BlueprintReadOnly)
	FGuid LinkId;

	/** Parent side of the edge, closer to the core */
	UPROPERTY(BlueprintReadOnly)
	TWeakObjectPtr<ARobotPart> Parent;

	/** Child side of the edge, the attached part */
	UPROPERTY(BlueprintReadOnly)
	TWeakObjectPtr<ARobotPart> Child;

	/** Constraint realizing this link physically */
	UPROPERTY(BlueprintReadOnly)
	TWeakObjectPtr<UPhysicsConstraintComponent> Constraint;

	/** Remaining health of the connection itself */
	UPROPERTY(BlueprintReadOnly)
	float JointHP = 100.0f;

	/** Health of the connection when it was created */
	UPROPERTY(BlueprintReadOnly)
	float MaxJointHP = 100.0f;
};
