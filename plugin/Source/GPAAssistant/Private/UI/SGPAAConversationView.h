// Copyright Game Production AI Assistant contributors 2026.
#pragma once
#include "Widgets/SCompoundWidget.h"

class SMultiLineEditableTextBox;
class SScrollBox;
class SVerticalBox;

// Session-only conversation rendering; retrieval and provider selection stay outside Slate.
class SGPAAConversationView final : public SCompoundWidget
{
  public:
    SLATE_BEGIN_ARGS(SGPAAConversationView) {}
    SLATE_END_ARGS()
    void Construct(const FArguments &InArgs);
    void BeginTurn(const FString &Prompt);
    void UpdateAnswer(const FString &Text);
    void FinishTurn(const FString &Text);
    bool HasActiveTurn() const { return bActiveTurn; }

  private:
    friend class FGPAAPromptLayoutTest;
    friend class FGPAAViewSelectorTest;
    TSharedPtr<SScrollBox> Scroll;
    TSharedPtr<SVerticalBox> Messages;
    TSharedPtr<SMultiLineEditableTextBox> AnswerTextBox;
    bool bActiveTurn = false;
};
