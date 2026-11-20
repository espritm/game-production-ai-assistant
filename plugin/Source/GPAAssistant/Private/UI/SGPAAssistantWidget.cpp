// Copyright Game Production AI Assistant contributors 2026.
#include "UI/SGPAAssistantWidget.h"

#include "Application/GPAANativeRuntime.h"
#include "Application/GPAAQuickStart.h"
#include "UI/SGPAAQuickStartView.h"
#include "UI/SGPAAConversationView.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#include "Editor.h"
#include "ISettingsModule.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Input/SCheckBox.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
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
const FString SourceFilterAll = TEXT("All");
const FString SearchResultFilterCode = TEXT("Code");
const FString SearchResultFilterAssets = TEXT("Assets");
} // namespace

// Preserve Slate declarative layout; clang-format treats widget slots as C++ operators.
// clang-format off
void SGPAAssistantWidget::Construct(const FArguments& InArgs)
{

    ChildSlot
    [
        SNew(SBorder)
        .Padding(12.0f)
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
                        [
                            SNew(SButton)
                            .Text(LOCTEXT("OpenSettingsButton", "Settings"))
                            .OnClicked(this, &SGPAAssistantWidget::OnOpenSettingsClicked)
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

                + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)
                [SAssignNew(QuickStartView, SGPAAQuickStartView)]

                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SBox)
                    .HAlign(HAlign_Left)
                    [
                        SAssignNew(ViewSelector, SSegmentedControl<int32>)
                        .Value_Lambda([this]() { return ActiveTabIndex; })
                        .SupportsMultiSelection(false)
                        .SupportsEmptySelection(false)
                        .OnValueChanged(this, &SGPAAssistantWidget::OnViewSelectionChanged)
                        + SSegmentedControl<int32>::Slot(0)
                        .Text(LOCTEXT("PromptTabButton", "Prompt IA"))
                        + SSegmentedControl<int32>::Slot(1)
                        .Text(LOCTEXT("SearchTabButton", "Search"))
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
                        .FillHeight(1.0f)
                        .Padding(0.0f, 0.0f, 0.0f, 10.0f)
                        [
                            SAssignNew(Conversation, SGPAAConversationView)
                        ]

                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SAssignNew(PromptInputRow, SHorizontalBox)

                            + SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
                            [
                                SNew(SBox).MinDesiredHeight(64).MaxDesiredHeight(160)
                                [SAssignNew(PromptQuestionTextBox, SMultiLineEditableTextBox)
                                .HintText(LOCTEXT("QuestionPlaceholder", "Prompt your AI here"))
                                .AutoWrapText(true)
                                .ModiferKeyForNewLine(EModifierKey::Shift)
                                .ToolTipText(LOCTEXT("PromptKeyboardHelp", "Enter to send. Shift+Enter for a new line."))
                                .OnTextCommitted_Lambda([this](const FText&, ETextCommit::Type CommitType)
                                {
                                    if (CommitType == ETextCommit::OnEnter) OnAskClicked();
                                })]
                            ]

                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Bottom)
                            [
                                SAssignNew(SendButton, SButton)
                                .Text(LOCTEXT("SendButton", "Send"))
                                .OnClicked(this, &SGPAAssistantWidget::OnAskClicked)
                            ]
                            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(8,0,0,0)
                            [
                                SAssignNew(PromptCancelButton, SButton)
                                .Text(LOCTEXT("CancelRequestButton", "Cancel"))
                                .Visibility(this, &SGPAAssistantWidget::GetRequestCancelVisibility, true)
                                .OnClicked(this, &SGPAAssistantWidget::OnCancelClicked)
                            ]
                        ]
                    ]

                    + SWidgetSwitcher::Slot()
                    [
                        SNew(SVerticalBox)

                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 0.0f, 0.0f, 8.0f)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                            [SNew(STextBlock).Text(LOCTEXT("SearchScopeLabel", "Search in"))]
                            + SHorizontalBox::Slot().AutoWidth().Padding(12,0,0,0)
                            [MakeSearchScopeRadio(LOCTEXT("AssetScope", "Asset"), SearchResultFilterAssets)]
                            + SHorizontalBox::Slot().AutoWidth().Padding(12,0,0,0)
                            [MakeSearchScopeRadio(LOCTEXT("CodeScope", "Code"), SearchResultFilterCode)]
                            + SHorizontalBox::Slot().AutoWidth().Padding(12,0,0,0)
                            [MakeSearchScopeRadio(LOCTEXT("CombinedScope", "Asset and Code"), SourceFilterAll)]
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
                                .HintText(LOCTEXT("SearchPlaceholder", "Search"))
                            ]

                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            [
                                SNew(SButton)
                                .Text(LOCTEXT("SearchButton", "Search"))
                                .OnClicked(this, &SGPAAssistantWidget::OnSearchClicked)
                            ]
                            + SHorizontalBox::Slot().AutoWidth().Padding(8,0,0,0)
                            [
                                SAssignNew(SearchCancelButton, SButton)
                                .Text(LOCTEXT("CancelRequestButton", "Cancel"))
                                .Visibility(this, &SGPAAssistantWidget::GetRequestCancelVisibility, false)
                                .OnClicked(this, &SGPAAssistantWidget::OnCancelClicked)
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
    ];

    SetResult(true, TEXT("Provider settings loaded locally."));
}

// clang-format on

FReply SGPAAssistantWidget::OnOpenSettingsClicked()
{
    if (ISettingsModule *SettingsModule = FModuleManager::LoadModulePtr<ISettingsModule>(TEXT("Settings")))
    {
        SettingsModule->ShowViewer(TEXT("Project"), TEXT("Plugins"), TEXT("GameProductionAIAssistant"));
    }

    return FReply::Handled();
}

void SGPAAssistantWidget::OnViewSelectionChanged(int32 Index)
{
    if (Index != 0 && Index != 1)
        return;
    ActiveTabIndex = Index;
    if (TabSwitcher.IsValid())
        TabSwitcher->SetActiveWidgetIndex(Index);
}

FReply SGPAAssistantWidget::OnPromptTabClicked()
{
    OnViewSelectionChanged(0);
    return FReply::Handled();
}

FReply SGPAAssistantWidget::OnSearchTabClicked()
{
    OnViewSelectionChanged(1);
    return FReply::Handled();
}

FReply SGPAAssistantWidget::OnSearchClicked()
{
    StartNative(false);
    return FReply::Handled();
}

FReply SGPAAssistantWidget::OnAskClicked()
{
    StartNative(true);
    return FReply::Handled();
}

SGPAAssistantWidget::~SGPAAssistantWidget()
{
    if (bBusy)
        FGPAANativeRuntime::CancelIfRunning();
}

FReply SGPAAssistantWidget::OnCancelClicked()
{
    if (!bCancellable)
        return FReply::Handled();
    if (bRequestIsAsk && Conversation->HasActiveTurn())
        FinishPromptTurn(NativeAnswer + TEXT("\n[Cancelled]"));
    NativeRequestId = FGuid::NewGuid();
    FGPAANativeRuntime::CancelIfRunning();
    StopBusy();
    StatusText->SetText(FText::FromString(TEXT("Operation cancelled. Any partial response is kept in local history.")));
    return FReply::Handled();
}

void SGPAAssistantWidget::StartNative(bool Ask)
{
    if (bBusy)
    {
        StatusText->SetText(
            LOCTEXT("RequestStillRunning", "A request is already running. Wait for it to finish or cancel it first."));
        return;
    }
    if (FGPAAQuickStart::IsRunning())
    {
        SetResult(false, TEXT("Wait for Quick Start to finish, or cancel it first."));
        return;
    }
    const FString Query = Ask ? GetPromptQuestion() : GetSearchQuery();
    if (Query.TrimStartAndEnd().IsEmpty())
    {
        SetResult(false, TEXT("Enter a question or search query first."));
        return;
    }
    NativeRequestId = FGuid::NewGuid();
    const FGuid RequestId = NativeRequestId;
    TWeakPtr<SGPAAssistantWidget> Weak = SharedThis(this);
    bCancellable = true;
    bRequestIsAsk = Ask;
    if (Ask)
        ActiveChatProviderName = GetSelectedChatProviderName();
    NativeAnswer.Reset();
    SetBusy(TEXT("Preparing local project knowledge..."));
    if (Ask)
        BeginPromptTurn(Query);
    FGPAANativeRuntime::Get()->Run(
        GetProjectRoot(), BuildAssetMetadataSyncRequest(), Query, Ask, false,
        [Weak, RequestId](const FString &Message)
        {
            if (auto W = Weak.Pin())
                if (W->NativeRequestId == RequestId)
                    W->SetBusy(Message);
        },
        [Weak, RequestId](const FString &Delta)
        {
            if (auto W = Weak.Pin())
                if (W->NativeRequestId == RequestId)
                {
                    W->NativeAnswer += Delta;
                    W->RenderPromptTurn();
                }
        },
        [Weak, RequestId, Ask](bool Ok, const FString &Message, const FGPAANativeResult &Result)
        {
            auto W = Weak.Pin();
            if (!W || W->NativeRequestId != RequestId)
                return;
            if (Ask)
            {
                W->SetChatResult(Ok, Message, Result.Answer, Result.Sources);
            }
            else
                W->SetSearchResult(Ok, Message, Result.Sources);
        },
        Ask ? SourceFilterAll : GetSelectedSourceFilterName());
}

FString SGPAAssistantWidget::BuildAssetMetadataSyncRequest() const
{
    return FGPAAAssetMetadataRequestBuilder::BuildSyncRequest(GetProjectRoot());
}

void SGPAAssistantWidget::SetBusy(const FString &Message)
{
    BusyBaseMessage = Message.TrimEnd();
    BusyFrame = 0;
    bBusy = true;
    StatusText->SetText(FText::FromString(BusyBaseMessage + TEXT(" .")));
    RegisterActiveTimer(0.35f, FWidgetActiveTimerDelegate::CreateSP(this, &SGPAAssistantWidget::HandleBusyTick));
}

void SGPAAssistantWidget::SetResult(bool bSucceeded, const FString &Message)
{
    StopBusy();
    StatusText->SetText(FText::FromString(bSucceeded ? TEXT("Ready") : Message));
    StatusText->SetToolTipText(FText::FromString(Message));
}

void SGPAAssistantWidget::SetSearchResult(bool bSucceeded, const FString &Message,
                                          const TArray<FGPAASourceReference> &Results)
{
    StopBusy();
    StatusText->SetText(FText::FromString(bSucceeded ? TEXT("Search complete") : TEXT("Error")));
    SearchSummaryText->SetText(FText::FromString(Message));

    SearchResults.Reset();
    if (bSucceeded)
    {
        for (const FGPAASourceReference &Result : Results)
        {
            SearchResults.Add(MakeShared<FGPAASourceReference>(Result));
        }
    }

    RebuildFilteredSearchResults();
    RefreshSourceLists();
}

void SGPAAssistantWidget::SetAnswerText(const FString &Text)
{
    Conversation->UpdateAnswer(Text);
}

void SGPAAssistantWidget::BeginPromptTurn(const FString &Prompt)
{
    NativeAnswer.Reset();
    PromptQuestionTextBox->SetText(FText::GetEmpty());
    Conversation->BeginTurn(Prompt);
}

void SGPAAssistantWidget::RenderPromptTurn()
{
    if (Conversation->HasActiveTurn())
        Conversation->UpdateAnswer(NativeAnswer);
}

void SGPAAssistantWidget::FinishPromptTurn(const FString &Answer)
{
    Conversation->FinishTurn(Answer);
}

void SGPAAssistantWidget::SetChatResult(bool bSucceeded, const FString &Message, const FString &Answer,
                                        const TArray<FGPAASourceReference> &Sources)
{
    StopBusy();
    StatusText->SetText(
        FText::FromString(bSucceeded ? FString::Printf(TEXT("Answer complete via %s"),
                                                       ActiveChatProviderName.IsEmpty() ? *GetSelectedChatProviderName()
                                                                                        : *ActiveChatProviderName)
                                     : TEXT("Error")));
    FString FinalAnswer = Answer.IsEmpty() ? NativeAnswer : Answer;
    if (!bSucceeded)
        FinalAnswer += (FinalAnswer.IsEmpty() ? TEXT("[Error] ") : TEXT("\n\n[Interrupted] ")) + Message;
    if (Conversation->HasActiveTurn())
        FinishPromptTurn(FinalAnswer);
    else
        SetAnswerText(FinalAnswer);

    AnswerSources.Reset();
    if (bSucceeded)
    {
        for (const FGPAASourceReference &Source : Sources)
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

EVisibility SGPAAssistantWidget::GetRequestCancelVisibility(bool ForAsk) const
{
    return bCancellable && bBusy && bRequestIsAsk == ForAsk ? EVisibility::Visible : EVisibility::Collapsed;
}

void SGPAAssistantWidget::StopBusy()
{
    bBusy = false;
    bCancellable = false;
}

TSharedRef<ITableRow> SGPAAssistantWidget::GenerateSourceRow(TSharedPtr<FGPAASourceReference> Item,
                                                             const TSharedRef<STableViewBase> &OwnerTable)
{
    const FString SourceType = Item.IsValid() && !Item->SourceType.IsEmpty() ? Item->SourceType : TEXT("Code");
    const FString Location = Item.IsValid()
                                 ? FString::Printf(TEXT("[%s] %s:%d-%d (score %.3f)"), *SourceType, *Item->RelativePath,
                                                   Item->StartLine, Item->EndLine, Item->Score)
                                 : TEXT("Unknown source");

    const FString Snippet = Item.IsValid() ? Item->Content : FString();

    return SNew(STableRow<TSharedPtr<FGPAASourceReference>>, OwnerTable)[SNew(SBorder).Padding(
        6.0f)[SNew(SHorizontalBox)

              + SHorizontalBox::Slot().FillWidth(
                    1.0f)[SNew(SVerticalBox)

                          + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Location))]

                          + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
                                [SNew(STextBlock).Text(FText::FromString(Snippet)).AutoWrapText(true)]]

              + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(8.0f, 0.0f, 0.0f, 0.0f)
                    .VAlign(VAlign_Center)[SNew(SButton)
                                               .Text(LOCTEXT("OpenSourceButton", "Open"))
                                               .OnClicked(this, &SGPAAssistantWidget::OnOpenSourceClicked, Item)]]];
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

TSharedRef<SWidget> SGPAAssistantWidget::MakeSearchScopeRadio(const FText &Label, const FString &Scope)
{
    return SNew(SCheckBox)
        .Style(FAppStyle::Get(), "RadioButton")
        .IsChecked(this, &SGPAAssistantWidget::GetSearchScopeCheckState, Scope)
        .OnCheckStateChanged(this, &SGPAAssistantWidget::OnSearchScopeChanged, Scope)[SNew(STextBlock).Text(Label)];
}
ECheckBoxState SGPAAssistantWidget::GetSearchScopeCheckState(FString Scope) const
{
    return SelectedSearchScope == Scope ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}
void SGPAAssistantWidget::OnSearchScopeChanged(ECheckBoxState State, FString Scope)
{
    if (State != ECheckBoxState::Checked)
        return;
    SelectedSearchScope = Scope;
    RebuildFilteredSearchResults();
    RefreshSourceLists();
}

FText SGPAAssistantWidget::GetSettingsSummaryText() const
{
    const auto *Settings = GetDefault<UGPAAEditorSettings>();
    if (Settings->ChatProvider.IsEmpty())
        return LOCTEXT(
            "SetupRequired",
            "Setup required — use Quick Start or choose an AI provider in Settings. Search is available locally.");
    const FString Model =
        Settings->ChatProvider == TEXT("Local")
            ? (Settings->LocalModelFile.IsEmpty() ? Settings->LocalModelId
                                                  : FPaths::GetCleanFilename(Settings->LocalModelFile))
            : Settings->OpenAIModel;
    return FText::FromString(
        FString::Printf(TEXT("Provider: %s | Model: %s | Search: local"), *Settings->ChatProvider, *Model));
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
    return SelectedSearchScope;
}

FString SGPAAssistantWidget::GetSourceOpenerCommand() const
{
    return GetDefault<UGPAAEditorSettings>()->CustomSourceOpenerCommand;
}

void SGPAAssistantWidget::RebuildFilteredSearchResults()
{
    FilteredSearchResults.Reset();

    const FString &Filter = SelectedSearchScope;

    for (const TSharedPtr<FGPAASourceReference> &Result : SearchResults)
    {
        if (!Result.IsValid())
        {
            continue;
        }

        const bool bIsAsset = Result->SourceType.Equals(TEXT("Asset"), ESearchCase::IgnoreCase) ||
                              Result->SourceType.Equals(SearchResultFilterAssets, ESearchCase::IgnoreCase);
        const bool bShouldShow = Filter.Equals(SourceFilterAll, ESearchCase::IgnoreCase) ||
                                 (Filter.Equals(SearchResultFilterCode, ESearchCase::IgnoreCase) && !bIsAsset) ||
                                 (Filter.Equals(SearchResultFilterAssets, ESearchCase::IgnoreCase) && bIsAsset);

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
