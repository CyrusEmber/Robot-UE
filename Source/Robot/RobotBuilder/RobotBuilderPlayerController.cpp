// Copyright Epic Games, Inc. All Rights Reserved.

#include "RobotBuilderPlayerController.h"
#include "RobotBuilderComponent.h"
#include "RobotPartDefinition.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "UObject/ConstructorHelpers.h"

ARobotBuilderPlayerController::ARobotBuilderPlayerController()
{
	BuilderComponent = CreateDefaultSubobject<URobotBuilderComponent>(TEXT("BuilderComponent"));

	// make the template movement context active for this controller too
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> DefaultContextFinder(TEXT("/Game/Input/IMC_Default.IMC_Default"));
	if (DefaultContextFinder.Succeeded())
	{
		DefaultMappingContexts.Add(DefaultContextFinder.Object);
	}

	// auto-wire builder assets when they exist at the documented paths,
	// so no manual assignment is needed after creating them
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> BuilderContextFinder(TEXT("/Game/Input/IMC_Builder.IMC_Builder"));
	if (BuilderContextFinder.Succeeded())
	{
		DefaultMappingContexts.Add(BuilderContextFinder.Object);
	}

	if (BuilderComponent)
	{
		static ConstructorHelpers::FObjectFinder<UInputMappingContext> DriveContextFinder(TEXT("/Game/Input/IMC_Drive.IMC_Drive"));
		if (DriveContextFinder.Succeeded())
		{
			BuilderComponent->SetDriveMappingContext(DriveContextFinder.Object);
		}

		// auto-wire the default part definitions created by the setup script
		TArray<TObjectPtr<URobotPartDefinition>> DefaultParts;

		static ConstructorHelpers::FObjectFinder<URobotPartDefinition> CoreFinder(TEXT("/Game/RobotParts/DA_RobotCore.DA_RobotCore"));
		if (CoreFinder.Succeeded())
		{
			DefaultParts.Add(CoreFinder.Object);
		}

		static ConstructorHelpers::FObjectFinder<URobotPartDefinition> WheelFinder(TEXT("/Game/RobotParts/DA_RobotWheel.DA_RobotWheel"));
		if (WheelFinder.Succeeded())
		{
			DefaultParts.Add(WheelFinder.Object);
		}

		static ConstructorHelpers::FObjectFinder<URobotPartDefinition> ThrusterFinder(TEXT("/Game/RobotParts/DA_RobotThruster.DA_RobotThruster"));
		if (ThrusterFinder.Succeeded())
		{
			DefaultParts.Add(ThrusterFinder.Object);
		}

		static ConstructorHelpers::FObjectFinder<URobotPartDefinition> ArmorFinder(TEXT("/Game/RobotParts/DA_RobotArmor.DA_RobotArmor"));
		if (ArmorFinder.Succeeded())
		{
			DefaultParts.Add(ArmorFinder.Object);
		}

		if (DefaultParts.Num() > 0)
		{
			BuilderComponent->SetPartDefinitions(DefaultParts);
		}
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> ClickActionFinder(TEXT("/Game/Input/Actions/IA_RobotClick.IA_RobotClick"));
	if (ClickActionFinder.Succeeded())
	{
		RobotClickAction = ClickActionFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> AttackActionFinder(TEXT("/Game/Input/Actions/IA_RobotAttack.IA_RobotAttack"));
	if (AttackActionFinder.Succeeded())
	{
		RobotAttackAction = AttackActionFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> CycleModeActionFinder(TEXT("/Game/Input/Actions/IA_RobotCycleMode.IA_RobotCycleMode"));
	if (CycleModeActionFinder.Succeeded())
	{
		RobotCycleModeAction = CycleModeActionFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> RotateYawActionFinder(TEXT("/Game/Input/Actions/IA_RobotRotateYaw.IA_RobotRotateYaw"));
	if (RotateYawActionFinder.Succeeded())
	{
		RobotRotateYawAction = RotateYawActionFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> RotatePitchActionFinder(TEXT("/Game/Input/Actions/IA_RobotRotatePitch.IA_RobotRotatePitch"));
	if (RotatePitchActionFinder.Succeeded())
	{
		RobotRotatePitchAction = RotatePitchActionFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> DriveMoveActionFinder(TEXT("/Game/Input/Actions/IA_RobotDriveMove.IA_RobotDriveMove"));
	if (DriveMoveActionFinder.Succeeded())
	{
		RobotDriveMoveAction = DriveMoveActionFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> ThrustActionFinder(TEXT("/Game/Input/Actions/IA_RobotThrust.IA_RobotThrust"));
	if (ThrustActionFinder.Succeeded())
	{
		RobotThrustAction = ThrustActionFinder.Object;
	}
}

void ARobotBuilderPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInputComponent || !BuilderComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("RobotBuilderPlayerController requires an Enhanced Input component."));
		return;
	}

	URobotBuilderComponent* Builder = BuilderComponent.Get();

	if (RobotClickAction)
	{
		EnhancedInputComponent->BindAction(RobotClickAction, ETriggerEvent::Started, Builder, &URobotBuilderComponent::HandleClick);
	}

	if (RobotAttackAction)
	{
		EnhancedInputComponent->BindAction(RobotAttackAction, ETriggerEvent::Started, Builder, &URobotBuilderComponent::HandleAttack);
	}

	if (RobotCycleModeAction)
	{
		EnhancedInputComponent->BindAction(RobotCycleModeAction, ETriggerEvent::Started, Builder, &URobotBuilderComponent::CycleMode);
	}

	if (RobotRotateYawAction)
	{
		EnhancedInputComponent->BindAction(RobotRotateYawAction, ETriggerEvent::Triggered, Builder, &URobotBuilderComponent::HandleRotateYaw);
	}

	if (RobotRotatePitchAction)
	{
		EnhancedInputComponent->BindAction(RobotRotatePitchAction, ETriggerEvent::Started, Builder, &URobotBuilderComponent::HandleRotatePitch);
	}

	if (RobotDriveMoveAction)
	{
		EnhancedInputComponent->BindAction(RobotDriveMoveAction, ETriggerEvent::Triggered, Builder, &URobotBuilderComponent::HandleDriveMove);
	}

	if (RobotThrustAction)
	{
		EnhancedInputComponent->BindAction(RobotThrustAction, ETriggerEvent::Started, Builder, &URobotBuilderComponent::HandleThrustStart);
		EnhancedInputComponent->BindAction(RobotThrustAction, ETriggerEvent::Completed, Builder, &URobotBuilderComponent::HandleThrustStop);
	}
}
