// Copyright Game Production AI Assistant contributors 2026.
#include "UI/SGPAAConversationView.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SGPAAConversationView::Construct(const FArguments &InArgs)
{
    ChildSlot[SNew(SBorder).Padding(
        8)[SAssignNew(Scroll, SScrollBox) + SScrollBox::Slot()[SNew(SBox)
                                                                   .VAlign(VAlign_Bottom)
                                                                   .MinDesiredHeight_Lambda(
                                                                       [this]() {
                                                                           return Scroll->GetCachedGeometry()
                                                                               .GetLocalSize()
                                                                               .Y;
                                                                       })[SAssignNew(Messages, SVerticalBox)]]]];
}

void SGPAAConversationView::BeginTurn(const FString &Prompt)
{
    Messages->AddSlot()
        .AutoHeight()
        .HAlign(HAlign_Right)
        .Padding(0, 0, 0, 10)[SNew(SBox).MaxDesiredWidth_Lambda(
            [this]()
            {
                const float Width = Scroll->GetCachedGeometry().GetLocalSize().X;
                return Width > 0 ? Width * .8f : 600.f;
            })[SNew(SBorder)
                   .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                   .BorderBackgroundColor(FLinearColor(.06f, .18f, .28f))
                   .Padding(12)[SNew(STextBlock)
                                    .Text(FText::FromString(Prompt))
                                    .AutoWrapText(true)
                                    .Justification(ETextJustify::Right)
                                    .ColorAndOpacity(FLinearColor(.65f, .85f, 1.f))]]];
    AnswerTextBox.Reset();
    bActiveTurn = true;
    UpdateAnswer(FString());
}

void SGPAAConversationView::UpdateAnswer(const FString &Text)
{
    if (!AnswerTextBox)
        Messages->AddSlot().AutoHeight().Padding(
            0, 0, 0, 14)[SAssignNew(AnswerTextBox, SMultiLineEditableTextBox).IsReadOnly(true).AutoWrapText(true)];
    AnswerTextBox->SetText(FText::FromString(Text));
    Scroll->ScrollToEnd();
}

void SGPAAConversationView::FinishTurn(const FString &Text)
{
    UpdateAnswer(Text);
    bActiveTurn = false;
}
