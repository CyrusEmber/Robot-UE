// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RobotBuilderComponent.h"
#include "RobotBuilderWidget.generated.h"

class UBorder;
class UButton;
class URobotBuilderComponent;
class UTextBlock;
class UVerticalBox;

/**
 *  Small helper object that knows which builder action a UButton click
 *  belongs to, since OnClicked carries no payload.
 */
UCLASS()
class URobotButtonHandler : public UObject
{
	GENERATED_BODY()

public:
	/** Handler for button clicks routed to the builder component */
	UFUNCTION()
	void HandleClicked();

	/** Index into the builder part list. INDEX_NONE for mode buttons. */
	int32 PartIndex = INDEX_NONE;

	/** Mode to switch to for mode buttons */
	ERobotBuilderMode TargetMode = ERobotBuilderMode::Build;

	UPROPERTY()
	TWeakObjectPtr<URobotBuilderComponent> Builder;
};

/**
 *  Builder HUD built entirely in C++: one button per part definition,
 *  three mode buttons and a status line.
 */
UCLASS()
class URobotBuilderWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Constructor */
	URobotBuilderWidget();

protected:
	/** Builds the whole layout once */
	virtual void NativeConstruct() override;

	/** Refreshes the status line */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Returns the builder component of the owning player */
	URobotBuilderComponent* GetBuilder() const;

	/** Creates buttons and panels from the builder component state */
	void BuildLayout();

protected:
	/** Outer border of the panel */
	UPROPERTY(Transient)
	TObjectPtr<UBorder> PanelBorder;

	/** One button per part definition, same order as the definitions */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> PartButtons;

	/** Build, demolish and drive buttons */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> ModeButtons;

	/** Bottom status line */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

private:
	bool bLayoutBuilt = false;
};
