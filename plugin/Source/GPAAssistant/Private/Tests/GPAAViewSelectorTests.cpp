// Copyright Game Production AI Assistant contributors 2026.
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "UI/SGPAAssistantWidget.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPAAViewSelectorTest, "GPAA.Native.UI.SegmentedViewSelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGPAAViewSelectorTest::RunTest(const FString& Parameters)
{
    auto Panel = SNew(SGPAAssistantWidget);
    TestEqual(TEXT("Selector contains exactly two views"), Panel->ViewSelector->NumSlots(), 2);
    TestEqual(TEXT("Prompt is initially displayed"), Panel->TabSwitcher->GetActiveWidgetIndex(), 0);
    Panel->PromptQuestionTextBox->SetText(FText::FromString(TEXT("Keep this prompt")));
    Panel->SearchQueryTextBox->SetText(FText::FromString(TEXT("Keep this search")));
    Panel->SetAnswerText(TEXT("Keep this answer"));
    Panel->OnViewSelectionChanged(1);
    TestEqual(TEXT("Search selection displays search"), Panel->TabSwitcher->GetActiveWidgetIndex(), 1);
    TestEqual(TEXT("Selection state agrees with content"), Panel->ActiveTabIndex, 1);
    Panel->OnHistoryClicked();
    Panel->OnCloseHistoryClicked();
    TestEqual(TEXT("History Back preserves selected view"), Panel->TabSwitcher->GetActiveWidgetIndex(), 1);
    Panel->OnViewSelectionChanged(0);
    TestEqual(TEXT("Prompt selection restores prompt"), Panel->TabSwitcher->GetActiveWidgetIndex(), 0);
    TestEqual(TEXT("Prompt text retained"), Panel->GetPromptQuestion(), FString(TEXT("Keep this prompt")));
    TestEqual(TEXT("Search text retained"), Panel->GetSearchQuery(), FString(TEXT("Keep this search")));
    TestEqual(TEXT("Answer retained"), Panel->AnswerTextBox->GetText().ToString(), FString(TEXT("Keep this answer")));
    Panel->OnViewSelectionChanged(-1);
    TestEqual(TEXT("Invalid selection cannot hide both views"), Panel->TabSwitcher->GetActiveWidgetIndex(), 0);
    return true;
}
#endif
