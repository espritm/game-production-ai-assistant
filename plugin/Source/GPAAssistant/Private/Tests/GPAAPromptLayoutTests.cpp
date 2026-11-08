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
    return true;
}
#endif
