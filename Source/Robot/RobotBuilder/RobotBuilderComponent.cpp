// Copyright Epic Games, Inc. All Rights Reserved.

#include "RobotBuilderComponent.h"
#include "RobotAssembly.h"
#include "RobotPart.h"
#include "RobotPartDefinition.h"
#include "RobotTypes.h"
#include "RobotBuilderWidget.h"
#include "Blueprint/UserWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

URobotBuilderComponent::URobotBuilderComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	BuilderWidgetClass = URobotBuilderWidget::StaticClass();
}

void URobotBuilderComponent::BeginPlay()
{
	Super::BeginPlay();

	// spawn the builder UI for local players
	APlayerController* OwnerPC = Cast<APlayerController>(GetOwner());
	if (OwnerPC && OwnerPC->IsLocalPlayerController() && BuilderWidgetClass)
	{
		BuilderWidget = CreateWidget<URobotBuilderWidget>(OwnerPC, BuilderWidgetClass);
		if (BuilderWidget)
		{
			BuilderWidget->AddToViewport(0);
		}
	}
}

void URobotBuilderComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetDriveTarget(nullptr);

	if (BuilderWidget)
	{
		BuilderWidget->RemoveFromParent();
		BuilderWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void URobotBuilderComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	FHitResult Hit;
	const bool bTraceHit = TraceFromCamera(Hit);
	UpdatePreview(Hit, bTraceHit);

	// draw the center of mass of the drive target so weight shifts are visible
	if (bDrawCenterOfMass && DriveTarget.IsValid() && DriveTarget->GetCorePart())
	{
		const FVector CenterOfMass = DriveTarget->GetCenterOfMassWorld();
		DrawDebugPoint(GetWorld(), CenterOfMass, 12.0f, FColor::Red, false, -1.0f);
		DrawDebugDirectionalArrow(GetWorld(), CenterOfMass, CenterOfMass + FVector(0.0f, 0.0f, 50.0f), 120.0f, FColor::Yellow, false, -1.0f, 0, 3.0f);
	}
}

bool URobotBuilderComponent::TraceFromCamera(FHitResult& OutHit) const
{
	const APlayerController* OwnerPC = Cast<APlayerController>(GetOwner());
	UWorld* World = GetWorld();
	if (!OwnerPC || !World)
	{
		return false;
	}

	FVector CameraLocation;
	FRotator CameraRotation;
	OwnerPC->GetPlayerViewPoint(CameraLocation, CameraRotation);

	const FVector TraceEnd = CameraLocation + CameraRotation.Vector() * TraceDistance;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RobotBuilderTrace), true);
	if (APawn* PlayerPawn = OwnerPC->GetPawn())
	{
		QueryParams.AddIgnoredActor(PlayerPawn);
	}
	if (PreviewMesh)
	{
		QueryParams.AddIgnoredComponent(PreviewMesh.Get());
	}

	return World->LineTraceSingleByChannel(OutHit, CameraLocation, TraceEnd, ECC_Visibility, QueryParams);
}

URobotPartDefinition* URobotBuilderComponent::GetCurrentDefinition() const
{
	return PartDefinitions.IsValidIndex(SelectedPartIndex) ? PartDefinitions[SelectedPartIndex].Get() : nullptr;
}

FTransform URobotBuilderComponent::ComputePlacementTransform(const FHitResult& Hit) const
{
	URobotPartDefinition* Definition = GetCurrentDefinition();
	if (!Definition || !Definition->Mesh)
	{
		return FTransform::Identity;
	}

	// offset the part from the surface by its projected half extent
	const FVector BoxExtent = Definition->Mesh->GetBounds().BoxExtent;
	const float HalfExtentAlongNormal = FVector::DotProduct(BoxExtent, Hit.Normal.GetAbs());
	const FVector SpawnLocation = Hit.Location + Hit.Normal * (HalfExtentAlongNormal + PlacementClearance);

	FRotator SpawnRotation = PendingRotation;

	// wheels start with their axle perpendicular to the view direction
	if (Definition->IsWheel())
	{
		if (const APlayerController* OwnerPC = Cast<APlayerController>(GetOwner()))
		{
			FVector CameraLocation;
			FRotator CameraRotation;
			OwnerPC->GetPlayerViewPoint(CameraLocation, CameraRotation);
			SpawnRotation.Yaw = CameraRotation.Yaw + 90.0f + PendingRotation.Yaw;
		}
	}

	return FTransform(SpawnRotation, SpawnLocation);
}

void URobotBuilderComponent::EnsurePreviewMesh()
{
	if (PreviewMesh)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	PreviewMesh = NewObject<UStaticMeshComponent>(Owner, TEXT("RobotPreviewMesh"));
	PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewMesh->SetCastShadow(false);
	PreviewMesh->SetVisibility(false, false);

	if (PreviewMaterial)
	{
		PreviewMesh->SetMaterial(0, PreviewMaterial);
	}

	PreviewMesh->RegisterComponent();
}

void URobotBuilderComponent::UpdatePreview(const FHitResult& Hit, bool bTraceHit)
{
	URobotPartDefinition* Definition = GetCurrentDefinition();
	const bool bShowPreview = bTraceHit && Mode == ERobotBuilderMode::Build && Definition && Definition->Mesh;

	if (!bShowPreview)
	{
		if (PreviewMesh)
		{
			PreviewMesh->SetVisibility(false, false);
		}
		return;
	}

	EnsurePreviewMesh();
	if (!PreviewMesh)
	{
		return;
	}

	// keep the ghost attached to the current pawn so it follows respawns
	APawn* PlayerPawn = Cast<APlayerController>(GetOwner()) ? Cast<APlayerController>(GetOwner())->GetPawn() : nullptr;
	if (PlayerPawn && PlayerPawn->GetRootComponent() && PreviewMesh->GetAttachParent() != PlayerPawn->GetRootComponent())
	{
		PreviewMesh->AttachToComponent(PlayerPawn->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
	}

	PreviewMesh->SetStaticMesh(Definition->Mesh);
	PreviewMesh->SetWorldTransform(ComputePlacementTransform(Hit));
	PreviewMesh->SetVisibility(true, false);
}

void URobotBuilderComponent::HandleClick()
{
	FHitResult Hit;
	if (!TraceFromCamera(Hit))
	{
		return;
	}

	ExecuteModeAction(Hit);
}

void URobotBuilderComponent::ExecuteModeAction(const FHitResult& Hit)
{
	ARobotPart* HitPart = Cast<ARobotPart>(Hit.GetActor());

	switch (Mode)
	{
	case ERobotBuilderMode::Build:
	{
		if (HitPart && HitPart->GetOwningRobot())
		{
			TryAttachPart(Hit, HitPart);
		}
		else
		{
			TrySpawnRobotAt(Hit);
		}
		break;
	}

	case ERobotBuilderMode::Demolish:
	{
		if (HitPart)
		{
			if (ARobot* HitRobot = HitPart->GetOwningRobot())
			{
				HitRobot->RemovePart(HitPart);
			}
			else
			{
				DemolishDebris(HitPart);
			}
		}
		break;
	}

	case ERobotBuilderMode::Drive:
	{
		SetDriveTarget(HitPart ? HitPart->GetOwningRobot() : nullptr);
		break;
	}
	}
}

ARobot* URobotBuilderComponent::TrySpawnRobotAt(const FHitResult& Hit)
{
	URobotPartDefinition* Definition = GetCurrentDefinition();
	if (!Definition)
	{
		UE_LOG(LogTemp, Warning, TEXT("RobotBuilder: select a part definition first."));
		return nullptr;
	}

	if (!Definition->IsCore())
	{
		UE_LOG(LogTemp, Warning, TEXT("RobotBuilder: only core parts can start a new robot. Aim at an existing part to attach."));
		return nullptr;
	}

	return ARobot::SpawnRobot(GetWorld(), Definition, ComputePlacementTransform(Hit));
}

void URobotBuilderComponent::TryAttachPart(const FHitResult& Hit, ARobotPart* HitPart)
{
	URobotPartDefinition* Definition = GetCurrentDefinition();
	ARobot* HitRobot = HitPart ? HitPart->GetOwningRobot() : nullptr;
	if (!Definition || !HitRobot)
	{
		return;
	}

	// placing a core against a robot simply starts another robot there
	if (Definition->IsCore())
	{
		TrySpawnRobotAt(Hit);
		return;
	}

	HitRobot->AddPart(Definition, HitPart, ComputePlacementTransform(Hit), Hit.Location);
}

void URobotBuilderComponent::DemolishDebris(ARobotPart* DebrisPart)
{
	if (DebrisPart)
	{
		DebrisPart->Destroy();
	}
}

void URobotBuilderComponent::HandleAttack()
{
	FHitResult Hit;
	if (!TraceFromCamera(Hit))
	{
		return;
	}

	AttackTracedPart(Hit);
}

void URobotBuilderComponent::AttackTracedPart(const FHitResult& Hit)
{
	ARobotPart* HitPart = Cast<ARobotPart>(Hit.GetActor());
	if (!HitPart)
	{
		return;
	}

	if (ARobot* HitRobot = HitPart->GetOwningRobot())
	{
		HitRobot->ApplyAttackDamage(HitPart);
	}
	else if (HitPart->GetDefinition())
	{
		HitPart->ApplyPartDamage(HitPart->GetDefinition()->AttackDamage);
	}
}

void URobotBuilderComponent::CycleMode()
{
	switch (Mode)
	{
	case ERobotBuilderMode::Build:
		SetMode(ERobotBuilderMode::Demolish);
		break;
	case ERobotBuilderMode::Demolish:
		SetMode(ERobotBuilderMode::Drive);
		break;
	case ERobotBuilderMode::Drive:
		SetMode(ERobotBuilderMode::Build);
		break;
	}
}

void URobotBuilderComponent::HandleRotateYaw(const FInputActionValue& Value)
{
	const float Delta = Value.Get<float>();

	if (FMath::Abs(Delta) > KINDA_SMALL_NUMBER)
	{
		PendingRotation.Yaw += FMath::Sign(Delta) * RotateYawStep;
	}
}

void URobotBuilderComponent::HandleRotatePitch()
{
	PendingRotation.Pitch += RotatePitchStep;
	PendingRotation.Normalize();
}

void URobotBuilderComponent::HandleDriveMove(const FInputActionValue& Value)
{
	const FVector2D AxisValue = Value.Get<FVector2D>();

	if (ARobot* Target = DriveTarget.Get())
	{
		// X = forward/back, Y = turn left/right
		Target->SetDriveInput(FVector2D(AxisValue.Y, AxisValue.X));
	}
}

void URobotBuilderComponent::HandleThrustStart()
{
	if (ARobot* Target = DriveTarget.Get())
	{
		Target->SetThrustInput(1.0f);
	}
}

void URobotBuilderComponent::HandleThrustStop()
{
	if (ARobot* Target = DriveTarget.Get())
	{
		Target->SetThrustInput(0.0f);
	}
}

void URobotBuilderComponent::SelectPartDefinition(int32 Index)
{
	if (PartDefinitions.IsValidIndex(Index))
	{
		SelectedPartIndex = Index;
		SetMode(ERobotBuilderMode::Build);
	}
}

void URobotBuilderComponent::SetMode(ERobotBuilderMode NewMode)
{
	if (Mode == NewMode)
	{
		return;
	}

	// stop driving input when leaving drive mode
	if (Mode == ERobotBuilderMode::Drive)
	{
		if (ARobot* Target = DriveTarget.Get())
		{
			Target->SetDriveInput(FVector2D::ZeroVector);
			Target->SetThrustInput(0.0f);
		}
	}

	Mode = NewMode;
	UpdateDriveMappingContext();
}

void URobotBuilderComponent::UpdateDriveMappingContext()
{
	APlayerController* OwnerPC = Cast<APlayerController>(GetOwner());
	if (!OwnerPC || !OwnerPC->GetLocalPlayer() || !DriveMappingContext)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(OwnerPC->GetLocalPlayer()))
	{
		if (Mode == ERobotBuilderMode::Drive)
		{
			Subsystem->AddMappingContext(DriveMappingContext, 1);
		}
		else
		{
			Subsystem->RemoveMappingContext(DriveMappingContext);
		}
	}
}

void URobotBuilderComponent::SetDriveTarget(ARobot* NewTarget)
{
	if (DriveTarget.Get() == NewTarget)
	{
		return;
	}

	if (ARobot* OldTarget = DriveTarget.Get())
	{
		OldTarget->SetDriveInput(FVector2D::ZeroVector);
		OldTarget->SetThrustInput(0.0f);
		OldTarget->OnRobotDestroyed.RemoveAll(this);
	}

	DriveTarget = NewTarget;

	if (NewTarget)
	{
		NewTarget->OnRobotDestroyed.AddUniqueDynamic(this, &URobotBuilderComponent::OnRobotDestroyed);
	}
}

void URobotBuilderComponent::OnRobotDestroyed(ARobot* DestroyedRobot)
{
	if (DriveTarget.Get() == DestroyedRobot)
	{
		DriveTarget = nullptr;
	}
}
