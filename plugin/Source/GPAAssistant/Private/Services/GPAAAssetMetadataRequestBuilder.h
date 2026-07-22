#pragma once

#include "CoreMinimal.h"

class FGPAAAssetMetadataRequestBuilder
{
public:
    static FString BuildSyncRequest(const FString& ProjectRoot);
};
