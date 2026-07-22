#pragma once

#include "CoreMinimal.h"
#include "Models/GPAAApiModels.h"

class FGPAAChatSettingsValidator
{
public:
    static bool Validate(const FGPAAChatSettings& Settings, FString& OutError);
};
