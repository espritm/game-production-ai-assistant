// Copyright Game Production AI Assistant contributors 2026.
#include "Settings/GPAAEditorSettingsDetails.h"
#include "Settings/GPAAEditorSettings.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"
#include "GPAALocalModels.h"
#include "GPAALocalChat.h"
#include "IDetailPropertyRow.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Styling/CoreStyle.h"

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
    auto MakeAction = [Settings, Utilities](const TCHAR* Label, void (UGPAAEditorSettings::*Action)())
    {
        return SNew(SButton).Text(FText::FromString(Label))
            .OnClicked_Lambda([Settings, Utilities, Action]()
            {
                if (Settings.IsValid()) (Settings.Get()->*Action)();
                if (const auto Pinned = Utilities.Pin()) Pinned->RequestRefresh();
                return FReply::Handled();
            });
    };
    const auto Key = Builder.GetProperty(GET_MEMBER_NAME_CHECKED(UGPAAEditorSettings, OpenAIApiKey));
    Builder.EditCategory(TEXT("Chat")).AddProperty(Key).CustomWidget()
        .NameContent()[Key->CreatePropertyNameWidget()]
        .ValueContent().MinDesiredWidth(480).MaxDesiredWidth(1200)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1)[Key->CreatePropertyValueWidget()]
            + SHorizontalBox::Slot().AutoWidth().Padding(6, 0)
                [MakeAction(TEXT("Save Personal Key"), &UGPAAEditorSettings::SavePersonalKey)]
            + SHorizontalBox::Slot().AutoWidth()
                [MakeAction(TEXT("Remove Personal Key"), &UGPAAEditorSettings::RemovePersonalKey)]
        ];
    auto AddStatus = [&](const FName Category, FString UGPAAEditorSettings::*Member)
    {
        Builder.EditCategory(Category).AddCustomRow(FText::FromString(TEXT("Status")))
            .Visibility(TAttribute<EVisibility>::CreateLambda([Settings, Member]()
            { return Settings.IsValid() && !(Settings.Get()->*Member).IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; }))
            .WholeRowContent()
            [SNew(STextBlock).AutoWrapText(true).Text_Lambda([Settings, Member]()
            { return FText::FromString(Settings.IsValid() ? Settings.Get()->*Member : FString()); })];
    };
    AddStatus(TEXT("Chat"), &UGPAAEditorSettings::LastKeyResult);
    auto& Local = Builder.EditCategory(TEXT("Local AI"));
    const auto ModelId = Builder.GetProperty(GET_MEMBER_NAME_CHECKED(UGPAAEditorSettings, LocalModelId));
    const auto ModelSelector = ModelId->CreatePropertyValueWidget();
    ModelSelector->SetEnabled(TAttribute<bool>::CreateLambda([]() { return !FGPAALocalModels::Get().IsBusy(); }));
    Local.AddProperty(ModelId).CustomWidget()
        .NameContent()[ModelId->CreatePropertyNameWidget()]
        .ValueContent().MinDesiredWidth(320)
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1)
                [ModelSelector]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6, 0)
                [SNew(SImage).Image(FCoreStyle::Get().GetBrush(TEXT("Icons.Help")))
                    .ToolTipText(NSLOCTEXT("GPAASettings", "LocalModelCatalogueHelp",
                        "This list is a catalogue built into the plugin, not a live list from a model service.\n\n"
                        "It contains Qwen2.5-Coder Instruct 1.5B (Light) and 7B (Larger), in GGUF Q4_K_M format, "
                        "from Qwen's official repositories on Hugging Face:\n"
                        "huggingface.co/Qwen/Qwen2.5-Coder-1.5B-Instruct-GGUF\n"
                        "huggingface.co/Qwen/Qwen2.5-Coder-7B-Instruct-GGUF\n\n"
                        "Both models use the Apache 2.0 license. Downloads start only when you request them; "
                        "the plugin verifies the pinned model version using its size and SHA-256 checksum. "
                        "Generation runs locally on your CPU, without sending prompts to Hugging Face.\n\n"
                        "To try another model, use Browse... to select a local GGUF file. Compatibility is not guaranteed."))]];
    Local.AddCustomRow(FText::FromString(TEXT("Model download information"))).WholeRowContent()
        [SNew(STextBlock).AutoWrapText(true).Text_Lambda([Settings]()
        { return FText::FromString(Settings.IsValid() ? Settings->GetLocalModelDescription() : FString()); })];
    const auto ModelFile = Builder.GetProperty(GET_MEMBER_NAME_CHECKED(UGPAAEditorSettings, LocalModelFile));
    Local.AddProperty(ModelFile).CustomWidget()
        .NameContent()[ModelFile->CreatePropertyNameWidget()]
        .ValueContent().MinDesiredWidth(480)
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1)[ModelFile->CreatePropertyValueWidget()]
            + SHorizontalBox::Slot().AutoWidth().Padding(6, 0)[MakeAction(TEXT("Browse..."), &UGPAAEditorSettings::ChooseLocalModelFile)]
            + SHorizontalBox::Slot().AutoWidth()[MakeAction(TEXT("Use Downloaded Model"), &UGPAAEditorSettings::ClearLocalModelFile)]];
    auto DownloadButton = MakeAction(TEXT("Download Selected Model"), &UGPAAEditorSettings::DownloadLocalModel);
    auto CancelButton = MakeAction(TEXT("Cancel Download"), &UGPAAEditorSettings::CancelLocalDownload);
    auto RemoveButton = MakeAction(TEXT("Remove Downloaded Model"), &UGPAAEditorSettings::RemoveLocalModel);
    DownloadButton->SetEnabled(TAttribute<bool>::CreateLambda([]() { return !FGPAALocalModels::Get().IsBusy(); }));
    CancelButton->SetEnabled(TAttribute<bool>::CreateLambda([]() { return FGPAALocalModels::Get().IsBusy(); }));
    RemoveButton->SetEnabled(TAttribute<bool>::CreateLambda([]() { return !FGPAALocalModels::Get().IsBusy() && !FGPAALocalChat::IsAnyBusy(); }));
    Local.AddCustomRow(FText::FromString(TEXT("Download local model"))).WholeRowContent()
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()[DownloadButton]
            + SHorizontalBox::Slot().AutoWidth().Padding(6, 0)[CancelButton]
            + SHorizontalBox::Slot().AutoWidth()[RemoveButton]];
    Local.AddCustomRow(FText::FromString(TEXT("Local model status"))).WholeRowContent()
        [SNew(STextBlock).AutoWrapText(true).Text_Lambda([]()
        { return FText::FromString(FGPAALocalModels::Get().Status()); })];
    auto& Profiles = Builder.EditCategory(TEXT("Import / Export"));
    Profiles.AddCustomRow(FText::FromString(TEXT("Export Settings Profile"))).WholeRowContent()
        [MakeAction(TEXT("Export Settings Profile"), &UGPAAEditorSettings::ExportSettingsProfile)];
    Profiles.AddCustomRow(FText::FromString(TEXT("Import Settings Profile"))).WholeRowContent()
        [MakeAction(TEXT("Import Settings Profile"), &UGPAAEditorSettings::ImportSettingsProfile)];
    AddStatus(TEXT("Import / Export"), &UGPAAEditorSettings::LastProfileResult);
}
