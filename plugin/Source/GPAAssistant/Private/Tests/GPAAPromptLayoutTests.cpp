// Copyright Game Production AI Assistant contributors 2026.
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "UI/SGPAAssistantWidget.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Layout/ArrangedChildren.h"
#include "Layout/Children.h"

namespace
{
bool HasOldPromptLabel(const TSharedRef<SWidget>& Widget)
{
    if (Widget->GetTypeAsString() == TEXT("STextBlock"))
    {
        const FString Text = StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString();
        if (Text == TEXT("Ask answer") || Text == TEXT("Question")) return true;
    }
    auto* Children = Widget->GetChildren();
    for (int32 I = 0; I < Children->Num(); ++I)
        if (HasOldPromptLabel(Children->GetChildAt(I))) return true;
    return false;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPAAPromptLayoutTest, "GPAA.Native.UI.PromptBottomAlignment",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGPAAPromptLayoutTest::RunTest(const FString& Parameters)
{
    auto Panel = SNew(SGPAAssistantWidget);
    TestTrue(TEXT("Prompt starts empty, not with a submitted example"), Panel->GetPromptQuestion().IsEmpty());
    TestEqual(TEXT("Prompt hint matches requested copy"), Panel->PromptQuestionTextBox->GetHintText().ToString(), FString(TEXT("Prompt your AI here")));
    TestFalse(TEXT("Redundant prompt and answer labels removed"), HasOldPromptLabel(Panel));
    Panel->SetAnswerText(TEXT("A short answer."));
    Panel->AnswerTextBox->SlatePrepass();
    const FGeometry Viewport = FGeometry::MakeRoot(FVector2D(600, 400), FSlateLayoutTransform());
    FArrangedChildren Short(EVisibility::Visible);
    Panel->AnswerTextBox->ArrangeChildren(Viewport, Short);
    TestEqual(TEXT("Answer content has one bordered child"), Short.Num(), 1);
    if (Short.Num() == 1)
        TestTrue(TEXT("Short answer is arranged at the bottom of the full-height frame"), Short[0].Geometry.GetAbsolutePosition().Y > 300);
    FString LongAnswer;
    for (int32 I = 0; I < 300; ++I) LongAnswer += TEXT("A longer streamed answer remains selectable and scrollable.\n");
    Panel->SetAnswerText(LongAnswer);
    Panel->AnswerTextBox->SlatePrepass();
    FArrangedChildren Long(EVisibility::Visible);
    Panel->AnswerTextBox->ArrangeChildren(Viewport, Long);
    TestEqual(TEXT("Long answer text retained in full"), Panel->AnswerTextBox->GetText().ToString(), LongAnswer);
    if (Long.Num() == 1)
        TestTrue(TEXT("Long content is constrained to its viewport"), Long[0].Geometry.GetLocalSize().Y <= 400);
    Panel->PromptQuestionTextBox->SetText(FText::FromString(TEXT("First question")));
    Panel->BeginPromptTurn(Panel->GetPromptQuestion());
    TestTrue(TEXT("Sending clears the input"), Panel->GetPromptQuestion().IsEmpty());
    Panel->NativeAnswer = TEXT("First streamed answer"); Panel->RenderPromptTurn();
    TestTrue(TEXT("Submitted question remains visible during streaming"), Panel->AnswerTextBox->GetText().ToString().Contains(TEXT("You:\nFirst question")));
    Panel->SetChatResult(true, TEXT("Complete"), TEXT("First complete answer"), {});
    Panel->BeginPromptTurn(TEXT("Second question"));
    Panel->NativeAnswer = TEXT("Second partial answer"); Panel->RenderPromptTurn();
    const FString Stream = Panel->AnswerTextBox->GetText().ToString();
    TestTrue(TEXT("Second response preserves the previous turn"), Stream.Contains(TEXT("First complete answer")) && Stream.Contains(TEXT("Second question")) && Stream.Contains(TEXT("Second partial answer")));
    TestFalse(TEXT("Completed turn replaces its streaming draft"), Stream.Contains(TEXT("First streamed answer")));
    Panel->SetChatResult(false, TEXT("Test interruption"), TEXT("Second partial answer"), {});
    TestTrue(TEXT("Interrupted response remains in the thread"), Panel->AnswerTextBox->GetText().ToString().Contains(TEXT("Second partial answer\n\n[Interrupted] Test interruption")));
    const FString Thread = Panel->AnswerTextBox->GetText().ToString();
    Panel->SetResult(true, TEXT("Settings loaded"));
    TestEqual(TEXT("Status updates do not replace conversation"), Panel->AnswerTextBox->GetText().ToString(), Thread);
    Panel->bBusy = true;
    Panel->PromptQuestionTextBox->SetText(FText::FromString(TEXT("Unsent draft")));
    Panel->StartNative(true);
    TestEqual(TEXT("Sending while busy preserves unsent input"), Panel->GetPromptQuestion(), FString(TEXT("Unsent draft")));
    Panel->StopBusy();
    return true;
}
#endif
