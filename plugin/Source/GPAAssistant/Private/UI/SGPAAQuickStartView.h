// Copyright Game Production AI Assistant contributors 2026.
#pragma once
#include "Widgets/SCompoundWidget.h"

class FGPAAQuickStart;
class SGPAAQuickStartView final : public SCompoundWidget
{
  public:
    SLATE_BEGIN_ARGS(SGPAAQuickStartView) {}
    SLATE_END_ARGS()
    void Construct(const FArguments &InArgs);
    virtual ~SGPAAQuickStartView() override;
    EVisibility GetQuickStartButtonVisibility() const;

  private:
    TSharedPtr<FGPAAQuickStart> QuickStart;
    FReply OnQuickStartClicked();
    EActiveTimerReturnType TickQuickStart(double Time, float Delta);
};
