// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RobotTypes.h"
#include "RobotPart.generated.h"

class ARobot;
class URobotPartDefinition;
class UStaticMeshComponent;
class UPhysicsConstraintComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRobotPartDamagedSignature, ARobotPart*, Part, float, NewHP);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRobotPartDiedSignature, ARobotPart*, Part);

/**
 *  One node of a robot part graph.
 *  A simulating rigid body with its own hit points that reports impact
 *  damage to the owning robot. Once detached it keeps simulating as debris.
 */
UCLASS()
class ARobotPart : public AActor
{
	GENERATED_BODY()

public:
	/** Constructor */
	ARobotPart();

	/** Applies mesh, mass and health from a definition. Call right after spawning. */
	void InitFromDefinition(URobotPartDefinition* InDefinition);

	/** Handles damage routed through the engine damage pipeline */
	virtual float TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	/** Applies damage to the part body itself */
	void ApplyPartDamage(float Amount);

	/** Marks the part as free-floating debris: no more damage reporting */
	void MarkAsDebris();

	/** Returns the robot graph this part currently belongs to, null once detached */
	ARobot* GetOwningRobot() const { return OwningRobot; }
	void SetOwningRobot(ARobot* InRobot) { OwningRobot = InRobot; }

	/** Constraint connecting this part to its parent. Managed by ARobot. */
	UPhysicsConstraintComponent* GetJoint() const { return Joint; }
	void SetJoint(UPhysicsConstraintComponent* InJoint) { Joint = InJoint; }

	URobotPartDefinition* GetDefinition() const { return Definition; }
	UStaticMeshComponent* GetMeshComponent() const { return MeshComponent; }
	ERobotPartCategory GetCategory() const;

	float GetCurrentHP() const { return CurrentHP; }
	float GetMaxHP() const { return MaxHP; }
	float GetHPRatio() const { return MaxHP > 0.0f ? CurrentHP / MaxHP : 0.0f; }
	bool IsDead() const { return bDead; }

protected:
	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Routes collision impulses to the owning robot as damage */
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

protected:
	/** Simulating rigid body of this part */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	/** Definition this part was spawned from */
	UPROPERTY(BlueprintReadOnly, Category="Robot", meta=(AllowPrivateAccess="true"))
	TObjectPtr<URobotPartDefinition> Definition;

	/** Graph owner while attached to a robot */
	UPROPERTY(BlueprintReadOnly, Category="Robot", meta=(AllowPrivateAccess="true"))
	TObjectPtr<ARobot> OwningRobot;

	/** Constraint to the parent part */
	UPROPERTY(BlueprintReadOnly, Category="Robot", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UPhysicsConstraintComponent> Joint;

	/** Part body hit points */
	float CurrentHP = 0.0f;
	float MaxHP = 0.0f;

	/** Dead parts no longer report damage */
	bool bDead = false;

	/** Game time of the last applied impact damage */
	float LastImpactDamageTime = -1000.0f;

public:
	/** Minimum time in seconds between two impact damage applications */
	UPROPERTY(EditAnywhere, Category="Robot|Damage")
	float ImpactDamageCooldown = 0.1f;

	/** Broadcast whenever the part body takes damage */
	FRobotPartDamagedSignature OnPartDamaged;

	/** Broadcast when the part body is destroyed */
	FRobotPartDiedSignature OnPartDied;
};
