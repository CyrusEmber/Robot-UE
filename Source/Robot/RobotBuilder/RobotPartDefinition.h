// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RobotTypes.h"
#include "RobotPartDefinition.generated.h"

class UStaticMesh;

/**
 *  Data asset describing one type of robot part.
 *  The builder UI lists these assets and spawns ARobotPart actors from them.
 */
UCLASS(Blueprintable)
class URobotPartDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Display name of this part type shown in the builder UI */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Part")
	FText PartName;

	/** Category determines connection and drive behavior */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Part")
	ERobotPartCategory Category = ERobotPartCategory::Armor;

	/** Mesh used for every part spawned from this definition */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Part")
	TObjectPtr<UStaticMesh> Mesh;

	/** Mass in kg. Values <= 0 fall back to the mesh body setup mass */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Part|Physics", meta=(ClampMin=0))
	float Mass = 50.0f;

	/** Hit points of the part body itself */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Part|Health", meta=(ClampMin=1))
	float MaxHP = 100.0f;

	/** Hit points of the connection created when this part is attached */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Part|Health", meta=(ClampMin=1))
	float MaxJointHP = 100.0f;

	/** A single impact at or above this impulse magnitude instantly breaks the connection */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Part|Damage", meta=(ClampMin=0))
	float BreakImpulse = 60000.0f;

	/** Impacts weaker than this deal no damage at all */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Part|Damage", meta=(ClampMin=0))
	float MinImpactImpulse = 3000.0f;

	/** Damage applied per impulse unit above the threshold */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Part|Damage", meta=(ClampMin=0))
	float ImpulseDamageScale = 0.004f;

	/** Damage dealt to the part and its connection by a single player attack */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Part|Damage", meta=(ClampMin=0))
	float AttackDamage = 25.0f;

	/** Torque applied around the wheel axle while driving, in kg*cm^2/s^2 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Part|Wheel", meta=(ClampMin=0))
	float WheelTorque = 400000.0f;

	/** Wheel spin rate cap in rad/s */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Part|Wheel", meta=(ClampMin=1))
	float MaxWheelSpinRate = 40.0f;

	/** Fraction of drive torque used to brake the wheel when input is released, 0..1 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Part|Wheel", meta=(ClampMin=0, ClampMax=1))
	float WheelBrakeFactor = 0.35f;

	/** Thrust force applied along local +X while driving, in kg*cm/s^2 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Part|Thruster", meta=(ClampMin=0))
	float ThrustForce = 80000.0f;

	/** Total wheel torque this core can supply. Demand above the budget is scaled down proportionally, so extra wheels or mass just accelerate slower */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Part|Core", meta=(ClampMin=0))
	float DriveTorqueBudget = 1000000.0f;

	/** Total thruster force this core can supply. Demand above the budget is scaled down proportionally */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Part|Core", meta=(ClampMin=0))
	float ThrustForceBudget = 160000.0f;

	bool IsCore() const { return Category == ERobotPartCategory::Core; }
	bool IsWheel() const { return Category == ERobotPartCategory::Wheel; }
	bool IsThruster() const { return Category == ERobotPartCategory::Thruster; }
};
