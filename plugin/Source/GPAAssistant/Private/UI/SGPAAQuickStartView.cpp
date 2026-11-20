// Copyright Game Production AI Assistant contributors 2026.
#include "UI/SGPAAQuickStartView.h"
#include "Application/GPAAQuickStart.h"
#include "Application/GPAANativeRuntime.h"
#include "Settings/GPAAEditorSettings.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateColorBrush.h"
#define LOCTEXT_NAMESPACE "SGPAAQuickStartView"
EVisibility SGPAAQuickStartView::GetQuickStartButtonVisibility() const
{
    return GetDefault<UGPAAEditorSettings>()->ChatProvider.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed;
}
// clang-format off
void SGPAAQuickStartView::Construct(const FArguments& InArgs)
{
    QuickStart = MakeShared<FGPAAQuickStart>();
    auto Steps = SNew(SVerticalBox);
    const TCHAR* Labels[] = {TEXT("1. Set up local AI settings"), TEXT("2. Download and verify the Light model"), TEXT("3. Verify with a local test prompt")};
    for (int32 I = 0; I < 3; ++I)
    {
        Steps->AddSlot().AutoHeight().Padding(0,3)
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,8,0)
            [SNew(SOverlay)
                + SOverlay::Slot()[SNew(SThrobber).NumPieces(3)
                    .Visibility_Lambda([this,I]() { return QuickStart->Running() && QuickStart->StepIndex() == I ? EVisibility::Visible : EVisibility::Hidden; })]
                + SOverlay::Slot().HAlign(HAlign_Center)[SNew(STextBlock).Text(FText::FromString(TEXT("\u2713")))
                    .Visibility_Lambda([this,I]() { return QuickStart->StepIndex() > I ? EVisibility::Visible : EVisibility::Hidden; })]]
            + SHorizontalBox::Slot().FillWidth(1)
            [SNew(STextBlock).Text(FText::FromString(Labels[I]))
                .ColorAndOpacity_Lambda([this,I]() { return QuickStart->StepIndex() >= I ? FSlateColor::UseForeground() : FSlateColor(FLinearColor(.45f,.45f,.45f)); })
                .StrikeBrush_Lambda([this,I]() -> const FSlateBrush*
                { static FSlateColorBrush Strike(FLinearColor::White); Strike.ImageSize = FVector2D(1,1); return QuickStart->StepIndex() > I ? &Strike : nullptr; })]];
    }
    ChildSlot
    [SNew(SVerticalBox)
        .Visibility_Lambda([this]() { return GetDefault<UGPAAEditorSettings>()->ChatProvider.IsEmpty() || QuickStart->StepIndex() >= 0 ? EVisibility::Visible : EVisibility::Collapsed; })
        + SVerticalBox::Slot().AutoHeight()
        [SNew(SHorizontalBox)
            .Visibility_Lambda([this]() { return GetDefault<UGPAAEditorSettings>()->ChatProvider.IsEmpty() || QuickStart->Running() ? EVisibility::Visible : EVisibility::Collapsed; })
            + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(LOCTEXT("QuickStartButton", "Quick Start"))
                .Visibility(this, &SGPAAQuickStartView::GetQuickStartButtonVisibility)
                .IsEnabled_Lambda([this]() { return !QuickStart->Running() && !FGPAANativeRuntime::Get()->IsBusy(); })
                .OnClicked(this, &SGPAAQuickStartView::OnQuickStartClicked)]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6,0)
                [SNew(SImage).Image(FCoreStyle::Get().GetBrush(TEXT("Icons.Help")))
                    .Visibility(this, &SGPAAQuickStartView::GetQuickStartButtonVisibility)
                    .ToolTipText(LOCTEXT("QuickStartHelp", "Selects Local AI, downloads and verifies the Light Qwen model (about 1.04 GiB, Apache 2.0) from Qwen on Hugging Face, then runs a short local test prompt. Requires internet for the first download and additional disk space/RAM. A verified cached model is reused. Answer quality depends directly on the selected model: Light is a starting point, and a more capable model is recommended for better results if your hardware supports it. No API key or cloud prompt is used; prompts and project excerpts stay on this computer. Cloud models can offer better results than lightweight local models, but sending project content to a cloud provider does not guarantee its confidentiality. Closing this panel cancels Quick Start."))]
            + SHorizontalBox::Slot().AutoWidth().Padding(8,0)
                [SNew(SButton).Text(LOCTEXT("CancelQuickStart", "Cancel"))
                    .Visibility_Lambda([this]() { return QuickStart->Running() ? EVisibility::Visible : EVisibility::Collapsed; })
                    .OnClicked_Lambda([this]() { QuickStart->Cancel(); return FReply::Handled(); })]]
        + SVerticalBox::Slot().AutoHeight()
            [SNew(SBox).Visibility_Lambda([this]() { return QuickStart->StepIndex() >= 0 ? EVisibility::Visible : EVisibility::Collapsed; })[Steps]]
        + SVerticalBox::Slot().AutoHeight().Padding(0,4)
            [SNew(STextBlock).AutoWrapText(true)
                .ColorAndOpacity_Lambda([this]() { return QuickStart->Succeeded() ? FSlateColor(FLinearColor(.25f,.8f,.35f)) : FSlateColor::UseForeground(); })
                .Text_Lambda([this]() { return FText::FromString(QuickStart->Message); })]];
}
// clang-format on

FReply SGPAAQuickStartView::OnQuickStartClicked()
{
    if (QuickStart->Start())
        RegisterActiveTimer(.1f, FWidgetActiveTimerDelegate::CreateSP(this, &SGPAAQuickStartView::TickQuickStart));
    return FReply::Handled();
}
EActiveTimerReturnType SGPAAQuickStartView::TickQuickStart(double Time, float Delta)
{
    QuickStart->Tick();
    return QuickStart->Running() ? EActiveTimerReturnType::Continue : EActiveTimerReturnType::Stop;
}

SGPAAQuickStartView::~SGPAAQuickStartView()
{
    if (QuickStart)
        QuickStart->Cancel();
}
#undef LOCTEXT_NAMESPACE
