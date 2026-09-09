// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RobotTypes.h"
#include "RobotBuilderComponent.generated.h"

class ARobot;
class ARobotPart;
class URobotPartDefinition;
class URobotBuilderWidget;
class UStaticMeshComponent;
class UMaterialInterface;
class UInputMappingContext;

/** Interaction modes of the robot builder */
UENUM(BlueprintType)
enum class ERobotBuilderMode : uint8
{
	/** Select a part in the UI and click to place it */
	Build,
	/** Click parts to remove them */
	Demolish,
	/** Click a robot to drive it with WASD and Space */
	Drive
};

/**
 *  Player facing build interaction: part selection, camera trace placement,
 *  part rotation, demolishing, attacking and robot driving.
 *  Lives on the player controller.
 */
UCLASS(ClassGroup=(Robot), meta=(BlueprintSpawnableComponent))
class URobotBuilderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Constructor */
	URobotBuilderComponent();

	/** Gameplay initialization: spawns the builder widget */
	virtual void BeginPlay() override;

	/** Updates the placement preview and debug helpers */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Gameplay cleanup */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//~ Input handlers, bound by the owning player controller

	/** Left click: place, remove or select drive target depending on the mode */
	void HandleClick();

	/** Right click: apply attack damage to the traced part */
	void HandleAttack();

	/** Tab: cycle Build -> Demolish -> Drive */
	void CycleMode();

	/** Scroll wheel: rotate the pending part around the yaw axis */
	void HandleRotateYaw(const struct FInputActionValue& Value);

	/** Key: rotate the pending part around the pitch axis */
	void HandleRotatePitch();

	/** WASD while in drive mode */
	void HandleDriveMove(const struct FInputActionValue& Value);

	/** Space pressed while in drive mode */
	void HandleThrustStart();

	/** Space released while in drive mode */
	void HandleThrustStop();

	//~ API used by the builder widget

	const TArray<TObjectPtr<URobotPartDefinition>>& GetPartDefinitions() const { return PartDefinitions; }

	/** Selects a part definition and switches to build mode */
	void SelectPartDefinition(int32 Index);

	ERobotBuilderMode GetMode() const { return Mode; }
	void SetMode(ERobotBuilderMode NewMode);

	ARobot* GetDriveTarget() const { return DriveTarget.Get(); }

	/** Sets the drive mapping context used while in drive mode */
	void SetDriveMappingContext(UInputMappingContext* Context) { DriveMappingContext = Context; }

	/** Sets the pawn movement mapping context that is swapped out while driving */
	void SetPawnMovementContext(UInputMappingContext* Context) { PawnMovementContext = Context; }

	/** Sets the part definitions offered by the build UI */
	void SetPartDefinitions(const TArray<TObjectPtr<URobotPartDefinition>>& Definitions) { PartDefinitions = Definitions; }

protected:
	/** Traces from the player camera. Returns true on blocking hit. */
	bool TraceFromCamera(FHitResult& OutHit) const;

	/** Computes the transform for a part about to be placed at the hit point */
	FTransform ComputePlacementTransform(const FHitResult& Hit) const;

	/** Moves, rotates and shows/hides the preview mesh */
	void UpdatePreview(const FHitResult& Hit, bool bTraceHit);

	/** Creates the preview mesh component if it does not exist yet */
	void EnsurePreviewMesh();

	/** Runs the action for the current mode at the traced location */
	void ExecuteModeAction(const FHitResult& Hit);

	/** Spawns a new robot made of a core part on world geometry */
	ARobot* TrySpawnRobotAt(const FHitResult& Hit);

	/** Attaches the selected part to the traced robot part */
	void TryAttachPart(const FHitResult& Hit, ARobotPart* HitPart);

	/** Destroys a detached debris part outright */
	void DemolishDebris(ARobotPart* DebrisPart);

	/** Applies attack damage to the traced part */
	void AttackTracedPart(const FHitResult& Hit);

	/** Swaps the pawn movement context for the drive context when entering/leaving drive mode */
	void UpdateDriveMappingContext();

	/** Points drive input at another robot */
	void SetDriveTarget(ARobot* NewTarget);

	/** Returns the currently selected part definition */
	URobotPartDefinition* GetCurrentDefinition() const;

	UFUNCTION()
	void OnRobotDestroyed(ARobot* DestroyedRobot);

protected:
	/** Part types offered by the build UI */
	UPROPERTY(EditAnywhere, Category="Builder")
	TArray<TObjectPtr<URobotPartDefinition>> PartDefinitions;

	/** Widget class shown while playing */
	UPROPERTY(EditAnywhere, Category="Builder")
	TSubclassOf<URobotBuilderWidget> BuilderWidgetClass;

	/** Optional translucent material for the placement preview */
	UPROPERTY(EditAnywhere, Category="Builder")
	TObjectPtr<UMaterialInterface> PreviewMaterial;

	/** Trace distance from the camera */
	UPROPERTY(EditAnywhere, Category="Builder", meta=(ClampMin=100, ClampMax=20000, Units="cm"))
	float TraceDistance = 3000.0f;

	/** Extra clearance between the hit surface and the spawned part */
	UPROPERTY(EditAnywhere, Category="Builder", meta=(ClampMin=0, ClampMax=50))
	float PlacementClearance = 2.0f;

	/** Yaw rotation step in degrees applied per scroll notch */
	UPROPERTY(EditAnywhere, Category="Builder|Rotation", meta=(ClampMin=1, ClampMax=90, Units="Degrees"))
	float RotateYawStep = 15.0f;

	/** Pitch rotation step in degrees applied per key press */
	UPROPERTY(EditAnywhere, Category="Builder|Rotation", meta=(ClampMin=1, ClampMax=90, Units="Degrees"))
	float RotatePitchStep = 45.0f;

	/** Draw the center of mass of the drive target */
	UPROPERTY(EditAnywhere, Category="Builder|Debug")
	bool bDrawCenterOfMass = true;

	/** Drive mapping context added while in drive mode */
	UPROPERTY(EditAnywhere, Category="Builder|Input")
	TObjectPtr<UInputMappingContext> DriveMappingContext;

	/** Pawn movement mapping context removed while in drive mode so WASD stops walking */
	UPROPERTY(EditAnywhere, Category="Builder|Input")
	TObjectPtr<UInputMappingContext> PawnMovementContext;

private:
	/** Current interaction mode */
	ERobotBuilderMode Mode = ERobotBuilderMode::Build;

	/** Index of the selected part definition */
	int32 SelectedPartIndex = INDEX_NONE;

	/** Rotation accumulated for the next placement */
	FRotator PendingRotation = FRotator::ZeroRotator;

	/** Builder UI instance */
	UPROPERTY(Transient)
	TObjectPtr<URobotBuilderWidget> BuilderWidget;

	/** Ghost mesh shown where the next part will be placed */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> PreviewMesh;

	/** Robot currently receiving drive input */
	TWeakObjectPtr<ARobot> DriveTarget;
};
