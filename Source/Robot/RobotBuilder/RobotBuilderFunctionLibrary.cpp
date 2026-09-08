// Copyright Epic Games, Inc. All Rights Reserved.

#include "RobotBuilderFunctionLibrary.h"
#include "RobotPartDefinition.h"
#include "RobotTypes.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "UObject/Package.h"

bool URobotBuilderFunctionLibrary::MapKeyByName(UInputMappingContext* Context, UInputAction* Action, const FName& KeyName, bool bNegate)
{
	if (!Context || !Action || KeyName.IsNone())
	{
		return false;
	}

	const FKey Key(KeyName);
	if (!Key.IsValid())
	{
		return false;
	}

	FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);

	if (bNegate)
	{
		Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(Context));
	}

	return true;
}

namespace
{
	constexpr EObjectFlags AssetFlags = RF_Public | RF_Standalone | RF_Transactional;

	UInputAction* CreateInputAction(const FString& AssetName)
	{
		const FString PackagePath = FString::Printf(TEXT("/Game/Input/Actions/%s"), *AssetName);
		UPackage* Package = CreatePackage(*PackagePath);
		if (!Package)
		{
			return nullptr;
		}

		UInputAction* Action = FindObject<UInputAction>(Package, *AssetName);
		if (!Action)
		{
			Action = NewObject<UInputAction>(Package, *AssetName, AssetFlags);
			FAssetRegistryModule::AssetCreated(Action);
		}
		Action->MarkPackageDirty();
		return Action;
	}

	UInputMappingContext* CreateMappingContext(const FString& AssetName)
	{
		const FString PackagePath = FString::Printf(TEXT("/Game/Input/%s"), *AssetName);
		UPackage* Package = CreatePackage(*PackagePath);
		if (!Package)
		{
			return nullptr;
		}

		UInputMappingContext* Context = FindObject<UInputMappingContext>(Package, *AssetName);
		if (!Context)
		{
			Context = NewObject<UInputMappingContext>(Package, *AssetName, AssetFlags);
			FAssetRegistryModule::AssetCreated(Context);
		}
		Context->UnmapAll();
		Context->MarkPackageDirty();
		return Context;
	}

	URobotPartDefinition* CreatePartDefinition(const FString& AssetName)
	{
		const FString PackagePath = FString::Printf(TEXT("/Game/RobotParts/%s"), *AssetName);
		UPackage* Package = CreatePackage(*PackagePath);
		if (!Package)
		{
			return nullptr;
		}

		URobotPartDefinition* Definition = FindObject<URobotPartDefinition>(Package, *AssetName);
		if (!Definition)
		{
			Definition = NewObject<URobotPartDefinition>(Package, *AssetName, AssetFlags);
			FAssetRegistryModule::AssetCreated(Definition);
		}
		Definition->MarkPackageDirty();
		return Definition;
	}

	UStaticMesh* LoadBasicShape(const TCHAR* ShapeName)
	{
		const FString Path = FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), ShapeName, ShapeName);
		return LoadObject<UStaticMesh>(nullptr, *Path);
	}
}

int32 URobotBuilderFunctionLibrary::CreateBuilderAssets()
{
	int32 CreatedCount = 0;

	// ---- input actions ----
	UInputAction* ClickAction = CreateInputAction(TEXT("IA_RobotClick"));
	UInputAction* AttackAction = CreateInputAction(TEXT("IA_RobotAttack"));
	UInputAction* CycleModeAction = CreateInputAction(TEXT("IA_RobotCycleMode"));
	UInputAction* RotateYawAction = CreateInputAction(TEXT("IA_RobotRotateYaw"));
	UInputAction* RotatePitchAction = CreateInputAction(TEXT("IA_RobotRotatePitch"));
	UInputAction* DriveMoveAction = CreateInputAction(TEXT("IA_RobotDriveMove"));
	UInputAction* ThrustAction = CreateInputAction(TEXT("IA_RobotThrust"));

	if (ClickAction) { ClickAction->ValueType = EInputActionValueType::Boolean; ++CreatedCount; }
	if (AttackAction) { AttackAction->ValueType = EInputActionValueType::Boolean; ++CreatedCount; }
	if (CycleModeAction) { CycleModeAction->ValueType = EInputActionValueType::Boolean; ++CreatedCount; }
	if (RotateYawAction) { RotateYawAction->ValueType = EInputActionValueType::Axis1D; ++CreatedCount; }
	if (RotatePitchAction) { RotatePitchAction->ValueType = EInputActionValueType::Boolean; ++CreatedCount; }
	if (DriveMoveAction) { DriveMoveAction->ValueType = EInputActionValueType::Axis2D; ++CreatedCount; }
	if (ThrustAction) { ThrustAction->ValueType = EInputActionValueType::Boolean; ++CreatedCount; }

	// ---- builder mapping context ----
	if (UInputMappingContext* BuilderContext = CreateMappingContext(TEXT("IMC_Builder")))
	{
		++CreatedCount;
		MapKeyByName(BuilderContext, ClickAction, TEXT("LeftMouseButton"), false);
		MapKeyByName(BuilderContext, AttackAction, TEXT("RightMouseButton"), false);
		MapKeyByName(BuilderContext, CycleModeAction, TEXT("Tab"), false);
		MapKeyByName(BuilderContext, RotateYawAction, TEXT("MouseWheelAxis"), false);
		MapKeyByName(BuilderContext, RotatePitchAction, TEXT("F"), false);
	}

	// ---- drive mapping context ----
	if (UInputMappingContext* DriveContext = CreateMappingContext(TEXT("IMC_Drive")))
	{
		++CreatedCount;
		MapKeyByName(DriveContext, DriveMoveAction, TEXT("W"), false);
		MapKeyByName(DriveContext, DriveMoveAction, TEXT("S"), true);
		MapKeyByName(DriveContext, DriveMoveAction, TEXT("A"), true);
		MapKeyByName(DriveContext, DriveMoveAction, TEXT("D"), false);
		MapKeyByName(DriveContext, ThrustAction, TEXT("SpaceBar"), false);
	}

	// ---- part definitions ----
	if (URobotPartDefinition* CoreDefinition = CreatePartDefinition(TEXT("DA_RobotCore")))
	{
		++CreatedCount;
		CoreDefinition->PartName = FText::FromString(TEXT("Core"));
		CoreDefinition->Category = ERobotPartCategory::Core;
		CoreDefinition->Mesh = LoadBasicShape(TEXT("SM_Cube"));
		CoreDefinition->Mass = 150.0f;
		CoreDefinition->MaxHP = 300.0f;
		CoreDefinition->MaxJointHP = 200.0f;
	}

	if (URobotPartDefinition* WheelDefinition = CreatePartDefinition(TEXT("DA_RobotWheel")))
	{
		++CreatedCount;
		WheelDefinition->PartName = FText::FromString(TEXT("Wheel"));
		WheelDefinition->Category = ERobotPartCategory::Wheel;
		WheelDefinition->Mesh = LoadBasicShape(TEXT("SM_Sphere"));
		WheelDefinition->Mass = 25.0f;
		WheelDefinition->MaxHP = 80.0f;
		WheelDefinition->MaxJointHP = 80.0f;
		WheelDefinition->WheelTorque = 400000.0f;
		WheelDefinition->MaxWheelSpinRate = 40.0f;
		WheelDefinition->WheelBrakeFactor = 0.35f;
	}

	if (URobotPartDefinition* ThrusterDefinition = CreatePartDefinition(TEXT("DA_RobotThruster")))
	{
		++CreatedCount;
		ThrusterDefinition->PartName = FText::FromString(TEXT("Thruster"));
		ThrusterDefinition->Category = ERobotPartCategory::Thruster;
		ThrusterDefinition->Mesh = LoadBasicShape(TEXT("SM_Cylinder"));
		ThrusterDefinition->Mass = 20.0f;
		ThrusterDefinition->MaxHP = 60.0f;
		ThrusterDefinition->MaxJointHP = 60.0f;
		ThrusterDefinition->ThrustForce = 80000.0f;
	}

	if (URobotPartDefinition* ArmorDefinition = CreatePartDefinition(TEXT("DA_RobotArmor")))
	{
		++CreatedCount;
		ArmorDefinition->PartName = FText::FromString(TEXT("Armor"));
		ArmorDefinition->Category = ERobotPartCategory::Armor;
		ArmorDefinition->Mesh = LoadBasicShape(TEXT("SM_Cube"));
		ArmorDefinition->Mass = 80.0f;
		ArmorDefinition->MaxHP = 400.0f;
		ArmorDefinition->MaxJointHP = 120.0f;
	}

	UE_LOG(LogTemp, Log, TEXT("RobotBuilder: created or updated %d assets."), CreatedCount);
	return CreatedCount;
}
