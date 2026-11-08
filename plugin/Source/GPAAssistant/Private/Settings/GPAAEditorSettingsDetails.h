// Copyright Game Production AI Assistant contributors 2026.
#pragma once
#include "IDetailCustomization.h"

class FGPAAEditorSettingsDetails final : public IDetailCustomization
{
public:
    static TSharedRef<IDetailCustomization> MakeInstance();
    virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
