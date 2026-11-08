// Copyright Game Production AI Assistant contributors 2026.
#include "Settings/GPAAEditorSettingsDetails.h"
#include "Settings/GPAAEditorSettings.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IPropertyUtilities.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<IDetailCustomization> FGPAAEditorSettingsDetails::MakeInstance()
{
    return MakeShared<FGPAAEditorSettingsDetails>();
}

void FGPAAEditorSettingsDetails::CustomizeDetails(IDetailLayoutBuilder& Builder)
{
    TArray<TWeakObjectPtr<UObject>> Objects;
    Builder.GetObjectsBeingCustomized(Objects);
    if (Objects.Num() != 1) return;
    const TWeakObjectPtr<UGPAAEditorSettings> Settings(Cast<UGPAAEditorSettings>(Objects[0].Get()));
    if (!Settings.IsValid()) return;
    const TWeakPtr<IPropertyUtilities> Utilities = Builder.GetPropertyUtilities();
    auto AddAction = [&](const FName Category, const TCHAR* Label, void (UGPAAEditorSettings::*Action)())
    {
        const FText Text = FText::FromString(Label);
        Builder.EditCategory(Category).AddCustomRow(Text).WholeRowContent()
        [
            SNew(SButton)
            .Text(Text)
            .OnClicked_Lambda([Settings, Utilities, Action]()
            {
                if (Settings.IsValid()) (Settings.Get()->*Action)();
                if (const auto Pinned = Utilities.Pin()) Pinned->RequestRefresh();
                return FReply::Handled();
            })
        ];
    };
    AddAction(TEXT("Chat"), TEXT("Save Personal Key"), &UGPAAEditorSettings::SavePersonalKey);
    AddAction(TEXT("Chat"), TEXT("Remove Personal Key"), &UGPAAEditorSettings::RemovePersonalKey);
    Builder.EditCategory(TEXT("Chat")).AddCustomRow(FText::FromString(TEXT("Personal key status"))).WholeRowContent()
    [
        SNew(STextBlock)
        .AutoWrapText(true)
        .Text_Lambda([Settings]() { return FText::FromString(Settings.IsValid() ? Settings->LastDiagnosticsResult : FString()); })
    ];
    AddAction(TEXT("Diagnostics"), TEXT("Check Assistant"), &UGPAAEditorSettings::CheckAssistant);
    AddAction(TEXT("Diagnostics"), TEXT("Test Prompt IA (Billable)"), &UGPAAEditorSettings::TestPromptIA);
    AddAction(TEXT("Import / Export"), TEXT("Export Settings Profile"), &UGPAAEditorSettings::ExportSettingsProfile);
    AddAction(TEXT("Import / Export"), TEXT("Import Settings Profile"), &UGPAAEditorSettings::ImportSettingsProfile);
    AddAction(TEXT("Import / Export"), TEXT("Import Legacy Chat History"), &UGPAAEditorSettings::ImportLegacyChatHistory);
}
