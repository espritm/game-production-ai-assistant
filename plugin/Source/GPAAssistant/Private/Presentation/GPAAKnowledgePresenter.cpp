#include "Presentation/GPAAKnowledgePresenter.h"

FString FGPAAKnowledgePresenter::FormatKnowledgeStatus(const FGPAAKnowledgeStatus& Status)
{
    if (!Status.bHasIndex)
    {
        return TEXT("No project knowledge index yet. Use Refresh Knowledge, Search, or Ask.");
    }

    return FString::Printf(
        TEXT("Files: %d (%d code, %d asset docs) | Chunks: %d | Assets synced: %d | Embedding: %s | Last index: %s"),
        Status.Statistics.FilesIndexed,
        Status.CodeFilesIndexed,
        Status.AssetDocumentsIndexed,
        Status.Statistics.ChunksIndexed,
        Status.AssetsSynced,
        Status.EmbeddingModel.IsEmpty() ? TEXT("unknown") : *Status.EmbeddingModel,
        Status.Statistics.IndexedAt.IsEmpty() ? TEXT("unknown") : *Status.Statistics.IndexedAt);
}

FString FGPAAKnowledgePresenter::FormatIndexSummary(const FGPAAIndexStatistics& Statistics)
{
    return FString::Printf(
        TEXT("Files indexed: %d | Chunks: %d | Changed files: %d | Embedded chunks: %d | Embedding: %s | Last index: %s"),
        Statistics.FilesIndexed,
        Statistics.ChunksIndexed,
        Statistics.FilesChanged,
        Statistics.ChunksEmbedded,
        Statistics.EmbeddingModel.IsEmpty() ? TEXT("unknown") : *Statistics.EmbeddingModel,
        Statistics.IndexedAt.IsEmpty() ? TEXT("unknown") : *Statistics.IndexedAt);
}

FString FGPAAKnowledgePresenter::FormatIndexActivity(const FGPAAIndexStatistics& Statistics)
{
    return FString::Printf(
        TEXT("%s %d changed file(s), %d removed file(s), %d embedded chunk(s)."),
        Statistics.bFullRebuild ? TEXT("Project knowledge rebuilt:") : TEXT("Project knowledge refreshed:"),
        Statistics.FilesChanged,
        Statistics.FilesRemoved,
        Statistics.ChunksEmbedded);
}

bool FGPAAKnowledgePresenter::DidIndexDoWork(const FGPAAIndexStatistics& Statistics)
{
    return Statistics.bFullRebuild
        || Statistics.FilesChanged > 0
        || Statistics.FilesRemoved > 0
        || Statistics.ChunksEmbedded > 0;
}
