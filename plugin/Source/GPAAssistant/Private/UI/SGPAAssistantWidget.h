// Copyright Game Production AI Assistant contributors 2026.
#pragma once

#include "CoreMinimal.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#include "Models/GPAAApiModels.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Views/SListView.h"
PRAGMA_ENABLE_DEPRECATION_WARNINGS

class SEditableTextBox;
class SButton;
class SMultiLineEditableTextBox;
class STableViewBase;
class STextBlock;
class SWidgetSwitcher;
enum class ECheckBoxState : uint8;
class ITableRow;
template<typename OptionType> class SSegmentedControl;

class SGPAAssistantWidget final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SGPAAssistantWidget) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    virtual ~SGPAAssistantWidget() override;

private:
    friend class FGPAAHistoryLayoutTest;
    friend class FGPAAPromptLayoutTest;
    friend class FGPAASearchLayoutTest;
    friend class FGPAAViewSelectorTest;
    friend class FGPAACancelUiTest;
    EVisibility GetRequestCancelVisibility(bool ForAsk) const;
    TSharedPtr<SButton> PromptCancelButton, SearchCancelButton;
    bool bCancellable = false;
    bool bRequestIsAsk = false;
    void OnViewSelectionChanged(int32 Index);
    TSharedPtr<SSegmentedControl<int32>> ViewSelector;
    void SetAnswerText(const FString& Text);
    TSharedPtr<SWidgetSwitcher> MainViewSwitcher;
    FReply OnCancelClicked();
    void StartNative(bool Ask);
    FGuid NativeRequestId;
    FString NativeAnswer;
    FReply OnOpenSettingsClicked();
    FReply OnLoadChatSettingsClicked();
    FReply OnSaveChatSettingsClicked();
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
    void LoadChatSettings();
    void SaveChatSettings();
    void ApplyChatSettingsResponse(bool bSucceeded, const FString& Message);
    FGPAAChatSettings GetChatSettingsFromUi() const;
    void OnSourceOpenerModeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
    void OnChatProviderChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
    TSharedRef<SWidget> MakeSearchScopeRadio(const FText& Label, const FString& Scope);
    ECheckBoxState GetSearchScopeCheckState(FString Scope) const;
    void OnSearchScopeChanged(ECheckBoxState State, FString Scope);
    void OnCustomSourceOpenerCommitted(const FText& Text, ETextCommit::Type CommitType);
    TSharedRef<SWidget> GenerateSourceOpenerModeWidget(TSharedPtr<FString> Item) const;
    TSharedRef<SWidget> GenerateChatProviderWidget(TSharedPtr<FString> Item) const;
    FText GetSelectedSourceOpenerModeText() const;
    FText GetSelectedChatProviderText() const;
    FText GetSettingsSummaryText() const;
    EVisibility GetCustomSourceOpenerVisibility() const;
    EVisibility GetOpenAICompatibleSettingsVisibility() const;
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

    TSharedPtr<SComboBox<TSharedPtr<FString>>> SourceOpenerComboBox;
    TSharedPtr<SEditableTextBox> SourceOpenerTextBox;
    TSharedPtr<SComboBox<TSharedPtr<FString>>> ChatProviderComboBox;
    TSharedPtr<SEditableTextBox> PromptQuestionTextBox;
    TSharedPtr<SEditableTextBox> SearchQueryTextBox;
    TSharedPtr<SWidgetSwitcher> TabSwitcher;
    TSharedPtr<STextBlock> StatusText;
    TSharedPtr<STextBlock> SettingsSummaryText;
    TSharedPtr<STextBlock> SearchSummaryText;
    TSharedPtr<SMultiLineEditableTextBox> AnswerTextBox;
    TSharedPtr<SMultiLineEditableTextBox> HistoryTextBox;
    TSharedPtr<SListView<TSharedPtr<FGPAASourceReference>>> SearchResultsListView;
    TArray<TSharedPtr<FString>> SourceOpenerModes;
    TSharedPtr<FString> SelectedSourceOpenerMode;
    TArray<TSharedPtr<FString>> ChatProviderModes;
    TSharedPtr<FString> SelectedChatProviderMode;
    FString CustomSourceOpenerCommand = TEXT("code -g \"{file}:{line}\"");
    FString SelectedSearchScope = TEXT("All");
    FString BusyBaseMessage;
    int32 BusyFrame = 0;
    int32 ActiveTabIndex = 0;
    bool bBusy = false;
    TArray<TSharedPtr<FGPAASourceReference>> SearchResults;
    TArray<TSharedPtr<FGPAASourceReference>> FilteredSearchResults;
    TArray<TSharedPtr<FGPAASourceReference>> AnswerSources;
};
