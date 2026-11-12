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
#include "Styling/AppStyle.h"
#include "Widgets/Input/SCheckBox.h"
#include "Application/GPAAQuickStart.h"

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
    auto Help = [](const FText& Text) -> TSharedRef<SWidget>
    { return SNew(SImage).Image(FCoreStyle::Get().GetBrush(TEXT("Icons.Help"))).ToolTipText(Text); };
    auto Radio = [Settings](const TCHAR* Label, bool Local) -> TSharedRef<SWidget>
    {
        return SNew(SCheckBox).Style(FAppStyle::Get(), "RadioButton")
            .IsEnabled_Lambda([]() { return !FGPAAQuickStart::IsRunning(); })
            .IsChecked_Lambda([Settings, Local]()
            { return Settings.IsValid() && (Local ? Settings->ChatProvider == TEXT("Local") :
                Settings->ChatProvider == TEXT("OpenAI") || Settings->ChatProvider == TEXT("OpenAICompatible"))
                ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
            .OnCheckStateChanged_Lambda([Settings, Local](ECheckBoxState State)
            { if (Settings.IsValid() && State == ECheckBoxState::Checked) Settings->SelectAI(Local); })
            [SNew(STextBlock).Text(FText::FromString(Label))];
    };
    auto& Setup = Builder.EditCategory(TEXT("AI Setup")); Setup.SetSortOrder(0);
    Setup.AddCustomRow(FText::FromString(TEXT("Local AI Cloud AI Provider"))).WholeRowContent()
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()[Radio(TEXT("Local AI"), true)]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6,0,20,0)
                [Help(NSLOCTEXT("GPAASettings", "LocalChoiceHelp", "Generate answers on this computer using a downloaded model. No API key is needed and project context stays local. Download a model in Local AI, or use Quick Start in the assistant panel."))]
            + SHorizontalBox::Slot().AutoWidth()[Radio(TEXT("Cloud AI Provider"), false)]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6,0)
                [Help(NSLOCTEXT("GPAASettings", "CloudChoiceHelp", "Selecting Cloud AI Provider authorizes sending your questions and relevant project excerpts (code and asset metadata) to the configured provider. A personal API key is required; provider charges may apply. Search remains local."))]];
    Setup.AddCustomRow(FText::FromString(TEXT("Cloud context authorization"))).WholeRowContent()
        [SNew(STextBlock).AutoWrapText(true).Text(NSLOCTEXT("GPAASettings", "CloudChoiceNotice",
            "Selecting Cloud AI Provider authorizes sending questions and relevant project excerpts to that provider. API charges may apply."))];
    Builder.EditCategory(TEXT("Cloud AI Provider")).SetSortOrder(1);
    Builder.EditCategory(TEXT("Local AI")).SetSortOrder(2);
    auto& Project = Builder.EditCategory(TEXT("Project")); Project.SetSortOrder(3);
    const auto Root = Builder.GetProperty(GET_MEMBER_NAME_CHECKED(UGPAAEditorSettings, ProjectRootOverride));
    Project.AddProperty(Root).CustomWidget().NameContent()[Root->CreatePropertyNameWidget()]
        .ValueContent().MinDesiredWidth(320)
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1)[Root->CreatePropertyValueWidget()]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6,0)
                [Help(NSLOCTEXT("GPAASettings", "ProjectRootHelp", "Leave empty to search and index the current Unreal project. Override this with a folder path to use another project root for code indexing and search. Asset metadata is available only when it belongs to the open Unreal project. The selected root also determines the local index and history location."))]];
    Builder.EditCategory(TEXT("Import / Export")).SetSortOrder(4);
    Builder.EditCategory(TEXT("Source Opener")).SetSortOrder(5);
    auto MakeAction = [Settings, Utilities](const TCHAR* Label, void (UGPAAEditorSettings::*Action)())
    {
        return SNew(SButton).Text(FText::FromString(Label))
            .IsEnabled_Lambda([]() { return !FGPAAQuickStart::IsRunning(); })
            .OnClicked_Lambda([Settings, Utilities, Action]()
            {
                if (Settings.IsValid()) (Settings.Get()->*Action)();
                if (const auto Pinned = Utilities.Pin()) Pinned->RequestRefresh();
                return FReply::Handled();
            });
    };
    const auto Key = Builder.GetProperty(GET_MEMBER_NAME_CHECKED(UGPAAEditorSettings, OpenAIApiKey));
    Builder.EditCategory(TEXT("Cloud AI Provider")).AddProperty(Key).CustomWidget()
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
    AddStatus(TEXT("Cloud AI Provider"), &UGPAAEditorSettings::LastKeyResult);
    auto& Local = Builder.EditCategory(TEXT("Local AI"));
    const auto ModelId = Builder.GetProperty(GET_MEMBER_NAME_CHECKED(UGPAAEditorSettings, LocalModelId));
    const auto ModelSelector = ModelId->CreatePropertyValueWidget();
    ModelSelector->SetEnabled(TAttribute<bool>::CreateLambda([]() { return !FGPAALocalModels::Get().IsBusy() && !FGPAAQuickStart::IsRunning(); }));
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
    DownloadButton->SetEnabled(TAttribute<bool>::CreateLambda([]() { return !FGPAALocalModels::Get().IsBusy() && !FGPAAQuickStart::IsRunning(); }));
    CancelButton->SetEnabled(TAttribute<bool>::CreateLambda([]() { return FGPAALocalModels::Get().IsBusy() && !FGPAAQuickStart::IsRunning(); }));
    RemoveButton->SetEnabled(TAttribute<bool>::CreateLambda([]() { return !FGPAALocalModels::Get().IsBusy() && !FGPAALocalChat::IsAnyBusy() && !FGPAAQuickStart::IsRunning(); }));
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
