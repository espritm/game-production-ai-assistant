#include "UI/SGPAAssistantWidget.h"

#include "Editor.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SGPAAssistantWidget"

void SGPAAssistantWidget::Construct(const FArguments& InArgs)
{
    HttpClient = MakeShared<FGPAAHttpClient>();

    const FString DefaultProjectRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());

    ChildSlot
    [
        SNew(SBorder)
        .Padding(12.0f)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(STextBlock)
                .Text(LOCTEXT("BackendUrlLabel", "Backend URL"))
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 4.0f, 0.0f, 10.0f)
            [
                SAssignNew(BackendUrlTextBox, SEditableTextBox)
                .Text(FText::FromString(TEXT("http://localhost:5000")))
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(STextBlock)
                .Text(LOCTEXT("ProjectRootLabel", "Project Root"))
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 4.0f, 0.0f, 10.0f)
            [
                SAssignNew(ProjectRootTextBox, SEditableTextBox)
                .Text(FText::FromString(DefaultProjectRoot))
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 10.0f)
            [
                SNew(SUniformGridPanel)
                .SlotPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f))

                + SUniformGridPanel::Slot(0, 0)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("CheckBackendButton", "Check Backend"))
                    .OnClicked(this, &SGPAAssistantWidget::OnCheckBackendClicked)
                ]

                + SUniformGridPanel::Slot(1, 0)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("IndexButton", "Index Project"))
                    .OnClicked(this, &SGPAAssistantWidget::OnIndexProjectClicked)
                ]
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SSeparator)
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 10.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("QuestionLabel", "Question"))
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 4.0f, 0.0f, 10.0f)
            [
                SAssignNew(QuestionTextBox, SEditableTextBox)
                .Text(LOCTEXT("QuestionPlaceholder", "Where is stamina regeneration handled?"))
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 10.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("AskButton", "Ask"))
                .OnClicked(this, &SGPAAssistantWidget::OnAskClicked)
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 8.0f)
            [
                SAssignNew(StatusText, STextBlock)
                .Text(LOCTEXT("ReadyStatus", "Ready"))
            ]

            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            [
                SAssignNew(AnswerTextBox, SMultiLineEditableTextBox)
                .IsReadOnly(true)
                .AutoWrapText(true)
                .Text(LOCTEXT("EmptyAnswer", ""))
            ]
        ]
    ];
}

FReply SGPAAssistantWidget::OnCheckBackendClicked()
{
    SetBusy(TEXT("Checking backend..."));

    HttpClient->CheckBackend(
        GetBackendUrl(),
        FGPAARequestCompleted::CreateSP(this, &SGPAAssistantWidget::SetResult));

    return FReply::Handled();
}

FReply SGPAAssistantWidget::OnIndexProjectClicked()
{
    SetBusy(TEXT("Indexing project..."));

    HttpClient->IndexProject(
        GetBackendUrl(),
        GetProjectRoot(),
        FGPAARequestCompleted::CreateSP(this, &SGPAAssistantWidget::SetResult));

    return FReply::Handled();
}

FReply SGPAAssistantWidget::OnAskClicked()
{
    SetBusy(TEXT("Asking local assistant..."));

    HttpClient->AskQuestion(
        GetBackendUrl(),
        GetProjectRoot(),
        GetQuestion(),
        FGPAARequestCompleted::CreateSP(this, &SGPAAssistantWidget::SetResult));

    return FReply::Handled();
}

void SGPAAssistantWidget::SetBusy(const FString& Message)
{
    StatusText->SetText(FText::FromString(Message));
}

void SGPAAssistantWidget::SetResult(bool bSucceeded, const FString& Message)
{
    StatusText->SetText(FText::FromString(bSucceeded ? TEXT("Done") : TEXT("Error")));
    AnswerTextBox->SetText(FText::FromString(Message));
}

FString SGPAAssistantWidget::GetBackendUrl() const
{
    return BackendUrlTextBox.IsValid() ? BackendUrlTextBox->GetText().ToString() : TEXT("http://localhost:5000");
}

FString SGPAAssistantWidget::GetProjectRoot() const
{
    return ProjectRootTextBox.IsValid() ? ProjectRootTextBox->GetText().ToString() : FPaths::ProjectDir();
}

FString SGPAAssistantWidget::GetQuestion() const
{
    return QuestionTextBox.IsValid() ? QuestionTextBox->GetText().ToString() : FString();
}

#undef LOCTEXT_NAMESPACE
