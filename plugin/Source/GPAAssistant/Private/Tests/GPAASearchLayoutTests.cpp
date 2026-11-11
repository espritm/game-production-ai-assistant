// Copyright Game Production AI Assistant contributors 2026.
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "UI/SGPAAssistantWidget.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/STextBlock.h"
#include "Layout/Children.h"

namespace
{
void FlattenSearchWidgets(const TSharedRef<SWidget>& Widget, TArray<TSharedRef<SWidget>>& Widgets)
{
    Widgets.Add(Widget);
    auto* Children = Widget->GetChildren();
    for (int32 I = 0; I < Children->Num(); ++I) FlattenSearchWidgets(Children->GetChildAt(I), Widgets);
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPAASearchLayoutTest, "GPAA.Native.UI.SearchScopeRadios",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGPAASearchLayoutTest::RunTest(const FString& Parameters)
{
    auto Panel = SNew(SGPAAssistantWidget);
    Panel->OnSearchTabClicked();
    TestTrue(TEXT("Search starts empty"), Panel->GetSearchQuery().IsEmpty());
    TestEqual(TEXT("Search hint"), Panel->SearchQueryTextBox->GetHintText().ToString(), FString(TEXT("Search")));
    TArray<TSharedRef<SWidget>> Widgets;
    FlattenSearchWidgets(Panel->TabSwitcher->GetActiveWidget().ToSharedRef(), Widgets);
    int32 RadioCount = 0, LastRadio = -1, Input = -1;
    for (int32 I = 0; I < Widgets.Num(); ++I)
    {
        if (Widgets[I]->GetTypeAsString() == TEXT("SCheckBox")) { ++RadioCount; LastRadio = I; }
        if (&Widgets[I].Get() == Panel->SearchQueryTextBox.Get()) Input = I;
        if (Widgets[I]->GetTypeAsString() == TEXT("STextBlock"))
        {
            const FString Text = StaticCastSharedRef<STextBlock>(Widgets[I])->GetText().ToString();
            TestFalse(TEXT("Obsolete search labels removed"), Text == TEXT("Search query") || Text == TEXT("Filter"));
        }
    }
    TestEqual(TEXT("Exactly three scope choices"), RadioCount, 3);
    TestTrue(TEXT("All radio choices precede the search field"), LastRadio >= 0 && Input > LastRadio);
    TestTrue(TEXT("Combined scope is default"), Panel->GetSearchScopeCheckState(TEXT("All")) == ECheckBoxState::Checked);
    auto Asset = MakeShared<FGPAASourceReference>(); Asset->SourceType = TEXT("Asset");
    auto Code = MakeShared<FGPAASourceReference>(); Code->SourceType = TEXT("Code");
    Panel->SearchResults = {Asset, Code};
    Panel->OnSearchScopeChanged(ECheckBoxState::Checked, TEXT("Assets"));
    TestEqual(TEXT("Asset scope displays only asset results"), Panel->FilteredSearchResults.Num(), 1);
    TestTrue(TEXT("Correct asset result selected"), Panel->FilteredSearchResults[0] == Asset);
    TestTrue(TEXT("Combined radio clears when asset selected"), Panel->GetSearchScopeCheckState(TEXT("All")) == ECheckBoxState::Unchecked);
    Panel->OnSearchScopeChanged(ECheckBoxState::Unchecked, TEXT("Assets"));
    TestTrue(TEXT("Selected radio cannot clear the group"), Panel->GetSearchScopeCheckState(TEXT("Assets")) == ECheckBoxState::Checked);
    Panel->OnSearchScopeChanged(ECheckBoxState::Checked, TEXT("Code"));
    TestTrue(TEXT("Code scope displays only code"), Panel->FilteredSearchResults.Num() == 1 && Panel->FilteredSearchResults[0] == Code);
    TestTrue(TEXT("Previous radio is unchecked"), Panel->GetSearchScopeCheckState(TEXT("Assets")) == ECheckBoxState::Unchecked);
    TestEqual(TEXT("Query receives the chosen scope"), Panel->GetSelectedSourceFilterName(), FString(TEXT("Code")));
    Panel->OnSearchScopeChanged(ECheckBoxState::Checked, TEXT("All"));
    TestEqual(TEXT("Combined scope restores both result types"), Panel->FilteredSearchResults.Num(), 2);
    Panel->SetResult(true, TEXT(""));
    TestEqual(TEXT("Successful generic tasks report Ready"), Panel->StatusText->GetText().ToString(), FString(TEXT("Ready")));
    return true;
}
#endif
