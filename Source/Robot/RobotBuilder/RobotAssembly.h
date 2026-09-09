// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RobotTypes.h"
#include "RobotPart.h"
#include "RobotAssembly.generated.h"

class URobotPartDefinition;
class UPhysicsConstraintComponent;
class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRobotPartAddedSignature, ARobotPart*, Part);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRobotPartDetachedSignature, ARobotPart*, Part);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRobotDestroyedSignature, ARobot*, Robot);

/**
 *  Manager and graph model of one physics robot.
 *
 *  Nodes are ARobotPart actors, edges are physics constraints carrying their
 *  own joint health. Whenever the graph changes, a BFS from the core part
 *  detaches every part that lost its path to the core, so cut-off subtrees
 *  fall off and keep simulating as debris.
 */
UCLASS()
class ARobot : public AActor
{
	GENERATED_BODY()

public:
	/** Constructor */
	ARobot();

	/** Spawns a new robot made of a single core part */
	static ARobot* SpawnRobot(UWorld* World, URobotPartDefinition* CoreDefinition, const FTransform& SpawnTransform);

	/**
	 *  Attaches a new part to an existing part of this robot.
	 *  The part is spawned with the given transform and connected to the
	 *  parent through a constraint anchored at AnchorWorldLocation.
	 */
	ARobotPart* AddPart(URobotPartDefinition* Definition, ARobotPart* ParentPart, const FTransform& PartTransform, const FVector& AnchorWorldLocation);

	/** Demolishes a part: destroys the actor and detaches anything it supported */
	bool RemovePart(ARobotPart* Part);

	/** Applies damage to a part body and its connection simultaneously */
	void ApplyImpactDamage(ARobotPart* Part, float Amount);

	/** Applies the definition's attack damage to a part body and its connection */
	void ApplyAttackDamage(ARobotPart* Part);

	/** Breaks the edge between the part and its parent, then re-runs connectivity */
	void BreakLink(ARobotPart* ChildPart);

	/** Called by parts when their body hit points reach zero */
	void NotifyPartDied(ARobotPart* Part);

	/** Total mass of all parts still connected to the core */
	float GetTotalMass() const;

	/** World space center of mass of all parts still connected to the core */
	FVector GetCenterOfMassWorld() const;

	/** Drive demand vs core supply budget from the last drive update, 0..N (1.0 = exactly at the core budget) */
	float GetDrivePowerUsage() const { return DrivePowerUsage; }

	/** Drive input: X = forward/back, Y = turn left/right */
	void SetDriveInput(FVector2D InDriveInput) { DriveInput = InDriveInput; }

	/** 0..1 thrust input forwarded to every connected thruster */
	void SetThrustInput(float InThrustInput) { ThrustInput = FMath::Clamp(InThrustInput, 0.0f, 1.0f); }

	/** Parts still connected to the core */
	const TArray<TObjectPtr<ARobotPart>>& GetParts() const { return Parts; }

	/** Core node of the graph */
	ARobotPart* GetCorePart() const { return CorePart; }

protected:
	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Applies wheel torques and thruster forces every frame */
	virtual void Tick(float DeltaSeconds) override;

	/** Gameplay cleanup */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Rebuilds node adjacency from the link list */
	TMap<ARobotPart*, TArray<ARobotPart*>> BuildAdjacency() const;

	/** BFS from the core; detaches every part that cannot reach it */
	void RunConnectivityPass();

	/**
	 *  Unregisters a part from this robot without destroying it.
	 *  bSeverParentLink=false keeps its remaining links so detached
	 *  subtrees fall off as one chunk.
	 */
	void DetachPart(ARobotPart* Part, bool bSeverParentLink = true);

	/** Removes and breaks the edge connecting a part to its parent */
	void DestroyLinkForChild(ARobotPart* ChildPart);

	/** Removes and breaks every edge that references the part */
	void DestroyAllLinksForPart(ARobotPart* Part);

	/** Creates the physics constraint realizing one graph edge */
	UPhysicsConstraintComponent* CreateLinkConstraint(ARobotPart* ParentPart, ARobotPart* ChildPart, const FVector& AnchorWorldLocation);

	/** Applies wheel torques and thruster forces for this frame */
	void ApplyDriveForces();

	/** Tears the whole robot apart and destroys it */
	void DestroyRobot();

protected:
	/** Root scene component */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Core node of the graph */
	UPROPERTY(BlueprintReadOnly, Category="Robot")
	TObjectPtr<ARobotPart> CorePart;

	/** Every part currently connected to this robot */
	UPROPERTY(BlueprintReadOnly, Category="Robot")
	TArray<TObjectPtr<ARobotPart>> Parts;

	/** Every edge currently active in the graph */
	UPROPERTY(BlueprintReadOnly, Category="Robot")
	TArray<FRobotLink> Links;

	/** Seconds a detached part survives as debris. 0 = forever */
	UPROPERTY(EditAnywhere, Category="Robot|Debris")
	float DebrisLifetime = 0.0f;

	/** Forward/back and turn drive input */
	FVector2D DriveInput = FVector2D::ZeroVector;

	/** Current thrust input */
	float ThrustInput = 0.0f;

	/** Drive demand vs core budget from the last ApplyDriveForces, 0..N */
	float DrivePowerUsage = 0.0f;

	/** True while the robot is being torn apart */
	bool bDestroying = false;

public:
	/** Broadcast whenever a part is attached to the robot */
	FRobotPartAddedSignature OnPartAdded;

	/** Broadcast whenever a connected part takes damage */
	FRobotPartDamagedSignature OnPartDamaged;

	/** Broadcast whenever a part is detached from the robot */
	FRobotPartDetachedSignature OnPartDetached;

	/** Broadcast right before the robot is destroyed */
	FRobotDestroyedSignature OnRobotDestroyed;
};
