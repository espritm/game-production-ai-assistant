// Copyright Game Production AI Assistant contributors 2026.
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Settings/GPAAEditorSettings.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "IDetailsView.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Layout/Children.h"

namespace
{
void TickSettingsTree(const TSharedRef<SWidget>& Widget)
{
    // A hidden native window is not ticked by Slate's visible-window traversal.
    // Tick the offscreen tree so virtualized detail rows are actually generated.
    Widget->Tick(FGeometry::MakeRoot(FVector2D(1200, 1800), FSlateLayoutTransform()), FPlatformTime::Seconds(), 1.f / 60.f);
    FChildren* Children = Widget->GetChildren();
    for (int32 I = 0; I < Children->Num(); ++I) TickSettingsTree(Children->GetChildAt(I));
}
bool ContainsSettingsText(const TSharedRef<SWidget>& Widget, const FString& Text)
{
    if (Widget->GetTypeAsString() == TEXT("STextBlock") &&
        StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString() == Text) return true;
    FChildren* Children = Widget->GetChildren();
    for (int32 I = 0; I < Children->Num(); ++I)
        if (ContainsSettingsText(Children->GetChildAt(I), Text)) return true;
    return false;
}
bool HasCredentialRow(const TSharedRef<SWidget>& Widget)
{
    if (Widget->GetTypeAsString() == TEXT("SHorizontalBox") &&
        ContainsSettingsText(Widget, TEXT("Save Personal Key")) &&
        ContainsSettingsText(Widget, TEXT("Remove Personal Key")) && Widget->GetChildren()->Num() == 3) return true;
    FChildren* Children = Widget->GetChildren();
    for (int32 I = 0; I < Children->Num(); ++I)
        if (HasCredentialRow(Children->GetChildAt(I))) return true;
    return false;
}
struct FGPAASettingsViewState
{
    TSharedPtr<IDetailsView> View;
    TSharedPtr<SWindow> Window;
    double Deadline = 0;
};
}
DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FGPAAWaitForSettingsActions, TSharedPtr<FGPAASettingsViewState>, State,
    FAutomationTestBase*, Test);
bool FGPAAWaitForSettingsActions::Update()
{
    State->Window->SlatePrepass();
    TickSettingsTree(State->View.ToSharedRef());
    const bool Save = ContainsSettingsText(State->View.ToSharedRef(), TEXT("Save Personal Key"));
    const bool Remove = ContainsSettingsText(State->View.ToSharedRef(), TEXT("Remove Personal Key"));
    if ((!Save || !Remove) && FPlatformTime::Seconds() < State->Deadline) return false;
    Test->TestTrue(TEXT("Settings view exposes Save Personal Key"), Save);
    Test->TestTrue(TEXT("Settings view exposes Remove Personal Key"), Remove);
    Test->TestTrue(TEXT("Credential field and both actions share one horizontal row"), HasCredentialRow(State->View.ToSharedRef()));
    Test->TestFalse(TEXT("Personal settings do not force shared default config"),
        UGPAAEditorSettings::StaticClass()->HasAnyClassFlags(CLASS_DefaultConfig));
    Test->TestTrue(TEXT("Local model download action available"), ContainsSettingsText(State->View.ToSharedRef(), TEXT("Download Selected Model")));
    Test->TestTrue(TEXT("Existing GGUF selection available"), ContainsSettingsText(State->View.ToSharedRef(), TEXT("Use Downloaded Model")));
    Test->TestTrue(TEXT("Export action remains available"), ContainsSettingsText(State->View.ToSharedRef(), TEXT("Export Settings Profile")));
    Test->TestTrue(TEXT("Import action remains available"), ContainsSettingsText(State->View.ToSharedRef(), TEXT("Import Settings Profile")));
    for (const TCHAR* Removed : {TEXT("Check Assistant"), TEXT("Test Prompt IA (Billable)"),
        TEXT("Import Legacy Chat History"), TEXT("Export Profile Path"), TEXT("Import Profile Path"), TEXT("Legacy History Path")})
        Test->TestFalse(Removed, ContainsSettingsText(State->View.ToSharedRef(), Removed));
    State->Window->RequestDestroyWindow();
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPAASettingsActionsTest, "GPAA.Native.UI.SettingsCredentialActions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGPAASettingsActionsTest::RunTest(const FString& Parameters)
{
    auto State = MakeShared<FGPAASettingsViewState>();
    FDetailsViewArgs Args;
    Args.bAllowSearch = false;
    auto& Module = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
    State->View = Module.CreateDetailView(Args);
    State->View->SetObject(GetMutableDefault<UGPAAEditorSettings>(), true);
    State->Window = SNew(SWindow).Title(FText::FromString(TEXT("GPAA settings acceptance")))
        .ClientSize(FVector2D(1200, 1800))[State->View.ToSharedRef()];
    FSlateApplication::Get().AddWindow(State->Window.ToSharedRef(), false);
    State->Deadline = FPlatformTime::Seconds() + 10;
    ADD_LATENT_AUTOMATION_COMMAND(FGPAAWaitForSettingsActions(State, this));
    return true;
}
#endif
