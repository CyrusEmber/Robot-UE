// Copyright Epic Games, Inc. All Rights Reserved.

#include "RobotBuilderWidget.h"
#include "RobotBuilderComponent.h"
#include "RobotAssembly.h"
#include "RobotPartDefinition.h"
#include "RobotTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "GameFramework/PlayerController.h"

void URobotButtonHandler::HandleClicked()
{
	URobotBuilderComponent* BuilderPtr = Builder.Get();
	if (!BuilderPtr)
	{
		return;
	}

	if (PartIndex != INDEX_NONE)
	{
		BuilderPtr->SelectPartDefinition(PartIndex);
	}
	else
	{
		BuilderPtr->SetMode(TargetMode);
	}
}

URobotBuilderWidget::URobotBuilderWidget()
{
	bIsFocusable = false;
}

URobotBuilderComponent* URobotBuilderWidget::GetBuilder() const
{
	const APlayerController* OwningPC = GetOwningPlayer();
	return OwningPC ? OwningPC->FindComponentByClass<URobotBuilderComponent>() : nullptr;
}

void URobotBuilderWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BuildLayout();
}

void URobotBuilderWidget::BuildLayout()
{
	if (bLayoutBuilt || !WidgetTree)
	{
		return;
	}
	bLayoutBuilt = true;

	URobotBuilderComponent* BuilderPtr = GetBuilder();
	if (!BuilderPtr)
	{
		return;
	}

	// root canvas anchored to the left edge, vertically centered
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelBorder"));
	PanelBorder->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.45f));
	PanelBorder->SetPadding(FMargin(8.0f));

	RootCanvas->AddChildToCanvas(PanelBorder);
	if (UCanvasPanelSlot* BorderSlot = Cast<UCanvasPanelSlot>(PanelBorder->Slot))
	{
		BorderSlot->SetAnchors(FAnchors(0.0f, 0.5f));
		BorderSlot->SetPosition(FVector2D(20.0f, -180.0f));
		BorderSlot->SetSize(FVector2D(280.0f, 360.0f));
	}

	UVerticalBox* ContentBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ContentBox"));
	PanelBorder->SetContent(ContentBox);

	// title
	UTextBlock* TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetText(FText::FromString(TEXT("ROBOT BUILDER")));
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor::White)));
	ContentBox->AddChildToVerticalBox(TitleText);

	// one button per part definition
	const TArray<TObjectPtr<URobotPartDefinition>>& Definitions = BuilderPtr->GetPartDefinitions();
	if (Definitions.Num() == 0)
	{
		UTextBlock* EmptyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EmptyText"));
		EmptyText->SetText(FText::FromString(TEXT("No part definitions assigned")));
		ContentBox->AddChildToVerticalBox(EmptyText);
	}

	for (int32 DefinitionIndex = 0; DefinitionIndex < Definitions.Num(); ++DefinitionIndex)
	{
		const URobotPartDefinition* Definition = Definitions[DefinitionIndex];
		if (!Definition)
		{
			continue;
		}

		FString Label = Definition->PartName.IsEmpty()
			? FString::Printf(TEXT("Part %d"), DefinitionIndex + 1)
			: Definition->PartName.ToString();
		Label += FString::Printf(TEXT("  [%s]"), *UEnum::GetDisplayValueAsText(Definition->Category).ToString());

		UButton* PartButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), FName(*FString::Printf(TEXT("PartButton_%d"), DefinitionIndex)));

		UTextBlock* PartLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PartLabel"));
		PartLabel->SetText(FText::FromString(Label));
		PartLabel->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
		PartButton->AddChild(PartLabel);

		if (UVerticalBoxSlot* ButtonSlot = ContentBox->AddChildToVerticalBox(PartButton))
		{
			ButtonSlot->SetPadding(FMargin(0.0f, 2.0f));
		}

		URobotButtonHandler* Handler = NewObject<URobotButtonHandler>(this);
		Handler->PartIndex = DefinitionIndex;
		Handler->Builder = BuilderPtr;
		PartButton->OnClicked.AddDynamic(Handler, &URobotButtonHandler::HandleClicked);

		PartButtons.Add(PartButton);
	}

	// mode buttons
	static const TCHAR* ModeLabels[] = { TEXT("Build"), TEXT("Demolish"), TEXT("Drive") };
	static const ERobotBuilderMode ModeValues[] = { ERobotBuilderMode::Build, ERobotBuilderMode::Demolish, ERobotBuilderMode::Drive };

	for (int32 ModeIndex = 0; ModeIndex < 3; ++ModeIndex)
	{
		UButton* ModeButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), FName(*FString::Printf(TEXT("ModeButton_%d"), ModeIndex)));

		UTextBlock* ModeLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ModeLabel"));
		ModeLabel->SetText(FText::FromString(ModeLabels[ModeIndex]));
		ModeLabel->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
		ModeButton->AddChild(ModeLabel);

		if (UVerticalBoxSlot* ButtonSlot = ContentBox->AddChildToVerticalBox(ModeButton))
		{
			ButtonSlot->SetPadding(FMargin(0.0f, 2.0f));
		}

		URobotButtonHandler* Handler = NewObject<URobotButtonHandler>(this);
		Handler->PartIndex = INDEX_NONE;
		Handler->TargetMode = ModeValues[ModeIndex];
		Handler->Builder = BuilderPtr;
		ModeButton->OnClicked.AddDynamic(Handler, &URobotButtonHandler::HandleClicked);

		ModeButtons.Add(ModeButton);
	}

	// status line
	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
	StatusText->SetText(FText::FromString(TEXT("Mode: Build")));
	StatusText->SetColorAndOpacity(FSlateColor(FLinearColor::White)));
	if (UVerticalBoxSlot* StatusSlot = ContentBox->AddChildToVerticalBox(StatusText))
	{
		StatusSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
	}
}

void URobotBuilderWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	URobotBuilderComponent* BuilderPtr = GetBuilder();
	if (!BuilderPtr || !StatusText)
	{
		return;
	}

	FString StatusLine;

	switch (BuilderPtr->GetMode())
	{
	case ERobotBuilderMode::Build:
		StatusLine = TEXT("Mode: Build (click ground: new robot / click part: attach)");
		break;
	case ERobotBuilderMode::Demolish:
		StatusLine = TEXT("Mode: Demolish (click part: remove)");
		break;
	case ERobotBuilderMode::Drive:
		StatusLine = TEXT("Mode: Drive (click robot: take control, WASD + Space)");
		break;
	}

	if (ARobot* DriveTarget = BuilderPtr->GetDriveTarget())
	{
		StatusLine += FString::Printf(TEXT(" | Robot: %d parts, %.0f kg"), DriveTarget->GetParts().Num(), DriveTarget->GetTotalMass());
	}

	StatusText->SetText(FText::FromString(StatusLine));
}
