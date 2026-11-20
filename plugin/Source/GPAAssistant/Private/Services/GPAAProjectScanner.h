// Copyright Game Production AI Assistant contributors 2026.
#pragma once
#include "GPAACore.h"

// Collect supported source files and the supplied asset snapshot; no retrieval or provider state.
namespace GPAAProjectScanner
{
bool Scan(const FString &Root, const FString &Assets, const FGPAATaskPtr &Task, TArray<FGPAADocument> &Documents,
          FString &Error);
}
