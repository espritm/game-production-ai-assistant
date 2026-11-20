// Copyright Game Production AI Assistant contributors 2026.
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "UI/SGPAAssistantWidget.h"
#include "UI/SGPAAQuickStartView.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/STextBlock.h"
#include "Layout/Children.h"
#include "Settings/GPAAEditorSettings.h"
#include "ISettingsModule.h"
#include "ISettingsContainer.h"
#include "ISettingsCategory.h"
#include "ISettingsSection.h"
#include "Modules/ModuleManager.h"
#include "Serialization/ObjectWriter.h"
#include "Serialization/ObjectReader.h"

namespace
{
bool HasObsoleteKnowledgeControl(const TSharedRef<SWidget>& Widget)
{
    if (Widget->GetTypeAsString() == TEXT("STextBlock"))
    {
        const FString Text = StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString();
        if (Text == TEXT("Refresh Knowledge") || Text == TEXT("Project Knowledge") || Text == TEXT("History")) return true;
    }
    auto* Children = Widget->GetChildren();
    for (int32 I = 0; I < Children->Num(); ++I)
        if (HasObsoleteKnowledgeControl(Children->GetChildAt(I))) return true;
    return false;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPAASetupLayoutTest, "GPAA.Native.UI.SetupVisibilityAndNoHistory",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGPAASetupLayoutTest::RunTest(const FString& Parameters)
{
    auto Panel = SNew(SGPAAssistantWidget);
    Panel->PromptQuestionTextBox->SetText(FText::FromString(TEXT("Keep my question")));
    Panel->OnSearchTabClicked();
    TestFalse(TEXT("Manual refresh and knowledge panel are removed"), HasObsoleteKnowledgeControl(Panel));
    auto* Settings = GetMutableDefault<UGPAAEditorSettings>();
    TArray<uint8> Snapshot;
    FObjectWriter Backup(Settings, Snapshot);
    Settings->SelectAI(true);
    TestTrue(TEXT("Quick Start is hidden with Local configured"), Panel->QuickStartView->GetQuickStartButtonVisibility() == EVisibility::Collapsed);
    Settings->SelectAI(false);
    TestTrue(TEXT("Quick Start is hidden with Cloud configured"), Panel->QuickStartView->GetQuickStartButtonVisibility() == EVisibility::Collapsed);
    TestFalse(TEXT("Configured local mode requires no setup"), Settings->ChatProvider.IsEmpty());
    auto Container = FModuleManager::LoadModuleChecked<ISettingsModule>(TEXT("Settings")).GetContainer(TEXT("Project"));
    auto Category = Container ? Container->GetCategory(TEXT("Plugins")) : nullptr;
    auto Section = Category ? Category->GetSection(TEXT("GameProductionAIAssistant")) : nullptr;
    TestTrue(TEXT("Native Settings section is registered"), Section.IsValid());
    if (Section)
    {
        TestTrue(TEXT("Native reset succeeds"), Section->ResetDefaults());
        TestTrue(TEXT("Reset clears both AI radio choices"), Settings->ChatProvider.IsEmpty());
        TestTrue(TEXT("Quick Start returns after native reset"), Panel->QuickStartView->GetQuickStartButtonVisibility() == EVisibility::Visible);
    }
    FObjectReader Restore(Settings, Snapshot);
    Settings->SaveConfig();
    TestFalse(TEXT("History button and old knowledge controls are absent"), HasObsoleteKnowledgeControl(Panel));
    TestEqual(TEXT("Question text is preserved"), Panel->GetPromptQuestion(), FString(TEXT("Keep my question")));
    return true;
}
#endif
