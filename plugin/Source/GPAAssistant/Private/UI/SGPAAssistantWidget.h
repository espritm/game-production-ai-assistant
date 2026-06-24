#pragma once

#include "CoreMinimal.h"
#include "Services/GPAAHttpClient.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Views/SListView.h"

class SEditableTextBox;
class SMultiLineEditableTextBox;
class STableViewBase;
class STextBlock;
class ITableRow;

class SGPAAssistantWidget final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SGPAAssistantWidget) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    FReply OnCheckBackendClicked();
    FReply OnHistoryClicked();
    FReply OnSearchClicked();
    FReply OnAskClicked();
    void SetBusy(const FString& Message);
    void SetResult(bool bSucceeded, const FString& Message);
    void SetSearchResult(bool bSucceeded, const FString& Message, const TArray<FGPAASourceReference>& Results);
    void SetChatResult(bool bSucceeded, const FString& Message, const FString& Answer, const TArray<FGPAASourceReference>& Sources);
    TSharedRef<ITableRow> GenerateSourceRow(TSharedPtr<FGPAASourceReference> Item, const TSharedRef<STableViewBase>& OwnerTable);
    FReply OnOpenSourceClicked(TSharedPtr<FGPAASourceReference> Item);
    bool TryRunConfiguredSourceOpener(const FString& AbsolutePath, int32 LineNumber);
    bool TryRunSourceOpenerMode(const FString& Mode, const FString& AbsolutePath, int32 LineNumber);
    bool TryRunCustomSourceOpener(const FString& CommandTemplate, const FString& AbsolutePath, int32 LineNumber);
    bool TryRunExecutable(const FString& Executable, const FString& Arguments);
    bool TryOpenWithVisualStudioCode(const FString& AbsolutePath, int32 LineNumber);
    bool TryOpenWithRider(const FString& AbsolutePath, int32 LineNumber);
    bool TryOpenWithVisualStudio(const FString& AbsolutePath);
    void InitializeSourceOpenerOptions();
    void LoadSourceOpenerSettings();
    void SaveSourceOpenerSettings() const;
    void OnSourceOpenerModeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
    void OnCustomSourceOpenerCommitted(const FText& Text, ETextCommit::Type CommitType);
    TSharedRef<SWidget> GenerateSourceOpenerModeWidget(TSharedPtr<FString> Item) const;
    FText GetSelectedSourceOpenerModeText() const;
    EVisibility GetCustomSourceOpenerVisibility() const;
    FString GetBackendUrl() const;
    FString GetProjectRoot() const;
    FString GetQuestion() const;
    FString GetSourceOpenerCommand() const;
    FString BuildAbsoluteSourcePath(const FGPAASourceReference& Source) const;
    void RefreshSourceLists();

    TSharedPtr<FGPAAHttpClient> HttpClient;
    TSharedPtr<SEditableTextBox> BackendUrlTextBox;
    TSharedPtr<SEditableTextBox> ProjectRootTextBox;
    TSharedPtr<SComboBox<TSharedPtr<FString>>> SourceOpenerComboBox;
    TSharedPtr<SEditableTextBox> SourceOpenerTextBox;
    TSharedPtr<SEditableTextBox> QuestionTextBox;
    TSharedPtr<STextBlock> StatusText;
    TSharedPtr<STextBlock> SearchSummaryText;
    TSharedPtr<STextBlock> AnswerSummaryText;
    TSharedPtr<SMultiLineEditableTextBox> AnswerTextBox;
    TSharedPtr<SListView<TSharedPtr<FGPAASourceReference>>> SearchResultsListView;
    TSharedPtr<SListView<TSharedPtr<FGPAASourceReference>>> AnswerSourcesListView;
    TArray<TSharedPtr<FString>> SourceOpenerModes;
    TSharedPtr<FString> SelectedSourceOpenerMode;
    FString CustomSourceOpenerCommand = TEXT("code -g \"{file}:{line}\"");
    TArray<TSharedPtr<FGPAASourceReference>> SearchResults;
    TArray<TSharedPtr<FGPAASourceReference>> AnswerSources;
};
