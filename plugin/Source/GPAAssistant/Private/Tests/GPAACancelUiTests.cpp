// Copyright Game Production AI Assistant contributors 2026.
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "UI/SGPAAssistantWidget.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/STextBlock.h"
#include "Layout/Children.h"
namespace
{
bool ContainsCancelWidget(const TSharedRef<SWidget>& Root, const SWidget* Target)
{
    if (&Root.Get() == Target) return true;
    auto* Children=Root->GetChildren();
    for(int32 I=0; I<Children->Num(); ++I) if(ContainsCancelWidget(Children->GetChildAt(I),Target)) return true;
    return false;
}
bool ContainsActionText(const TSharedRef<SWidget>& Root, const FString& Text)
{
    if(Root->GetTypeAsString()==TEXT("STextBlock") && StaticCastSharedRef<STextBlock>(Root)->GetText().ToString()==Text) return true;
    auto* Children=Root->GetChildren();
    for(int32 I=0; I<Children->Num(); ++I) if(ContainsActionText(Children->GetChildAt(I),Text)) return true;
    return false;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPAACancelUiTest,"GPAA.Native.UI.ContextualCancellation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGPAACancelUiTest::RunTest(const FString& Parameters)
{
    auto Panel=SNew(SGPAAssistantWidget);
    auto Prompt=Panel->TabSwitcher->GetActiveWidget().ToSharedRef();
    TestTrue(TEXT("Prompt action is Send"),ContainsActionText(Prompt,TEXT("Send")));
    TestFalse(TEXT("Old Ask label removed"),ContainsActionText(Prompt,TEXT("Ask")));
    TestTrue(TEXT("Prompt Cancel belongs to prompt content"),ContainsCancelWidget(Prompt,Panel->PromptCancelButton.Get()));
    TestFalse(TEXT("Search Cancel is outside prompt content"),ContainsCancelWidget(Prompt,Panel->SearchCancelButton.Get()));
    Panel->OnSearchTabClicked();
    TestTrue(TEXT("Search Cancel belongs to search content"),ContainsCancelWidget(Panel->TabSwitcher->GetActiveWidget().ToSharedRef(),Panel->SearchCancelButton.Get()));
    TestTrue(TEXT("Idle buttons collapse"),Panel->GetRequestCancelVisibility(true)==EVisibility::Collapsed && Panel->GetRequestCancelVisibility(false)==EVisibility::Collapsed);
    Panel->SetBusy(TEXT("Loading a non-cancellable task"));
    TestTrue(TEXT("Busy alone does not expose cancellation"),Panel->GetRequestCancelVisibility(false)==EVisibility::Collapsed);
    Panel->bCancellable=true; Panel->bRequestIsAsk=true;
    TestTrue(TEXT("Only Send cancellation is visible during prompt request"),Panel->GetRequestCancelVisibility(true)==EVisibility::Visible && Panel->GetRequestCancelVisibility(false)==EVisibility::Collapsed);
    Panel->SetHistoryResult(true,TEXT("history"));
    TestTrue(TEXT("History load cannot clear live prompt cancellation"),Panel->GetRequestCancelVisibility(true)==EVisibility::Visible);
    Panel->bRequestIsAsk=false;
    TestTrue(TEXT("Only Search cancellation is visible during search"),Panel->GetRequestCancelVisibility(false)==EVisibility::Visible && Panel->GetRequestCancelVisibility(true)==EVisibility::Collapsed);
    Panel->OnCancelClicked();
    TestTrue(TEXT("Cancellation hides both buttons"),Panel->GetRequestCancelVisibility(true)==EVisibility::Collapsed && Panel->GetRequestCancelVisibility(false)==EVisibility::Collapsed);
    Panel->bCancellable=true; Panel->SetBusy(TEXT("Indexing")); Panel->StopBusy();
    TestTrue(TEXT("Completion also removes cancellation"),Panel->GetRequestCancelVisibility(false)==EVisibility::Collapsed);
    return true;
}
#endif
