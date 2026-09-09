// Copyright Epic Games, Inc. All Rights Reserved.

#include "RobotPart.h"
#include "RobotAssembly.h"
#include "RobotPartDefinition.h"
#include "RobotTypes.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "Engine/StaticMesh.h"

ARobotPart::ARobotPart()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	SetRootComponent(MeshComponent);

	MeshComponent->SetCollisionProfileName(TEXT("PhysicsActor"));
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComponent->SetSimulatePhysics(true);
	MeshComponent->SetNotifyRigidBodyCollision(true);
	MeshComponent->OnComponentHit.AddDynamic(this, &ARobotPart::OnHit);
}

void ARobotPart::BeginPlay()
{
	Super::BeginPlay();
}

void ARobotPart::InitFromDefinition(URobotPartDefinition* InDefinition)
{
	Definition = InDefinition;
	if (!Definition)
	{
		return;
	}

	if (Definition->Mesh)
	{
		MeshComponent->SetStaticMesh(Definition->Mesh);
	}

	if (Definition->Mass > 0.0f)
	{
		if (FBodyInstance* BodyInstance = MeshComponent->GetBodyInstance())
		{
			BodyInstance->SetMassOverride(Definition->Mass, true);
			BodyInstance->UpdateMassProperties();
		}
	}

	MaxHP = Definition->MaxHP;
	CurrentHP = MaxHP;
}

ERobotPartCategory ARobotPart::GetCategory() const
{
	return Definition ? Definition->Category : ERobotPartCategory::Armor;
}

float ARobotPart::TakeDamage(float Damage, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float ActualDamage = Super::TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);
	ApplyPartDamage(ActualDamage);
	return ActualDamage;
}

void ARobotPart::ApplyPartDamage(float Amount)
{
	if (bDead || Amount <= 0.0f)
	{
		return;
	}

	CurrentHP = FMath::Max(0.0f, CurrentHP - Amount);
	OnPartDamaged.Broadcast(this, CurrentHP);

	if (CurrentHP <= 0.0f)
	{
		bDead = true;
		OnPartDied.Broadcast(this);

		if (OwningRobot)
		{
			OwningRobot->NotifyPartDied(this);
		}
	}
}

void ARobotPart::MarkAsDebris()
{
	// dead parts keep their robot reference so the graph can clean up
	// their links when the debris is demolished; bDead gates damage reporting
	bDead = true;
}

void ARobotPart::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bDead || !OwningRobot || !Definition)
	{
		return;
	}

	// collisions between parts of the same robot never deal damage
	const ARobotPart* OtherPart = Cast<ARobotPart>(OtherActor);
	if (OtherPart && OtherPart->GetOwningRobot() == OwningRobot)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (World && (World->GetTimeSeconds() - LastImpactDamageTime) < ImpactDamageCooldown)
	{
		return;
	}

	const float Impulse = NormalImpulse.Size();
	if (Impulse < Definition->MinImpactImpulse)
	{
		return;
	}

	LastImpactDamageTime = World ? World->GetTimeSeconds() : 0.0f;

	if (Impulse >= Definition->BreakImpulse)
	{
		// heavy impact: knock the part right off the robot
		OwningRobot->BreakLink(this);
		return;
	}

	const float Damage = (Impulse - Definition->MinImpactImpulse) * Definition->ImpulseDamageScale;
	OwningRobot->ApplyImpactDamage(this, Damage);
}
