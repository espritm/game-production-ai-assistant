// Copyright Game Production AI Assistant contributors 2026.
#include "UI/SGPAAssistantWidget.h"

#include "Application/GPAANativeRuntime.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#include "Editor.h"
#include "ISettingsModule.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Text/STextBlock.h"
PRAGMA_ENABLE_DEPRECATION_WARNINGS

#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Models/GPAAApiModels.h"
#include "Presentation/GPAAKnowledgePresenter.h"
#include "Services/GPAAAssetMetadataRequestBuilder.h"
#include "Services/GPAASourceOpener.h"
#include "Settings/GPAAEditorSettings.h"

#define LOCTEXT_NAMESPACE "SGPAAssistantWidget"

namespace
{
    const FString SourceOpenerAuto = TEXT("Auto");
    const FString SourceOpenerVSCode = TEXT("Visual Studio Code");
    const FString SourceOpenerRider = TEXT("Rider");
    const FString SourceOpenerVisualStudio = TEXT("Visual Studio");
    const FString SourceOpenerDefault = TEXT("Default Windows App");
    const FString SourceOpenerCustom = TEXT("Custom Command");
    const FString ChatProviderOllama = TEXT("Ollama");
    const FString ChatProviderOpenAICompatible = TEXT("OpenAICompatible");
    const FString ChatProviderEcho = TEXT("Echo");
    const FString SourceFilterAll = TEXT("All");
    const FString SearchResultFilterCode = TEXT("Code");
    const FString SearchResultFilterAssets = TEXT("Assets");
}

void SGPAAssistantWidget::Construct(const FArguments& InArgs)
{
    InitializeSourceOpenerOptions();
    InitializeChatProviderOptions();
    SearchResultFilterModes.Add(MakeShared<FString>(SourceFilterAll));
    SearchResultFilterModes.Add(MakeShared<FString>(SearchResultFilterCode));
    SearchResultFilterModes.Add(MakeShared<FString>(SearchResultFilterAssets));
    SelectedSearchResultFilterMode = SearchResultFilterModes[0];
    LoadPluginSettings();

    ChildSlot
    [
        SNew(SBorder)
        .Padding(12.0f)
        [
            SNew(SOverlay)

            + SOverlay::Slot()
            [
                SNew(SVerticalBox)

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, 0.0f, 0.0f, 8.0f)
                [
                    SNew(SBorder)
                    .Padding(8.0f)
                    [
                        SNew(SHorizontalBox)

                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .VAlign(VAlign_Center)
                        [
                            SAssignNew(SettingsSummaryText, STextBlock)
                            .Text(this, &SGPAAssistantWidget::GetSettingsSummaryText)
                            .AutoWrapText(true)
                        ]

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(0.0f, 0.0f, 8.0f, 0.0f)
                        [
                            SNew(SButton)
                            .Text(LOCTEXT("HistoryButton", "History"))
                            .OnClicked(this, &SGPAAssistantWidget::OnHistoryClicked)
                        ]

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        [
                            SNew(SButton)
                            .Text(LOCTEXT("OpenSettingsButton", "Settings"))
                            .OnClicked(this, &SGPAAssistantWidget::OnOpenSettingsClicked)
                        ]
                        + SHorizontalBox::Slot().AutoWidth().Padding(8,0,0,0)
                        [
                            SNew(SButton).Text(LOCTEXT("CancelNative", "Cancel"))
                            .OnClicked(this, &SGPAAssistantWidget::OnCancelClicked)
                        ]
                    ]
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, 0.0f, 0.0f, 8.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("RefreshKnowledgeButton", "Refresh Knowledge"))
                    .OnClicked(this, &SGPAAssistantWidget::OnRefreshKnowledgeClicked)
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, 0.0f, 0.0f, 10.0f)
                [
                    SNew(SBorder)
                    .Padding(8.0f)
                    [
                        SNew(SVerticalBox)

                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 0.0f, 0.0f, 4.0f)
                        [
                            SNew(STextBlock)
                            .Text(LOCTEXT("ProjectKnowledgeLabel", "Project Knowledge"))
                        ]

                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SAssignNew(KnowledgeText, STextBlock)
                            .Text(LOCTEXT("ProjectKnowledgeEmpty", "No local knowledge status loaded."))
                            .AutoWrapText(true)
                        ]
                    ]
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, 0.0f, 0.0f, 8.0f)
                [
                    SAssignNew(StatusText, STextBlock)
                    .Text(LOCTEXT("ReadyStatus", "Ready"))
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(0.0f, 0.0f, 8.0f, 0.0f)
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("PromptTabButton", "Prompt IA"))
                        .OnClicked(this, &SGPAAssistantWidget::OnPromptTabClicked)
                    ]

                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("SearchTabButton", "Search"))
                        .OnClicked(this, &SGPAAssistantWidget::OnSearchTabClicked)
                    ]
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, 8.0f, 0.0f, 8.0f)
                [
                    SNew(SSeparator)
                ]

                + SVerticalBox::Slot()
                .FillHeight(1.0f)
                [
                    SAssignNew(TabSwitcher, SWidgetSwitcher)
                    .WidgetIndex(0)

                    + SWidgetSwitcher::Slot()
                    [
                        SNew(SVerticalBox)

                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 0.0f, 0.0f, 4.0f)
                        [
                            SAssignNew(AnswerSummaryText, STextBlock)
                            .Text(LOCTEXT("AnswerLabel", "Ask answer"))
                        ]

                        + SVerticalBox::Slot()
                        .FillHeight(1.0f)
                        .Padding(0.0f, 0.0f, 0.0f, 10.0f)
                        [
                            SAssignNew(AnswerTextBox, SMultiLineEditableTextBox)
                            .IsReadOnly(true)
                            .AutoWrapText(true)
                            .Text(LOCTEXT("EmptyAnswer", ""))
                        ]

                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 0.0f, 0.0f, 4.0f)
                        [
                            SNew(STextBlock)
                            .Text(LOCTEXT("PromptQuestionLabel", "Question"))
                        ]

                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(SHorizontalBox)

                            + SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
                            [
                                SAssignNew(PromptQuestionTextBox, SEditableTextBox)
                                .Text(LOCTEXT("QuestionPlaceholder", "Where is stamina regeneration handled?"))
                            ]

                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            [
                                SNew(SButton)
                                .Text(LOCTEXT("AskButton", "Ask"))
                                .OnClicked(this, &SGPAAssistantWidget::OnAskClicked)
                            ]
                        ]
                    ]

                    + SWidgetSwitcher::Slot()
                    [
                        SNew(SVerticalBox)

                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 0.0f, 0.0f, 4.0f)
                        [
                            SNew(STextBlock)
                            .Text(LOCTEXT("SearchQueryLabel", "Search query"))
                        ]

                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 0.0f, 0.0f, 8.0f)
                        [
                            SNew(SHorizontalBox)

                            + SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
                            [
                                SAssignNew(SearchQueryTextBox, SEditableTextBox)
                                .Text(LOCTEXT("SearchPlaceholder", "Where is stamina regeneration handled?"))
                            ]

                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            [
                                SNew(SButton)
                                .Text(LOCTEXT("SearchButton", "Search"))
                                .OnClicked(this, &SGPAAssistantWidget::OnSearchClicked)
                            ]
                        ]

                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 0.0f, 0.0f, 4.0f)
                        [
                            SNew(STextBlock)
                            .Text(LOCTEXT("SearchFilterLabel", "Filter"))
                        ]

                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 0.0f, 0.0f, 8.0f)
                        [
                            SAssignNew(SearchResultFilterComboBox, SComboBox<TSharedPtr<FString>>)
                            .OptionsSource(&SearchResultFilterModes)
                            .InitiallySelectedItem(SelectedSearchResultFilterMode)
                            .OnSelectionChanged(this, &SGPAAssistantWidget::OnSearchResultFilterChanged)
                            .OnGenerateWidget(this, &SGPAAssistantWidget::GenerateSearchResultFilterWidget)
                            [
                                SNew(STextBlock)
                                .Text(this, &SGPAAssistantWidget::GetSelectedSearchResultFilterText)
                            ]
                        ]

                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 0.0f, 0.0f, 4.0f)
                        [
                            SAssignNew(SearchSummaryText, STextBlock)
                            .Text(LOCTEXT("SearchResultsLabel", "Search results"))
                        ]

                        + SVerticalBox::Slot()
                        .FillHeight(1.0f)
                        [
                            SAssignNew(SearchResultsListView, SListView<TSharedPtr<FGPAASourceReference>>)
                            .ListItemsSource(&FilteredSearchResults)
                            .OnGenerateRow(this, &SGPAAssistantWidget::GenerateSourceRow)
                            .SelectionMode(ESelectionMode::Single)
                        ]
                    ]
                ]
            ]

            + SOverlay::Slot()
            .HAlign(HAlign_Fill)
            .VAlign(VAlign_Fill)
            [
                SNew(SBorder)
                .Visibility(this, &SGPAAssistantWidget::GetHistoryVisibility)
                .Padding(12.0f)
                [
                    SNew(SVerticalBox)

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 0.0f, 0.0f, 8.0f)
                    [
                        SNew(SHorizontalBox)

                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                            .Text(LOCTEXT("HistoryPanelTitle", "History"))
                        ]

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        [
                            SNew(SButton)
                            .Text(LOCTEXT("CloseHistoryButton", "X"))
                            .OnClicked(this, &SGPAAssistantWidget::OnCloseHistoryClicked)
                        ]
                    ]

                    + SVerticalBox::Slot()
                    .FillHeight(1.0f)
                    [
                        SAssignNew(HistoryTextBox, SMultiLineEditableTextBox)
                        .IsReadOnly(true)
                        .AutoWrapText(true)
                        .Text(LOCTEXT("HistoryEmpty", "No history loaded."))
                    ]
                ]
            ]
        ]
    ];

    LoadChatSettings();
    LoadKnowledgeStatus();
}

FReply SGPAAssistantWidget::OnOpenSettingsClicked()
{
    if (ISettingsModule* SettingsModule = FModuleManager::LoadModulePtr<ISettingsModule>(TEXT("Settings")))
    {
        SettingsModule->ShowViewer(
            TEXT("Project"),
            TEXT("Plugins"),
            TEXT("GameProductionAIAssistant"));
    }

    LoadPluginSettings();
    return FReply::Handled();
}

FReply SGPAAssistantWidget::OnLoadChatSettingsClicked()
{
    SetBusy(TEXT("Loading chat settings..."));
    LoadChatSettings();
    return FReply::Handled();
}

FReply SGPAAssistantWidget::OnSaveChatSettingsClicked()
{
    SetBusy(FString::Printf(TEXT("Saving chat settings for %s..."), *GetSelectedChatProviderName()));
    SaveChatSettings();
    return FReply::Handled();
}

FReply SGPAAssistantWidget::OnRefreshKnowledgeClicked()
{
    StartNative(false, true); return FReply::Handled();
}

FReply SGPAAssistantWidget::OnHistoryClicked()
{
    bHistoryVisible = true; SetBusy(TEXT("Loading local history..."));
    TWeakPtr<SGPAAssistantWidget> Weak = SharedThis(this);
    FGPAANativeRuntime::Get()->LoadHistory(GetProjectRoot(), [Weak](bool Ok, const FString& Message)
    { if (auto Widget = Weak.Pin()) Widget->SetHistoryResult(Ok, Message); });
    return FReply::Handled();
}

FReply SGPAAssistantWidget::OnCloseHistoryClicked()
{
    bHistoryVisible = false;
    return FReply::Handled();
}

FReply SGPAAssistantWidget::OnPromptTabClicked()
{
    ActiveTabIndex = 0;
    if (TabSwitcher.IsValid())
    {
        TabSwitcher->SetActiveWidgetIndex(ActiveTabIndex);
    }

    return FReply::Handled();
}

FReply SGPAAssistantWidget::OnSearchTabClicked()
{
    ActiveTabIndex = 1;
    if (TabSwitcher.IsValid())
    {
        TabSwitcher->SetActiveWidgetIndex(ActiveTabIndex);
    }

    return FReply::Handled();
}

FReply SGPAAssistantWidget::OnSearchClicked()
{
    StartNative(false, false); return FReply::Handled();
}

FReply SGPAAssistantWidget::OnAskClicked()
{
    StartNative(true, false); return FReply::Handled();
}

SGPAAssistantWidget::~SGPAAssistantWidget()
{
    FGPAANativeRuntime::CancelIfRunning();
}

FReply SGPAAssistantWidget::OnCancelClicked()
{
    NativeRequestId = FGuid::NewGuid();
    FGPAANativeRuntime::Get()->Cancel(); StopBusy();
    StatusText->SetText(FText::FromString(TEXT("Operation cancelled. Any partial response is kept in local history.")));
    return FReply::Handled();
}

void SGPAAssistantWidget::StartNative(bool Ask, bool RefreshOnly)
{
    LoadPluginSettings();
    const FString Query = Ask ? GetPromptQuestion() : GetSearchQuery();
    if (!RefreshOnly && Query.TrimStartAndEnd().IsEmpty()) { SetResult(false, TEXT("Enter a question or search query first.")); return; }
    NativeRequestId = FGuid::NewGuid(); const FGuid RequestId = NativeRequestId;
    TWeakPtr<SGPAAssistantWidget> Weak = SharedThis(this);
    NativeAnswer.Reset(); SetBusy(TEXT("Preparing local project knowledge..."));
    if (Ask) AnswerTextBox->SetText(FText::GetEmpty());
    FGPAANativeRuntime::Get()->Run(GetProjectRoot(), BuildAssetMetadataSyncRequest(), Query, Ask, RefreshOnly,
        [Weak, RequestId](const FString& Message) { if (auto W = Weak.Pin()) if (W->NativeRequestId == RequestId) W->SetBusy(Message); },
        [Weak, RequestId](const FString& Delta) { if (auto W = Weak.Pin()) if (W->NativeRequestId == RequestId) { W->NativeAnswer += Delta; W->AnswerTextBox->SetText(FText::FromString(W->NativeAnswer)); } },
        [Weak, RequestId, Ask, RefreshOnly](bool Ok, const FString& Message, const FGPAANativeResult& Result)
        {
            auto W = Weak.Pin(); if (!W || W->NativeRequestId != RequestId) return;
            W->UpdateKnowledgeSummary(Result.Statistics);
            if (RefreshOnly) W->SetResult(Ok, Message);
            else if (Ask)
            {
                W->SetChatResult(Ok, Message, Result.Answer, Result.Sources);
                if (!Ok && !Result.Answer.IsEmpty()) W->AnswerTextBox->SetText(FText::FromString(Result.Answer + TEXT("\n\n[Interrupted] ") + Message));
            }
            else W->SetSearchResult(Ok, Message, Result.Sources);
        });
}

FString SGPAAssistantWidget::BuildAssetMetadataSyncRequest() const
{
    return FGPAAAssetMetadataRequestBuilder::BuildSyncRequest(GetProjectRoot());
}

void SGPAAssistantWidget::SetBusy(const FString& Message)
{
    BusyBaseMessage = Message.TrimEnd();
    BusyFrame = 0;
    bBusy = true;
    StatusText->SetText(FText::FromString(BusyBaseMessage + TEXT(" .")));
    RegisterActiveTimer(0.35f, FWidgetActiveTimerDelegate::CreateSP(this, &SGPAAssistantWidget::HandleBusyTick));
}

void SGPAAssistantWidget::SetBusyFromWorkflow(bool bSucceeded, const FString& Message)
{
    if (bSucceeded)
    {
        SetBusy(Message);
    }
}

void SGPAAssistantWidget::SetResult(bool bSucceeded, const FString& Message)
{
    StopBusy();
    StatusText->SetText(FText::FromString(bSucceeded ? TEXT("Done") : TEXT("Error")));
    AnswerTextBox->SetText(FText::FromString(Message));
}

void SGPAAssistantWidget::SetHistoryResult(bool bSucceeded, const FString& Message)
{
    StopBusy();
    StatusText->SetText(FText::FromString(bSucceeded ? TEXT("History loaded") : TEXT("Error")));
    if (HistoryTextBox.IsValid())
    {
        HistoryTextBox->SetText(FText::FromString(Message));
    }
}

void SGPAAssistantWidget::SetSearchResult(bool bSucceeded, const FString& Message, const TArray<FGPAASourceReference>& Results)
{
    StopBusy();
    StatusText->SetText(FText::FromString(bSucceeded ? TEXT("Search complete") : TEXT("Error")));
    SearchSummaryText->SetText(FText::FromString(Message));

    SearchResults.Reset();
    if (bSucceeded)
    {
        for (const FGPAASourceReference& Result : Results)
        {
            SearchResults.Add(MakeShared<FGPAASourceReference>(Result));
        }
    }

    RebuildFilteredSearchResults();
    RefreshSourceLists();
}

void SGPAAssistantWidget::SetChatResult(bool bSucceeded, const FString& Message, const FString& Answer, const TArray<FGPAASourceReference>& Sources)
{
    StopBusy();
    StatusText->SetText(FText::FromString(bSucceeded
        ? FString::Printf(TEXT("Answer complete via %s"), *GetSelectedChatProviderName())
        : TEXT("Error")));
    AnswerSummaryText->SetText(FText::FromString(Message));
    AnswerTextBox->SetText(FText::FromString(bSucceeded ? Answer : Message));

    AnswerSources.Reset();
    if (bSucceeded)
    {
        for (const FGPAASourceReference& Source : Sources)
        {
            AnswerSources.Add(MakeShared<FGPAASourceReference>(Source));
        }
    }

    RefreshSourceLists();
}

EActiveTimerReturnType SGPAAssistantWidget::HandleBusyTick(double InCurrentTime, float InDeltaTime)
{
    if (!bBusy || !StatusText.IsValid())
    {
        return EActiveTimerReturnType::Stop;
    }

    BusyFrame = (BusyFrame + 1) % 3;
    const int32 DotCount = BusyFrame + 1;
    StatusText->SetText(FText::FromString(BusyBaseMessage + TEXT(" ") + FString::ChrN(DotCount, TEXT('.'))));
    return EActiveTimerReturnType::Continue;
}

void SGPAAssistantWidget::StopBusy()
{
    bBusy = false;
}

TSharedRef<ITableRow> SGPAAssistantWidget::GenerateSourceRow(TSharedPtr<FGPAASourceReference> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
    const FString SourceType = Item.IsValid() && !Item->SourceType.IsEmpty() ? Item->SourceType : TEXT("Code");
    const FString Location = Item.IsValid()
        ? FString::Printf(TEXT("[%s] %s:%d-%d (score %.3f)"), *SourceType, *Item->RelativePath, Item->StartLine, Item->EndLine, Item->Score)
        : TEXT("Unknown source");

    const FString Snippet = Item.IsValid() ? Item->Content : FString();

    return SNew(STableRow<TSharedPtr<FGPAASourceReference>>, OwnerTable)
        [
            SNew(SBorder)
            .Padding(6.0f)
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNew(SVerticalBox)

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(Location))
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 4.0f, 0.0f, 0.0f)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(Snippet))
                        .AutoWrapText(true)
                    ]
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(8.0f, 0.0f, 0.0f, 0.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("OpenSourceButton", "Open"))
                    .OnClicked(this, &SGPAAssistantWidget::OnOpenSourceClicked, Item)
                ]
            ]
        ];
}

FReply SGPAAssistantWidget::OnOpenSourceClicked(TSharedPtr<FGPAASourceReference> Item)
{
    if (!Item.IsValid())
    {
        StatusText->SetText(LOCTEXT("InvalidSourceStatus", "Invalid source."));
        return FReply::Handled();
    }

    const FString Mode = GetDefault<UGPAAEditorSettings>()->SourceOpenerMode;
    FString Status;
    FGPAASourceOpener::OpenSource(*Item, GetProjectRoot(), Mode, GetSourceOpenerCommand(), Status);
    StatusText->SetText(FText::FromString(Status));
    return FReply::Handled();
}

void SGPAAssistantWidget::InitializeSourceOpenerOptions()
{
    SourceOpenerModes.Reset();
    SourceOpenerModes.Add(MakeShared<FString>(SourceOpenerAuto));
    SourceOpenerModes.Add(MakeShared<FString>(SourceOpenerVSCode));
    SourceOpenerModes.Add(MakeShared<FString>(SourceOpenerRider));
    SourceOpenerModes.Add(MakeShared<FString>(SourceOpenerVisualStudio));
    SourceOpenerModes.Add(MakeShared<FString>(SourceOpenerDefault));
    SourceOpenerModes.Add(MakeShared<FString>(SourceOpenerCustom));
    SelectedSourceOpenerMode = SourceOpenerModes[0];
}

void SGPAAssistantWidget::InitializeChatProviderOptions()
{
    ChatProviderModes.Reset();
    ChatProviderModes.Add(MakeShared<FString>(TEXT("OpenAI")));
    ChatProviderModes.Add(MakeShared<FString>(ChatProviderOpenAICompatible));
    SelectedChatProviderMode=ChatProviderModes[0];
}

void SGPAAssistantWidget::LoadPluginSettings()
{
    const UGPAAEditorSettings* Settings = GetDefault<UGPAAEditorSettings>();

    auto SelectStringOption = [](const TArray<TSharedPtr<FString>>& Options, const FString& Value, TSharedPtr<FString>& SelectedOption)
    {
        for (const TSharedPtr<FString>& Candidate : Options)
        {
            if (Candidate.IsValid() && Candidate->Equals(Value, ESearchCase::IgnoreCase))
            {
                SelectedOption = Candidate;
                return;
            }
        }
    };

    SelectStringOption(SourceOpenerModes, Settings->SourceOpenerMode, SelectedSourceOpenerMode);
    SelectStringOption(ChatProviderModes, Settings->ChatProvider, SelectedChatProviderMode);
    CustomSourceOpenerCommand = Settings->CustomSourceOpenerCommand;

    if (SourceOpenerComboBox.IsValid())
    {
        SourceOpenerComboBox->SetSelectedItem(SelectedSourceOpenerMode);
    }

    if (ChatProviderComboBox.IsValid())
    {
        ChatProviderComboBox->SetSelectedItem(SelectedChatProviderMode);
    }

    if (SettingsSummaryText.IsValid())
    {
        SettingsSummaryText->SetText(GetSettingsSummaryText());
    }
}

void SGPAAssistantWidget::SavePluginSettings() const
{
    UGPAAEditorSettings* Settings = GetMutableDefault<UGPAAEditorSettings>();
    Settings->SourceOpenerMode = SelectedSourceOpenerMode.IsValid() ? *SelectedSourceOpenerMode : SourceOpenerAuto;
    Settings->CustomSourceOpenerCommand = GetSourceOpenerCommand();
    Settings->ChatProvider = SelectedChatProviderMode.IsValid() ? *SelectedChatProviderMode : Settings->ChatProvider;
    Settings->SaveConfig();
}


void SGPAAssistantWidget::LoadChatSettings()
{
    LoadPluginSettings(); SetResult(true, TEXT("Provider settings loaded locally."));
}

void SGPAAssistantWidget::SaveChatSettings()
{
    SavePluginSettings(); SetResult(true, TEXT("Provider settings saved. Personal keys are managed in Settings."));
}

void SGPAAssistantWidget::ApplyChatSettingsResponse(bool bSucceeded, const FString& Message)
{
    if (!bSucceeded)
    {
        StatusText->SetText(FText::FromString(TEXT("Chat settings unavailable")));
        return;
    }

    FGPAAChatSettings Settings;
    if (!FGPAAApiModels::ParseChatSettings(Message, Settings))
    {
        StatusText->SetText(FText::FromString(TEXT("Invalid chat settings response")));
        return;
    }

    GetMutableDefault<UGPAAEditorSettings>()->ApplyChatSettings(Settings);

    for (const TSharedPtr<FString>& Candidate : ChatProviderModes)
    {
        if (Candidate.IsValid() && Candidate->Equals(Settings.Provider, ESearchCase::IgnoreCase))
        {
            SelectedChatProviderMode = Candidate;
            if (ChatProviderComboBox.IsValid())
            {
                ChatProviderComboBox->SetSelectedItem(Candidate);
            }
            break;
        }
    }

    StatusText->SetText(FText::FromString(FString::Printf(TEXT("Chat provider active: %s"), *GetSelectedChatProviderName())));
    if (SettingsSummaryText.IsValid())
    {
        SettingsSummaryText->SetText(GetSettingsSummaryText());
    }
}

void SGPAAssistantWidget::LoadKnowledgeStatus()
{
    KnowledgeText->SetText(FText::FromString(TEXT("First use: Settings > choose provider and model > Save Personal Key > Cloud Context Consent. Then Refresh Knowledge and Ask. API usage is billed by your provider. Search works offline.")));
}

void SGPAAssistantWidget::ApplyKnowledgeStatusResponse(bool bSucceeded, const FString& Message)
{
    if (!KnowledgeText.IsValid())
    {
        return;
    }

    if (!bSucceeded)
    {
        KnowledgeText->SetText(FText::FromString(TEXT("Project knowledge status unavailable.")));
        return;
    }

    FGPAAKnowledgeStatus Status;
    if (!FGPAAApiModels::ParseKnowledgeStatus(Message, Status))
    {
        KnowledgeText->SetText(FText::FromString(TEXT("Invalid project knowledge response.")));
        return;
    }

    KnowledgeText->SetText(FText::FromString(FGPAAKnowledgePresenter::FormatKnowledgeStatus(Status)));
}

void SGPAAssistantWidget::UpdateKnowledgeSummaryFromIndexResponse(const FString& Message)
{
    if (!KnowledgeText.IsValid())
    {
        return;
    }

    FGPAAIndexStatistics Statistics;
    if (FGPAAApiModels::ParseIndexStatisticsResponse(Message, Statistics))
    {
        UpdateKnowledgeSummary(Statistics);
    }
}

void SGPAAssistantWidget::UpdateKnowledgeSummary(const FGPAAIndexStatistics& Statistics)
{
    if (KnowledgeText.IsValid())
    {
        KnowledgeText->SetText(FText::FromString(FGPAAKnowledgePresenter::FormatIndexSummary(Statistics)));
    }
}

FGPAAChatSettings SGPAAssistantWidget::GetChatSettingsFromUi() const
{
    return GetDefault<UGPAAEditorSettings>()->ToChatSettings();
}

void SGPAAssistantWidget::OnSourceOpenerModeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
    if (NewSelection.IsValid())
    {
        SelectedSourceOpenerMode = NewSelection;
        SavePluginSettings();
    }
}

void SGPAAssistantWidget::OnChatProviderChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
    if (NewSelection.IsValid())
    {
        SelectedChatProviderMode = NewSelection;
        SavePluginSettings();
        if (StatusText.IsValid())
        {
            StatusText->SetText(FText::FromString(FString::Printf(TEXT("Chat provider selected: %s"), *GetSelectedChatProviderName())));
        }
    }
}

void SGPAAssistantWidget::OnSearchResultFilterChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
    if (NewSelection.IsValid())
    {
        SelectedSearchResultFilterMode = NewSelection;
    }

    RebuildFilteredSearchResults();
    RefreshSourceLists();
}

void SGPAAssistantWidget::OnCustomSourceOpenerCommitted(const FText& Text, ETextCommit::Type CommitType)
{
    CustomSourceOpenerCommand = Text.ToString();
    SavePluginSettings();
}

TSharedRef<SWidget> SGPAAssistantWidget::GenerateSourceOpenerModeWidget(TSharedPtr<FString> Item) const
{
    return SNew(STextBlock)
        .Text(FText::FromString(Item.IsValid() ? *Item : SourceOpenerAuto));
}

TSharedRef<SWidget> SGPAAssistantWidget::GenerateChatProviderWidget(TSharedPtr<FString> Item) const
{
    return SNew(STextBlock)
        .Text(FText::FromString(Item.IsValid() ? *Item : ChatProviderOllama));
}

TSharedRef<SWidget> SGPAAssistantWidget::GenerateSearchResultFilterWidget(TSharedPtr<FString> Item) const
{
    return SNew(STextBlock)
        .Text(FText::FromString(Item.IsValid() ? *Item : SourceFilterAll));
}

FText SGPAAssistantWidget::GetSelectedSourceOpenerModeText() const
{
    return FText::FromString(SelectedSourceOpenerMode.IsValid() ? *SelectedSourceOpenerMode : SourceOpenerAuto);
}

FText SGPAAssistantWidget::GetSelectedChatProviderText() const
{
    return FText::FromString(SelectedChatProviderMode.IsValid() ? *SelectedChatProviderMode : ChatProviderOllama);
}

FText SGPAAssistantWidget::GetSelectedSearchResultFilterText() const
{
    return FText::FromString(SelectedSearchResultFilterMode.IsValid() ? *SelectedSearchResultFilterMode : SourceFilterAll);
}

FText SGPAAssistantWidget::GetSettingsSummaryText() const
{
    const auto* Settings=GetDefault<UGPAAEditorSettings>();
    return FText::FromString(FString::Printf(TEXT("Provider: %s | Model: %s | Search: local"),*Settings->ChatProvider,*Settings->OpenAIModel));
}

EVisibility SGPAAssistantWidget::GetHistoryVisibility() const
{
    return bHistoryVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SGPAAssistantWidget::GetCustomSourceOpenerVisibility() const
{
    return SelectedSourceOpenerMode.IsValid() && *SelectedSourceOpenerMode == SourceOpenerCustom
        ? EVisibility::Visible
        : EVisibility::Collapsed;
}

EVisibility SGPAAssistantWidget::GetOpenAICompatibleSettingsVisibility() const
{
    return SelectedChatProviderMode.IsValid() && *SelectedChatProviderMode == ChatProviderOpenAICompatible
        ? EVisibility::Visible
        : EVisibility::Collapsed;
}


FString SGPAAssistantWidget::GetProjectRoot() const
{
    return GetDefault<UGPAAEditorSettings>()->GetResolvedProjectRoot();
}

FString SGPAAssistantWidget::GetPromptQuestion() const
{
    return PromptQuestionTextBox.IsValid() ? PromptQuestionTextBox->GetText().ToString() : FString();
}

FString SGPAAssistantWidget::GetSearchQuery() const
{
    return SearchQueryTextBox.IsValid() ? SearchQueryTextBox->GetText().ToString() : FString();
}

FString SGPAAssistantWidget::GetSelectedChatProviderName() const
{
    return GetDefault<UGPAAEditorSettings>()->ChatProvider;
}

FString SGPAAssistantWidget::GetSelectedSourceFilterName() const
{
    return SourceFilterAll;
}

FString SGPAAssistantWidget::GetSourceOpenerCommand() const
{
    return GetDefault<UGPAAEditorSettings>()->CustomSourceOpenerCommand;
}

void SGPAAssistantWidget::RebuildFilteredSearchResults()
{
    FilteredSearchResults.Reset();

    const FString Filter = SelectedSearchResultFilterMode.IsValid()
        ? *SelectedSearchResultFilterMode
        : SourceFilterAll;

    for (const TSharedPtr<FGPAASourceReference>& Result : SearchResults)
    {
        if (!Result.IsValid())
        {
            continue;
        }

        const bool bIsAsset = Result->SourceType.Equals(TEXT("Asset"), ESearchCase::IgnoreCase)
            || Result->SourceType.Equals(SearchResultFilterAssets, ESearchCase::IgnoreCase);
        const bool bShouldShow =
            Filter.Equals(SourceFilterAll, ESearchCase::IgnoreCase)
            || (Filter.Equals(SearchResultFilterCode, ESearchCase::IgnoreCase) && !bIsAsset)
            || (Filter.Equals(SearchResultFilterAssets, ESearchCase::IgnoreCase) && bIsAsset);

        if (bShouldShow)
        {
            FilteredSearchResults.Add(Result);
        }
    }
}

void SGPAAssistantWidget::RefreshSourceLists()
{
    if (SearchResultsListView.IsValid())
    {
        SearchResultsListView->RequestListRefresh();
    }

}

#undef LOCTEXT_NAMESPACE
