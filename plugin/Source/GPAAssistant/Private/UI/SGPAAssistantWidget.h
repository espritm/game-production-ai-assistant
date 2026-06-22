#pragma once

#include "CoreMinimal.h"
#include "Services/GPAAHttpClient.h"
#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;
class SMultiLineEditableTextBox;
class STextBlock;

class SGPAAssistantWidget final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SGPAAssistantWidget) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    FReply OnCheckBackendClicked();
    FReply OnIndexProjectClicked();
    FReply OnAskClicked();
    void SetBusy(const FString& Message);
    void SetResult(bool bSucceeded, const FString& Message);
    FString GetBackendUrl() const;
    FString GetProjectRoot() const;
    FString GetQuestion() const;

    TSharedPtr<FGPAAHttpClient> HttpClient;
    TSharedPtr<SEditableTextBox> BackendUrlTextBox;
    TSharedPtr<SEditableTextBox> ProjectRootTextBox;
    TSharedPtr<SEditableTextBox> QuestionTextBox;
    TSharedPtr<STextBlock> StatusText;
    TSharedPtr<SMultiLineEditableTextBox> AnswerTextBox;
};
