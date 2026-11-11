// Copyright Game Production AI Assistant contributors 2026.
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "UI/SGPAAssistantWidget.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/STextBlock.h"
#include "Layout/Children.h"

namespace
{
bool HasObsoleteKnowledgeControl(const TSharedRef<SWidget>& Widget)
{
    if (Widget->GetTypeAsString() == TEXT("STextBlock"))
    {
        const FString Text = StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString();
        if (Text == TEXT("Refresh Knowledge") || Text == TEXT("Project Knowledge")) return true;
    }
    auto* Children = Widget->GetChildren();
    for (int32 I = 0; I < Children->Num(); ++I)
        if (HasObsoleteKnowledgeControl(Children->GetChildAt(I))) return true;
    return false;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPAAHistoryLayoutTest, "GPAA.Native.UI.HistoryExclusiveView",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGPAAHistoryLayoutTest::RunTest(const FString& Parameters)
{
    auto Panel = SNew(SGPAAssistantWidget);
    Panel->PromptQuestionTextBox->SetText(FText::FromString(TEXT("Keep my question")));
    Panel->OnSearchTabClicked();
    TestFalse(TEXT("Manual refresh and knowledge panel are removed"), HasObsoleteKnowledgeControl(Panel));
    Panel->OnHistoryClicked();
    TestEqual(TEXT("History replaces the entire main view"), Panel->MainViewSwitcher->GetActiveWidgetIndex(), 1);
    Panel->OnCloseHistoryClicked();
    TestEqual(TEXT("Back restores the main view"), Panel->MainViewSwitcher->GetActiveWidgetIndex(), 0);
    TestEqual(TEXT("Search tab selection is preserved"), Panel->TabSwitcher->GetActiveWidgetIndex(), 1);
    TestEqual(TEXT("Question text is preserved"), Panel->GetPromptQuestion(), FString(TEXT("Keep my question")));
    return true;
}
#endif
