// Copyright Epic Games, Inc. All Rights Reserved.

#include "RobotAssembly.h"
#include "RobotPart.h"
#include "RobotPartDefinition.h"
#include "RobotTypes.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "TimerManager.h"

ARobot::ARobot()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void ARobot::BeginPlay()
{
	Super::BeginPlay();
}

void ARobot::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ApplyDriveForces();
}

void ARobot::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

ARobot* ARobot::SpawnRobot(UWorld* World, URobotPartDefinition* CoreDefinition, const FTransform& SpawnTransform)
{
	if (!World || !CoreDefinition || !CoreDefinition->IsCore())
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ARobot* NewRobot = World->SpawnActor<ARobot>(ARobot::StaticClass(), SpawnTransform, SpawnParams);
	if (!NewRobot)
	{
		return nullptr;
	}

	ARobotPart* Core = NewRobot->AddPart(CoreDefinition, nullptr, SpawnTransform, SpawnTransform.GetLocation());
	if (!Core)
	{
		NewRobot->Destroy();
		return nullptr;
	}

	NewRobot->CorePart = Core;
	return NewRobot;
}

ARobotPart* ARobot::AddPart(URobotPartDefinition* Definition, ARobotPart* ParentPart, const FTransform& PartTransform, const FVector& AnchorWorldLocation)
{
	if (!Definition || bDestroying)
	{
		return nullptr;
	}

	if (ParentPart && ParentPart->GetOwningRobot() != this)
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Owner = this;

	ARobotPart* NewPart = World->SpawnActor<ARobotPart>(ARobotPart::StaticClass(), PartTransform, SpawnParams);
	if (!NewPart)
	{
		return nullptr;
	}

	NewPart->InitFromDefinition(Definition);
	NewPart->SetOwningRobot(this);

	if (ParentPart)
	{
		UPhysicsConstraintComponent* Joint = CreateLinkConstraint(ParentPart, NewPart, AnchorWorldLocation);
		if (!Joint)
		{
			NewPart->Destroy();
			return nullptr;
		}

		NewPart->SetJoint(Joint);

		FRobotLink NewLink;
		NewLink.LinkId = FGuid::NewGuid();
		NewLink.Parent = ParentPart;
		NewLink.Child = NewPart;
		NewLink.Constraint = Joint;
		NewLink.MaxJointHP = Definition->MaxJointHP;
		NewLink.JointHP = Definition->MaxJointHP;
		Links.Add(NewLink);
	}

	Parts.Add(NewPart);
	OnPartAdded.Broadcast(NewPart);

	return NewPart;
}

UPhysicsConstraintComponent* ARobot::CreateLinkConstraint(ARobotPart* ParentPart, ARobotPart* ChildPart, const FVector& AnchorWorldLocation)
{
	if (!ParentPart || !ChildPart || !ParentPart->GetMeshComponent() || !ChildPart->GetMeshComponent())
	{
		return nullptr;
	}

	UPhysicsConstraintComponent* Joint = NewObject<UPhysicsConstraintComponent>(ChildPart);
	if (!Joint)
	{
		return nullptr;
	}

	Joint->SetWorldLocation(AnchorWorldLocation);

	// wheels spin around their local X axis, everything else is welded
	const bool bIsWheel = ChildPart->GetCategory() == ERobotPartCategory::Wheel;
	Joint->SetWorldRotation(bIsWheel ? ChildPart->GetActorRotation() : FRotator::ZeroRotator);

	Joint->RegisterComponent();
	Joint->SetConstrainedComponents(ParentPart->GetMeshComponent(), NAME_None, ChildPart->GetMeshComponent(), NAME_None);
	Joint->SetDisableCollision(true);

	// position is welded for every part
	Joint->SetLinearXLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
	Joint->SetLinearYLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);
	Joint->SetLinearZLimit(ELinearConstraintMotion::LCM_Locked, 0.0f);

	// wheels keep one free twist axis, everything else is fully locked
	Joint->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Locked, 0.0f);
	Joint->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Locked, 0.0f);
	Joint->SetAngularTwistLimit(bIsWheel ? EAngularConstraintMotion::ACM_Free : EAngularConstraintMotion::ACM_Locked, 0.0f);

	return Joint;
}

bool ARobot::RemovePart(ARobotPart* Part)
{
	if (!Part || bDestroying)
	{
		return false;
	}

	if (Part->GetOwningRobot() != this)
	{
		return false;
	}

	if (Part == CorePart)
	{
		DestroyRobot();
		return true;
	}

	// every part hanging off this one loses support and drops
	DestroyAllLinksForPart(Part);
	Parts.RemoveSingle(Part);
	Part->Destroy();

	RunConnectivityPass();
	return true;
}

void ARobot::ApplyImpactDamage(ARobotPart* Part, float Amount)
{
	if (!Part || Amount <= 0.0f || Part->GetOwningRobot() != this)
	{
		return;
	}

	// damage the connection first: a broken joint shakes the part loose undamaged
	for (FRobotLink& Link : Links)
	{
		if (Link.Child.Get() == Part)
		{
			Link.JointHP = FMath::Max(0.0f, Link.JointHP - Amount);
			if (Link.JointHP <= 0.0f)
			{
				BreakLink(Part);
				return;
			}
			break;
		}
	}

	// then the part body itself
	Part->ApplyPartDamage(Amount);
	OnPartDamaged.Broadcast(Part, Part->GetCurrentHP());
}

void ARobot::ApplyAttackDamage(ARobotPart* Part)
{
	if (!Part || !Part->GetDefinition())
	{
		return;
	}

	ApplyImpactDamage(Part, Part->GetDefinition()->AttackDamage);
}

void ARobot::BreakLink(ARobotPart* ChildPart)
{
	if (!ChildPart || ChildPart->GetOwningRobot() != this)
	{
		return;
	}

	DestroyLinkForChild(ChildPart);
	RunConnectivityPass();
}

void ARobot::NotifyPartDied(ARobotPart* Part)
{
	if (!Part || bDestroying)
	{
		return;
	}

	if (Part == CorePart)
	{
		DestroyRobot();
		return;
	}

	// a dead part can no longer support anything: drop everything it held
	DestroyAllLinksForPart(Part);
	RunConnectivityPass();
}

TMap<ARobotPart*, TArray<ARobotPart*>> ARobot::BuildAdjacency() const
{
	TMap<ARobotPart*, TArray<ARobotPart*>> Adjacency;

	for (const FRobotLink& Link : Links)
	{
		ARobotPart* Parent = Link.Parent.Get();
		ARobotPart* Child = Link.Child.Get();
		if (!Parent || !Child)
		{
			continue;
		}

		Adjacency.FindOrAdd(Parent).Add(Child);
		Adjacency.FindOrAdd(Child).Add(Parent);
	}

	return Adjacency;
}

void ARobot::RunConnectivityPass()
{
	if (bDestroying || !CorePart)
	{
		return;
	}

	// breadth first search from the core across the link graph
	TMap<ARobotPart*, TArray<ARobotPart*>> Adjacency = BuildAdjacency();

	TSet<ARobotPart*> Reached;
	TArray<ARobotPart*> Queue;
	Queue.Push(CorePart);
	Reached.Add(CorePart);

	while (Queue.Num() > 0)
	{
		ARobotPart* Current = Queue.Pop();

		if (const TArray<ARobotPart*>* Neighbors = Adjacency.Find(Current))
		{
			for (ARobotPart* Neighbor : *Neighbors)
			{
				if (Neighbor && !Reached.Contains(Neighbor))
				{
					Reached.Add(Neighbor);
					Queue.Push(Neighbor);
				}
			}
		}
	}

	// sever only the edges crossing the cut so the detached subtree keeps
	// its internal constraints and falls off as one coherent chunk
	TArray<FGuid> BoundaryLinkIds;
	for (const FRobotLink& Link : Links)
	{
		ARobotPart* Parent = Link.Parent.Get();
		ARobotPart* Child = Link.Child.Get();
		if (!Parent || !Child)
		{
			continue;
		}

		if (Reached.Contains(Parent) != Reached.Contains(Child))
		{
			BoundaryLinkIds.Add(Link.LinkId);
		}
	}

	for (const FGuid& LinkId : BoundaryLinkIds)
	{
		for (int32 LinkIndex = 0; LinkIndex < Links.Num(); ++LinkIndex)
		{
			if (Links[LinkIndex].LinkId != LinkId)
			{
				continue;
			}

			if (UPhysicsConstraintComponent* Constraint = Links[LinkIndex].Constraint.Get())
			{
				Constraint->BreakConstraint();
				Constraint->DestroyComponent();
			}

			if (ARobotPart* Child = Links[LinkIndex].Child.Get())
			{
				Child->SetJoint(nullptr);
			}

			Links.RemoveAt(LinkIndex);
			break;
		}
	}

	// unregister the detached parts as debris, keeping their remaining links
	TArray<ARobotPart*> PartsToDetach;
	for (const TObjectPtr<ARobotPart>& Part : Parts)
	{
		if (Part && !Reached.Contains(Part))
		{
			PartsToDetach.Add(Part.Get());
		}
	}

	for (ARobotPart* Part : PartsToDetach)
	{
		DetachPart(Part, false);
	}
}

void ARobot::DetachPart(ARobotPart* Part, bool bSeverParentLink)
{
	if (!Part)
	{
		return;
	}

	// sever the edge to the parent for single-part detaches;
	// chunk detaches keep internal links so the subtree stays whole
	if (bSeverParentLink)
	{
		DestroyLinkForChild(Part);
	}

	// detach only once: later connectivity passes must not re-emit debris
	if (Parts.RemoveSingle(Part) == 0)
	{
		return;
	}

	Part->MarkAsDebris();
	OnPartDetached.Broadcast(Part);

	if (DebrisLifetime > 0.0f && GetWorld())
	{
		FTimerHandle DebrisTimer;
		FTimerDelegate DebrisDelegate;
		DebrisDelegate.BindWeakLambda(Part, [Part]()
		{
			if (IsValid(Part))
			{
				if (ARobot* Robot = Part->GetOwningRobot())
				{
					// route through the robot so chunk links are severed properly
					Robot->RemovePart(Part);
				}
				else
				{
					Part->Destroy();
				}
			}
		});
		GetWorld()->GetTimerManager().SetTimer(DebrisTimer, DebrisDelegate, DebrisLifetime, false);
	}
}

void ARobot::DestroyLinkForChild(ARobotPart* ChildPart)
{
	for (int32 LinkIndex = 0; LinkIndex < Links.Num(); ++LinkIndex)
	{
		if (Links[LinkIndex].Child.Get() != ChildPart)
		{
			continue;
		}

		if (UPhysicsConstraintComponent* Constraint = Links[LinkIndex].Constraint.Get())
		{
			Constraint->BreakConstraint();
			Constraint->DestroyComponent();
		}

		if (ARobotPart* Child = Links[LinkIndex].Child.Get())
		{
			Child->SetJoint(nullptr);
		}

		Links.RemoveAt(LinkIndex);
		return;
	}
}

void ARobot::DestroyAllLinksForPart(ARobotPart* Part)
{
	for (int32 LinkIndex = Links.Num() - 1; LinkIndex >= 0; --LinkIndex)
	{
		const bool bIsChild = Links[LinkIndex].Child.Get() == Part;
		const bool bIsParent = Links[LinkIndex].Parent.Get() == Part;
		if (!bIsChild && !bIsParent)
		{
			continue;
		}

		if (UPhysicsConstraintComponent* Constraint = Links[LinkIndex].Constraint.Get())
		{
			Constraint->BreakConstraint();
			Constraint->DestroyComponent();
		}

		if (ARobotPart* Child = Links[LinkIndex].Child.Get())
		{
			Child->SetJoint(nullptr);
		}

		Links.RemoveAt(LinkIndex);
	}
}

float ARobot::GetTotalMass() const
{
	float TotalMass = 0.0f;

	for (const TObjectPtr<ARobotPart>& Part : Parts)
	{
		if (Part && Part->GetMeshComponent())
		{
			TotalMass += Part->GetMeshComponent()->GetMass();
		}
	}

	return TotalMass;
}

FVector ARobot::GetCenterOfMassWorld() const
{
	float TotalMass = 0.0f;
	FVector WeightedSum = FVector::ZeroVector;

	for (const TObjectPtr<ARobotPart>& Part : Parts)
	{
		if (Part && Part->GetMeshComponent())
		{
			const float PartMass = Part->GetMeshComponent()->GetMass();
			WeightedSum += Part->GetMeshComponent()->GetCenterOfMass() * PartMass;
			TotalMass += PartMass;
		}
	}

	return TotalMass > 0.0f ? WeightedSum / TotalMass : GetActorLocation();
}

void ARobot::ApplyDriveForces()
{
	if (bDestroying || !CorePart)
	{
		return;
	}

	const FVector CoreRight = CorePart->GetActorRightVector();
	const FVector RobotCenterOfMass = GetCenterOfMassWorld();

	for (const TObjectPtr<ARobotPart>& Part : Parts)
	{
		const URobotPartDefinition* Definition = Part ? Part->GetDefinition() : nullptr;
		UStaticMeshComponent* Mesh = Part ? Part->GetMeshComponent() : nullptr;
		if (!Definition || !Mesh)
		{
			continue;
		}

		if (Definition->IsWheel() && Definition->WheelTorque > 0.0f)
		{
			const FVector AxleDirection = Part->GetActorForwardVector();

			// tank steering: wheels on opposite sides of the core spin opposite ways
			const float SideSign = FVector::DotProduct(Part->GetActorLocation() - RobotCenterOfMass, CoreRight) >= 0.0f ? 1.0f : -1.0f;
			const float Drive = DriveInput.X + DriveInput.Y * SideSign;

			const float Spin = FVector::DotProduct(Mesh->GetPhysicsAngularVelocityInRadians(), AxleDirection);

			if (FMath::Abs(Drive) > KINDA_SMALL_NUMBER)
			{
				if (FMath::Abs(Spin) < Definition->MaxWheelSpinRate)
				{
					Mesh->AddTorqueInRadians(AxleDirection * Definition->WheelTorque * Drive, NAME_None, true);
				}
			}
			else if (FMath::Abs(Spin) > 0.05f)
			{
				// brake: counter-torque proportional to the current spin,
				// capped at WheelBrakeFactor times the drive torque
				const float BrakeGain = Definition->WheelTorque * Definition->WheelBrakeFactor / FMath::Max(Definition->MaxWheelSpinRate, 1.0f);
				Mesh->AddTorqueInRadians(-AxleDirection * Spin * BrakeGain, NAME_None, true);
			}
		}
		else if (Definition->IsThruster() && ThrustInput > 0.0f && Definition->ThrustForce > 0.0f)
		{
			const FVector ThrustDirection = Part->GetActorForwardVector();
			Mesh->AddForceAtLocation(ThrustDirection * Definition->ThrustForce * ThrustInput, Mesh->GetComponentLocation());
		}
	}
}

void ARobot::DestroyRobot()
{
	if (bDestroying)
	{
		return;
	}
	bDestroying = true;

	// turn every part into debris first so nothing reports damage anymore
	TArray<ARobotPart*> AllParts;
	for (const TObjectPtr<ARobotPart>& Part : Parts)
	{
		if (Part)
		{
			AllParts.Add(Part.Get());
		}
	}

	for (ARobotPart* Part : AllParts)
	{
		if (Part)
		{
			Part->MarkAsDebris();
		}
	}
	Parts.Reset();

	// then snap every connection
	for (const FRobotLink& Link : Links)
	{
		if (UPhysicsConstraintComponent* Constraint = Link.Constraint.Get())
		{
			Constraint->BreakConstraint();
			Constraint->DestroyComponent();
		}

		if (ARobotPart* Child = Link.Child.Get())
		{
			Child->SetJoint(nullptr);
		}
	}
	Links.Reset();

	OnRobotDestroyed.Broadcast(this);

	Destroy();
}
