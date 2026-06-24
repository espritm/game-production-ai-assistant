#include "UI/SGPAAssistantWidget.h"

#include "Editor.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SGPAAssistantWidget"

namespace
{
    const FString SourceOpenerAuto = TEXT("Auto");
    const FString SourceOpenerVSCode = TEXT("Visual Studio Code");
    const FString SourceOpenerRider = TEXT("Rider");
    const FString SourceOpenerVisualStudio = TEXT("Visual Studio");
    const FString SourceOpenerDefault = TEXT("Default Windows App");
    const FString SourceOpenerCustom = TEXT("Custom Command");
    const TCHAR* SourceOpenerConfigSection = TEXT("GPAAssistant.SourceOpener");
}

void SGPAAssistantWidget::Construct(const FArguments& InArgs)
{
    HttpClient = MakeShared<FGPAAHttpClient>();
    InitializeSourceOpenerOptions();
    LoadSourceOpenerSettings();

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
            [
                SNew(STextBlock)
                .Text(LOCTEXT("SourceOpenerLabel", "Source Opener"))
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 4.0f, 0.0f, 10.0f)
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .FillWidth(0.35f)
                .Padding(0.0f, 0.0f, 8.0f, 0.0f)
                [
                    SAssignNew(SourceOpenerComboBox, SComboBox<TSharedPtr<FString>>)
                    .OptionsSource(&SourceOpenerModes)
                    .InitiallySelectedItem(SelectedSourceOpenerMode)
                    .OnSelectionChanged(this, &SGPAAssistantWidget::OnSourceOpenerModeChanged)
                    .OnGenerateWidget(this, &SGPAAssistantWidget::GenerateSourceOpenerModeWidget)
                    [
                        SNew(STextBlock)
                        .Text(this, &SGPAAssistantWidget::GetSelectedSourceOpenerModeText)
                    ]
                ]

                + SHorizontalBox::Slot()
                .FillWidth(0.65f)
                [
                    SAssignNew(SourceOpenerTextBox, SEditableTextBox)
                    .Text(FText::FromString(CustomSourceOpenerCommand))
                    .Visibility(this, &SGPAAssistantWidget::GetCustomSourceOpenerVisibility)
                    .OnTextCommitted(this, &SGPAAssistantWidget::OnCustomSourceOpenerCommitted)
                ]
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
                    .Text(LOCTEXT("HistoryButton", "History"))
                    .OnClicked(this, &SGPAAssistantWidget::OnHistoryClicked)
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
                SNew(SUniformGridPanel)
                .SlotPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f))

                + SUniformGridPanel::Slot(0, 0)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("SearchButton", "Search"))
                    .OnClicked(this, &SGPAAssistantWidget::OnSearchClicked)
                ]

                + SUniformGridPanel::Slot(1, 0)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("AskButton", "Ask"))
                    .OnClicked(this, &SGPAAssistantWidget::OnAskClicked)
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
            .FillHeight(1.0f)
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
                .FillHeight(0.44f)
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
                    SAssignNew(SearchSummaryText, STextBlock)
                    .Text(LOCTEXT("SearchResultsLabel", "Search results"))
                ]

                + SVerticalBox::Slot()
                .FillHeight(0.34f)
                .Padding(0.0f, 0.0f, 0.0f, 10.0f)
                [
                    SAssignNew(SearchResultsListView, SListView<TSharedPtr<FGPAASourceReference>>)
                    .ListItemsSource(&SearchResults)
                    .OnGenerateRow(this, &SGPAAssistantWidget::GenerateSourceRow)
                    .SelectionMode(ESelectionMode::Single)
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, 0.0f, 0.0f, 4.0f)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("AnswerSourcesLabel", "Answer sources"))
                ]

                + SVerticalBox::Slot()
                .FillHeight(0.22f)
                [
                    SAssignNew(AnswerSourcesListView, SListView<TSharedPtr<FGPAASourceReference>>)
                    .ListItemsSource(&AnswerSources)
                    .OnGenerateRow(this, &SGPAAssistantWidget::GenerateSourceRow)
                    .SelectionMode(ESelectionMode::Single)
                ]
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

FReply SGPAAssistantWidget::OnHistoryClicked()
{
    SetBusy(TEXT("Loading local chat history..."));

    HttpClient->GetChatHistory(
        GetBackendUrl(),
        GetProjectRoot(),
        FGPAARequestCompleted::CreateSP(this, &SGPAAssistantWidget::SetResult));

    return FReply::Handled();
}

FReply SGPAAssistantWidget::OnSearchClicked()
{
    SetBusy(TEXT("Refreshing index and searching project..."));

    HttpClient->SearchProject(
        GetBackendUrl(),
        GetProjectRoot(),
        GetQuestion(),
        FGPAASearchCompleted::CreateSP(this, &SGPAAssistantWidget::SetSearchResult));

    return FReply::Handled();
}

FReply SGPAAssistantWidget::OnAskClicked()
{
    SetBusy(TEXT("Refreshing index and asking local assistant..."));

    HttpClient->AskQuestion(
        GetBackendUrl(),
        GetProjectRoot(),
        GetQuestion(),
        FGPAAChatCompleted::CreateSP(this, &SGPAAssistantWidget::SetChatResult));

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

void SGPAAssistantWidget::SetSearchResult(bool bSucceeded, const FString& Message, const TArray<FGPAASourceReference>& Results)
{
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

    RefreshSourceLists();
}

void SGPAAssistantWidget::SetChatResult(bool bSucceeded, const FString& Message, const FString& Answer, const TArray<FGPAASourceReference>& Sources)
{
    StatusText->SetText(FText::FromString(bSucceeded ? TEXT("Answer complete") : TEXT("Error")));
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

TSharedRef<ITableRow> SGPAAssistantWidget::GenerateSourceRow(TSharedPtr<FGPAASourceReference> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
    const FString Location = Item.IsValid()
        ? FString::Printf(TEXT("%s:%d-%d (score %.3f)"), *Item->RelativePath, Item->StartLine, Item->EndLine, Item->Score)
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

    const FString AbsolutePath = BuildAbsoluteSourcePath(*Item);
    if (!FPaths::FileExists(AbsolutePath))
    {
        StatusText->SetText(FText::FromString(FString::Printf(TEXT("File not found: %s"), *AbsolutePath)));
        return FReply::Handled();
    }

    if (!TryRunConfiguredSourceOpener(AbsolutePath, Item->StartLine))
    {
        FPlatformProcess::LaunchFileInDefaultExternalApplication(*AbsolutePath);
    }

    StatusText->SetText(FText::FromString(FString::Printf(TEXT("Opened %s"), *Item->RelativePath)));
    return FReply::Handled();
}

bool SGPAAssistantWidget::TryRunConfiguredSourceOpener(const FString& AbsolutePath, int32 LineNumber)
{
    const FString Mode = SelectedSourceOpenerMode.IsValid() ? *SelectedSourceOpenerMode : SourceOpenerAuto;
    return TryRunSourceOpenerMode(Mode, AbsolutePath, LineNumber);
}

bool SGPAAssistantWidget::TryRunSourceOpenerMode(const FString& Mode, const FString& AbsolutePath, int32 LineNumber)
{
    if (Mode == SourceOpenerDefault)
    {
        return false;
    }

    if (Mode == SourceOpenerCustom)
    {
        return TryRunCustomSourceOpener(GetSourceOpenerCommand(), AbsolutePath, LineNumber);
    }

    if (Mode == SourceOpenerVSCode)
    {
        return TryOpenWithVisualStudioCode(AbsolutePath, LineNumber);
    }

    if (Mode == SourceOpenerRider)
    {
        return TryOpenWithRider(AbsolutePath, LineNumber);
    }

    if (Mode == SourceOpenerVisualStudio)
    {
        return TryOpenWithVisualStudio(AbsolutePath);
    }

    return TryOpenWithVisualStudioCode(AbsolutePath, LineNumber)
        || TryOpenWithRider(AbsolutePath, LineNumber)
        || TryOpenWithVisualStudio(AbsolutePath);
}

bool SGPAAssistantWidget::TryRunCustomSourceOpener(const FString& CommandTemplate, const FString& AbsolutePath, int32 LineNumber)
{
    FString Command = CommandTemplate.TrimStartAndEnd();
    if (Command.IsEmpty())
    {
        return false;
    }

    Command.ReplaceInline(TEXT("{file}"), *AbsolutePath);
    Command.ReplaceInline(TEXT("{line}"), *FString::FromInt(FMath::Max(LineNumber, 1)));

    FString Executable;
    FString Arguments;
    if (Command.StartsWith(TEXT("\"")))
    {
        const int32 ClosingQuoteIndex = Command.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, 1);
        if (ClosingQuoteIndex <= 0)
        {
            return false;
        }

        Executable = Command.Mid(1, ClosingQuoteIndex - 1);
        Arguments = Command.Mid(ClosingQuoteIndex + 1).TrimStartAndEnd();
    }
    else
    {
        FString Left;
        FString Right;
        if (Command.Split(TEXT(" "), &Left, &Right))
        {
            Executable = Left;
            Arguments = Right;
        }
        else
        {
            Executable = Command;
        }
    }

    if (Executable.IsEmpty())
    {
        return false;
    }

    return TryRunExecutable(Executable, Arguments);
}

bool SGPAAssistantWidget::TryRunExecutable(const FString& Executable, const FString& Arguments)
{
    if (Executable.IsEmpty())
    {
        return false;
    }

    FProcHandle Process = FPlatformProcess::CreateProc(*Executable, *Arguments, true, false, false, nullptr, 0, nullptr, nullptr);
    if (Process.IsValid())
    {
        FPlatformProcess::CloseProc(Process);
        return true;
    }

    return false;
}

bool SGPAAssistantWidget::TryOpenWithVisualStudioCode(const FString& AbsolutePath, int32 LineNumber)
{
    const FString LocalAppData = FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA"));
    const FString ProgramFiles = FPlatformMisc::GetEnvironmentVariable(TEXT("ProgramFiles"));
    const FString ProgramFilesX86 = FPlatformMisc::GetEnvironmentVariable(TEXT("ProgramFiles(x86)"));

    TArray<FString> Candidates;
    Candidates.Add(FPaths::Combine(LocalAppData, TEXT("Programs/Microsoft VS Code/Code.exe")));
    Candidates.Add(FPaths::Combine(ProgramFiles, TEXT("Microsoft VS Code/Code.exe")));
    Candidates.Add(FPaths::Combine(ProgramFilesX86, TEXT("Microsoft VS Code/Code.exe")));

    const FString Arguments = FString::Printf(TEXT("-g \"%s:%d\""), *AbsolutePath, FMath::Max(LineNumber, 1));
    for (const FString& Candidate : Candidates)
    {
        if (FPaths::FileExists(Candidate) && TryRunExecutable(Candidate, Arguments))
        {
            return true;
        }
    }

    return TryRunExecutable(TEXT("code"), Arguments);
}

bool SGPAAssistantWidget::TryOpenWithRider(const FString& AbsolutePath, int32 LineNumber)
{
    const FString LocalAppData = FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA"));
    TArray<FString> Candidates;
    if (!LocalAppData.IsEmpty())
    {
        IFileManager::Get().FindFilesRecursive(
            Candidates,
            *FPaths::Combine(LocalAppData, TEXT("JetBrains/Toolbox/apps")),
            TEXT("rider64.exe"),
            true,
            false);
    }

    const FString Arguments = FString::Printf(TEXT("--line %d \"%s\""), FMath::Max(LineNumber, 1), *AbsolutePath);
    for (const FString& Candidate : Candidates)
    {
        if (TryRunExecutable(Candidate, Arguments))
        {
            return true;
        }
    }

    return TryRunExecutable(TEXT("rider64"), Arguments) || TryRunExecutable(TEXT("rider"), Arguments);
}

bool SGPAAssistantWidget::TryOpenWithVisualStudio(const FString& AbsolutePath)
{
    const FString ProgramFiles = FPlatformMisc::GetEnvironmentVariable(TEXT("ProgramFiles"));
    const FString ProgramFilesX86 = FPlatformMisc::GetEnvironmentVariable(TEXT("ProgramFiles(x86)"));

    TArray<FString> Candidates;
    const TArray<FString> Roots = { ProgramFiles, ProgramFilesX86 };
    const TArray<FString> Versions = { TEXT("18"), TEXT("2022"), TEXT("2019") };
    const TArray<FString> Editions = { TEXT("Community"), TEXT("Professional"), TEXT("Enterprise") };

    for (const FString& Root : Roots)
    {
        if (Root.IsEmpty())
        {
            continue;
        }

        for (const FString& Version : Versions)
        {
            for (const FString& Edition : Editions)
            {
                Candidates.Add(FPaths::Combine(Root, TEXT("Microsoft Visual Studio"), Version, Edition, TEXT("Common7/IDE/devenv.exe")));
            }
        }
    }

    const FString Arguments = FString::Printf(TEXT("/Edit \"%s\""), *AbsolutePath);
    for (const FString& Candidate : Candidates)
    {
        if (FPaths::FileExists(Candidate) && TryRunExecutable(Candidate, Arguments))
        {
            return true;
        }
    }

    return TryRunExecutable(TEXT("devenv"), Arguments);
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

void SGPAAssistantWidget::LoadSourceOpenerSettings()
{
    FString Mode;
    if (GConfig->GetString(SourceOpenerConfigSection, TEXT("Mode"), Mode, GEditorPerProjectIni))
    {
        for (const TSharedPtr<FString>& Candidate : SourceOpenerModes)
        {
            if (Candidate.IsValid() && *Candidate == Mode)
            {
                SelectedSourceOpenerMode = Candidate;
                break;
            }
        }
    }

    FString SavedCustomCommand;
    if (GConfig->GetString(SourceOpenerConfigSection, TEXT("CustomCommand"), SavedCustomCommand, GEditorPerProjectIni)
        && !SavedCustomCommand.IsEmpty())
    {
        CustomSourceOpenerCommand = SavedCustomCommand;
    }
}

void SGPAAssistantWidget::SaveSourceOpenerSettings() const
{
    const FString Mode = SelectedSourceOpenerMode.IsValid() ? *SelectedSourceOpenerMode : SourceOpenerAuto;
    GConfig->SetString(SourceOpenerConfigSection, TEXT("Mode"), *Mode, GEditorPerProjectIni);
    GConfig->SetString(SourceOpenerConfigSection, TEXT("CustomCommand"), *GetSourceOpenerCommand(), GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
}

void SGPAAssistantWidget::OnSourceOpenerModeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
    if (NewSelection.IsValid())
    {
        SelectedSourceOpenerMode = NewSelection;
        SaveSourceOpenerSettings();
    }
}

void SGPAAssistantWidget::OnCustomSourceOpenerCommitted(const FText& Text, ETextCommit::Type CommitType)
{
    SaveSourceOpenerSettings();
}

TSharedRef<SWidget> SGPAAssistantWidget::GenerateSourceOpenerModeWidget(TSharedPtr<FString> Item) const
{
    return SNew(STextBlock)
        .Text(FText::FromString(Item.IsValid() ? *Item : SourceOpenerAuto));
}

FText SGPAAssistantWidget::GetSelectedSourceOpenerModeText() const
{
    return FText::FromString(SelectedSourceOpenerMode.IsValid() ? *SelectedSourceOpenerMode : SourceOpenerAuto);
}

EVisibility SGPAAssistantWidget::GetCustomSourceOpenerVisibility() const
{
    return SelectedSourceOpenerMode.IsValid() && *SelectedSourceOpenerMode == SourceOpenerCustom
        ? EVisibility::Visible
        : EVisibility::Collapsed;
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

FString SGPAAssistantWidget::GetSourceOpenerCommand() const
{
    return SourceOpenerTextBox.IsValid() ? SourceOpenerTextBox->GetText().ToString() : FString();
}

FString SGPAAssistantWidget::BuildAbsoluteSourcePath(const FGPAASourceReference& Source) const
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(GetProjectRoot(), Source.RelativePath));
}

void SGPAAssistantWidget::RefreshSourceLists()
{
    if (SearchResultsListView.IsValid())
    {
        SearchResultsListView->RequestListRefresh();
    }

    if (AnswerSourcesListView.IsValid())
    {
        AnswerSourcesListView->RequestListRefresh();
    }
}

#undef LOCTEXT_NAMESPACE
