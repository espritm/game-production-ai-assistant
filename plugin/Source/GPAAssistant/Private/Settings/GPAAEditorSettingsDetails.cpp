// Copyright Game Production AI Assistant contributors 2026.
#include "Settings/GPAAEditorSettingsDetails.h"
#include "Settings/GPAAEditorSettings.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"
#include "IDetailPropertyRow.h"
#include "Widgets/SBoxPanel.h"
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
    auto& Profiles = Builder.EditCategory(TEXT("Import / Export"));
    Profiles.AddCustomRow(FText::FromString(TEXT("Export Settings Profile"))).WholeRowContent()
        [MakeAction(TEXT("Export Settings Profile"), &UGPAAEditorSettings::ExportSettingsProfile)];
    Profiles.AddCustomRow(FText::FromString(TEXT("Import Settings Profile"))).WholeRowContent()
        [MakeAction(TEXT("Import Settings Profile"), &UGPAAEditorSettings::ImportSettingsProfile)];
    AddStatus(TEXT("Import / Export"), &UGPAAEditorSettings::LastProfileResult);
}
