// Copyright Game Production AI Assistant contributors 2026.
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "UI/SGPAAssistantWidget.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Layout/ArrangedChildren.h"
#include "Layout/Children.h"
#include "Widgets/SBoxPanel.h"

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
    auto CheckComposer = [&](const FString& Text)
    {
        Panel->PromptQuestionTextBox->SetText(FText::FromString(Text));
        Panel->PromptInputRow->SlatePrepass();
        const float Height = Panel->PromptInputRow->GetDesiredSize().Y;
        FArrangedChildren Composer(EVisibility::Visible);
        Panel->PromptInputRow->ArrangeChildren(FGeometry::MakeRoot(FVector2D(800, Height), FSlateLayoutTransform()), Composer);
        TestEqual(TEXT("Idle composer has an input and Send"), Composer.Num(), 2);
        if (Composer.Num() == 2)
        {
            TestTrue(TEXT("Prompt reserves at least 64 Slate units even when empty"), Composer[0].Geometry.GetLocalSize().Y >= 64);
            TestTrue(TEXT("Send keeps its natural compact height"), Composer[1].Geometry.GetLocalSize().Y < 40);
            const float InputBottom = Composer[0].Geometry.GetAbsolutePosition().Y + Composer[0].Geometry.GetLocalSize().Y;
            const float SendBottom = Composer[1].Geometry.GetAbsolutePosition().Y + Composer[1].Geometry.GetLocalSize().Y;
            TestTrue(TEXT("Send bottom matches prompt bottom"), FMath::IsNearlyEqual(InputBottom, SendBottom, .1f));
        }
    };
    CheckComposer(TEXT(""));
    CheckComposer(TEXT("One\nTwo\nThree"));
    CheckComposer(TEXT("One\nTwo\nThree\nFour\nFive\nSix\nSeven"));
    Panel->PromptQuestionTextBox->SetText(FText::GetEmpty());
    Panel->SetAnswerText(TEXT("A short answer."));
    const auto FirstAnswer = Panel->AnswerTextBox;
    FString LongAnswer;
    for (int32 I = 0; I < 300; ++I) LongAnswer += TEXT("A longer streamed answer remains selectable.\n");
    Panel->SetAnswerText(LongAnswer);
    TestEqual(TEXT("Long answers are retained"), Panel->AnswerTextBox->GetText().ToString(), LongAnswer);
    Panel->PromptQuestionTextBox->SetText(FText::FromString(TEXT("First question")));
    Panel->BeginPromptTurn(Panel->GetPromptQuestion());
    TestTrue(TEXT("Sending clears the input"), Panel->GetPromptQuestion().IsEmpty());
    Panel->NativeAnswer = TEXT("First streamed answer"); Panel->RenderPromptTurn();
    TestEqual(TEXT("User and AI have separate message rows"), Panel->ThreadMessages->NumSlots(), 3);
    Panel->ThreadMessages->SlatePrepass();
    FArrangedChildren Rows(EVisibility::Visible);
    Panel->ThreadMessages->ArrangeChildren(FGeometry::MakeRoot(FVector2D(800,1200), FSlateLayoutTransform()), Rows);
    TestEqual(TEXT("All message rows are arranged"), Rows.Num(), 3);
    if (Rows.Num() == 3) TestTrue(TEXT("User prompt is aligned to the right"), Rows[1].Geometry.GetAbsolutePosition().X > 0);
    const auto FirstTurnAnswer = Panel->AnswerTextBox;
    Panel->SetChatResult(true, TEXT("Complete"), TEXT("First complete answer"), {});
    Panel->BeginPromptTurn(TEXT("Second question"));
    Panel->NativeAnswer = TEXT("Second partial answer"); Panel->RenderPromptTurn();
    TestEqual(TEXT("Previous answer widget retains the completed response"), FirstTurnAnswer->GetText().ToString(), FString(TEXT("First complete answer")));
    TestTrue(TEXT("New response uses its own widget"), Panel->AnswerTextBox != FirstTurnAnswer);
    TestEqual(TEXT("Both prompt/answer pairs remain in the thread"), Panel->ThreadMessages->NumSlots(), 5);
    TestEqual(TEXT("Only the active response streams"), Panel->AnswerTextBox->GetText().ToString(), FString(TEXT("Second partial answer")));
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
