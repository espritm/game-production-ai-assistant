// Copyright Game Production AI Assistant contributors 2026.
#include "Services/GPAAProjectScanner.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
bool GPAAProjectScanner::Scan(const FString &Root, const FString &Assets, const FGPAATaskPtr &Task,
                              TArray<FGPAADocument> &Documents, FString &Error)
{
    const TSet<FString> Extensions = {TEXT("h"),   TEXT("hpp"),     TEXT("cpp"),     TEXT("cs"),
                                      TEXT("ini"), TEXT("uplugin"), TEXT("uproject")};
    const TSet<FString> Ignored = {TEXT(".git"),       TEXT(".vs"),          TEXT(".vscode"), TEXT(".gpaa"),
                                   TEXT("Binaries"),   TEXT("Intermediate"), TEXT("Saved"),   TEXT("DerivedDataCache"),
                                   TEXT("ThirdParty"), TEXT("node_modules")};
    const FString FullRoot = FPaths::ConvertRelativePathToFull(Root) / TEXT("");
    TArray<FString> Folders = {FPaths::ConvertRelativePathToFull(Root)};
    while (!Folders.IsEmpty())
    {
        if (Task->Cancelled.Load())
        {
            Error = TEXT("Indexing cancelled.");
            return false;
        }
        FString Folder = Folders.Pop(EAllowShrinking::No);
        TArray<FString> Names;
        IFileManager::Get().FindFiles(Names, *(Folder / TEXT("*")), false, true);
        for (const FString &Name : Names)
        {
            // Do not traverse directory links or index the assistant itself.
            const FString Path = Folder / Name;
            if (!Ignored.Contains(Name) && Name != TEXT("GameProductionAIAssistant") &&
                FPlatformFileManager::Get().GetPlatformFile().IsSymlink(*Path) != ESymlinkResult::Symlink)
                Folders.Add(Path);
        }
        Names.Reset();
        IFileManager::Get().FindFiles(Names, *(Folder / TEXT("*")), true, false);
        for (const FString &Name : Names)
        {
            if (!Extensions.Contains(FPaths::GetExtension(Name).ToLower()) ||
                Name.Contains(TEXT("EditorPerProjectUserSettings")))
                continue;
            if (Documents.Num() >= 10000)
            {
                Error = TEXT("Project exceeds the 10000-document native index limit.");
                return false;
            }
            FString Path = Folder / Name;
            if (IFileManager::Get().FileSize(*Path) > 512 * 1024)
            {
                Error = TEXT("Source file exceeds the 512 KiB limit: ") + Name;
                return false;
            }
            FGPAADocument Doc;
            Doc.Path = Path;
            FPaths::MakePathRelativeTo(Doc.Path, *FullRoot);
            Doc.Path.ReplaceInline(TEXT("\\"), TEXT("/"));
            if (!FFileHelper::LoadFileToString(Doc.Content, *Path))
            {
                Error = TEXT("Could not read ") + Doc.Path;
                return false;
            }
            Documents.Add(MoveTemp(Doc));
        }
    }
    TSharedPtr<FJsonObject> Snapshot;
    if (!Assets.IsEmpty() && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Assets), Snapshot) && Snapshot)
    {
        const TArray<TSharedPtr<FJsonValue>> *Array = nullptr;
        if (Snapshot->TryGetArrayField(TEXT("assets"), Array))
            for (const auto &Value : *Array)
            {
                const auto Asset = Value->AsObject();
                if (!Asset)
                    continue;
                FGPAADocument Doc;
                Doc.Kind = TEXT("Assets");
                FString ObjectPath;
                Asset->TryGetStringField(TEXT("objectPath"), ObjectPath);
                Doc.Path = TEXT("asset:") + ObjectPath;
                FJsonSerializer::Serialize(Asset.ToSharedRef(), TJsonWriterFactory<>::Create(&Doc.Content));
                Documents.Add(MoveTemp(Doc));
            }
    }
    Documents.Sort([](const FGPAADocument &A, const FGPAADocument &B) { return A.Path < B.Path; });
    return true;
}
