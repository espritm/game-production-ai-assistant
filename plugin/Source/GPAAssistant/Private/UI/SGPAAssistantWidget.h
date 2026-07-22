#pragma once

#include "CoreMinimal.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#include "Services/GPAAHttpClient.h"
#include "Services/GPAABackendSupervisor.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Views/SListView.h"
PRAGMA_ENABLE_DEPRECATION_WARNINGS

class SEditableTextBox;
class SMultiLineEditableTextBox;
class STableViewBase;
class STextBlock;
class SWidgetSwitcher;
class ITableRow;

class SGPAAssistantWidget final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SGPAAssistantWidget) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    FReply OnOpenSettingsClicked();
    FReply OnLoadChatSettingsClicked();
    FReply OnSaveChatSettingsClicked();
    FReply OnRefreshKnowledgeClicked();
    FReply OnHistoryClicked();
    FReply OnCloseHistoryClicked();
    FReply OnPromptTabClicked();
    FReply OnSearchTabClicked();
    FReply OnSearchClicked();
    FReply OnAskClicked();
    FString BuildAssetMetadataSyncRequest() const;
    void SetBusy(const FString& Message);
    void SetBusyFromWorkflow(bool bSucceeded, const FString& Message);
    void SetResult(bool bSucceeded, const FString& Message);
    void SetHistoryResult(bool bSucceeded, const FString& Message);
    void SetSearchResult(bool bSucceeded, const FString& Message, const TArray<FGPAASourceReference>& Results);
    void SetChatResult(bool bSucceeded, const FString& Message, const FString& Answer, const TArray<FGPAASourceReference>& Sources);
    TSharedRef<ITableRow> GenerateSourceRow(TSharedPtr<FGPAASourceReference> Item, const TSharedRef<STableViewBase>& OwnerTable);
    FReply OnOpenSourceClicked(TSharedPtr<FGPAASourceReference> Item);
    void InitializeSourceOpenerOptions();
    void InitializeChatProviderOptions();
    void LoadPluginSettings();
    void SavePluginSettings() const;
    void EnsureBackendReady(const FString& BusyMessage, TFunction<void()> OnReady);
    void LoadChatSettingsFromBackend();
    void SaveChatSettingsToBackend();
    void ApplyChatSettingsResponse(bool bSucceeded, const FString& Message);
    void LoadKnowledgeStatus();
    void ApplyKnowledgeStatusResponse(bool bSucceeded, const FString& Message);
    void UpdateKnowledgeSummaryFromIndexResponse(const FString& Message);
    void UpdateKnowledgeSummary(const FGPAAIndexStatistics& Statistics);
    FGPAAChatSettings GetChatSettingsFromUi() const;
    void OnSourceOpenerModeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
    void OnChatProviderChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
    void OnSearchResultFilterChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
    void OnCustomSourceOpenerCommitted(const FText& Text, ETextCommit::Type CommitType);
    TSharedRef<SWidget> GenerateSourceOpenerModeWidget(TSharedPtr<FString> Item) const;
    TSharedRef<SWidget> GenerateChatProviderWidget(TSharedPtr<FString> Item) const;
    TSharedRef<SWidget> GenerateSearchResultFilterWidget(TSharedPtr<FString> Item) const;
    FText GetSelectedSourceOpenerModeText() const;
    FText GetSelectedChatProviderText() const;
    FText GetSelectedSearchResultFilterText() const;
    FText GetSettingsSummaryText() const;
    EVisibility GetHistoryVisibility() const;
    EVisibility GetCustomSourceOpenerVisibility() const;
    EVisibility GetOpenAICompatibleSettingsVisibility() const;
    FString GetBackendUrl() const;
    FString GetProjectRoot() const;
    FString GetPromptQuestion() const;
    FString GetSearchQuery() const;
    FString GetSelectedChatProviderName() const;
    FString GetSelectedSourceFilterName() const;
    FString GetSourceOpenerCommand() const;
    void RebuildFilteredSearchResults();
    void RefreshSourceLists();
    EActiveTimerReturnType HandleBusyTick(double InCurrentTime, float InDeltaTime);
    void StopBusy();

    TSharedPtr<FGPAAHttpClient> HttpClient;
    TSharedPtr<FGPAABackendSupervisor, ESPMode::ThreadSafe> BackendSupervisor;
    TSharedPtr<SComboBox<TSharedPtr<FString>>> SourceOpenerComboBox;
    TSharedPtr<SEditableTextBox> SourceOpenerTextBox;
    TSharedPtr<SComboBox<TSharedPtr<FString>>> ChatProviderComboBox;
    TSharedPtr<SEditableTextBox> PromptQuestionTextBox;
    TSharedPtr<SEditableTextBox> SearchQueryTextBox;
    TSharedPtr<SComboBox<TSharedPtr<FString>>> SearchResultFilterComboBox;
    TSharedPtr<SWidgetSwitcher> TabSwitcher;
    TSharedPtr<STextBlock> StatusText;
    TSharedPtr<STextBlock> SettingsSummaryText;
    TSharedPtr<STextBlock> KnowledgeText;
    TSharedPtr<STextBlock> SearchSummaryText;
    TSharedPtr<STextBlock> AnswerSummaryText;
    TSharedPtr<SMultiLineEditableTextBox> AnswerTextBox;
    TSharedPtr<SMultiLineEditableTextBox> HistoryTextBox;
    TSharedPtr<SListView<TSharedPtr<FGPAASourceReference>>> SearchResultsListView;
    TArray<TSharedPtr<FString>> SourceOpenerModes;
    TSharedPtr<FString> SelectedSourceOpenerMode;
    TArray<TSharedPtr<FString>> ChatProviderModes;
    TSharedPtr<FString> SelectedChatProviderMode;
    TArray<TSharedPtr<FString>> SearchResultFilterModes;
    TSharedPtr<FString> SelectedSearchResultFilterMode;
    FString CustomSourceOpenerCommand = TEXT("code -g \"{file}:{line}\"");
    FString BusyBaseMessage;
    int32 BusyFrame = 0;
    int32 ActiveTabIndex = 0;
    bool bBusy = false;
    bool bHistoryVisible = false;
    TArray<TSharedPtr<FGPAASourceReference>> SearchResults;
    TArray<TSharedPtr<FGPAASourceReference>> FilteredSearchResults;
    TArray<TSharedPtr<FGPAASourceReference>> AnswerSources;
};
