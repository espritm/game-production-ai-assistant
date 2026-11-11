// Copyright Game Production AI Assistant contributors 2026.
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "GPAACore.h"
#include "GPAAEmbeddings.h"
#include "Infrastructure/GPAANativeStorage.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Services/GPAAAssetMetadataRequestBuilder.h"

// Opt-in only: the host must contain the owner's real project assets.
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FGPAAProjectAssetsTest, "GPAA.Acceptance.ProjectAssets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
void FGPAAProjectAssetsTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
    if (FParse::Param(FCommandLine::Get(), TEXT("GPAARealAssets")))
    {
        Names.Add(TEXT("LocalAssetRetrieval"));
        Commands.Add(TEXT("LocalAssetRetrieval"));
    }
}
bool FGPAAProjectAssetsTest::RunTest(const FString& Parameters)
{
    auto& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    Registry.Get().SearchAllAssets(true);
    const FString Snapshot = FGPAAAssetMetadataRequestBuilder::BuildSyncRequest(FPaths::ProjectDir());
    TSharedPtr<FJsonObject> Root;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Snapshot), Root))
    {
        AddError(TEXT("Cannot parse project asset snapshot."));
        return false;
    }
    TArray<FGPAADocument> Documents;
    for (const auto& Value : Root->GetArrayField(TEXT("assets")))
    {
        FGPAADocument Document;
        Document.Kind = TEXT("Assets");
        Document.Path = TEXT("asset:") + Value->AsObject()->GetStringField(TEXT("objectPath"));
        FJsonSerializer::Serialize(Value->AsObject().ToSharedRef(), TJsonWriterFactory<>::Create(&Document.Content));
        Documents.Add(MoveTemp(Document));
    }
    if (Documents.Num() < 100)
    {
        AddError(TEXT("Acceptance requires at least 100 real project asset records."));
        return false;
    }
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("GPAA/Acceptance");
    IFileManager::Get().MakeDirectory(*Directory, true);
    FGPAANativeStorage Store(Directory / TEXT("assets.sqlite"));
    FGPAAMiniLM Model(IPluginManager::Get().FindPlugin(TEXT("GameProductionAIAssistant"))->GetBaseDir());
    const auto Task = MakeShared<FGPAATask, ESPMode::ThreadSafe>();
    FGPAAIndexReport IndexReport;
    FString Error;
    const double Start = FPlatformTime::Seconds();
    if (!FGPAACore::Index(Documents, Model, Store, Task, IndexReport, Error))
    {
        AddError(Error);
        return false;
    }
    auto Report = MakeShared<FJsonObject>();
    Report->SetNumberField(TEXT("assets"), Documents.Num());
    Report->SetNumberField(TEXT("indexSeconds"), FPlatformTime::Seconds() - Start);
    const TArray<FString> Queries = {TEXT("pistol reload animation"), TEXT("rifle equip animation"),
        TEXT("death animation falling backwards"), TEXT("animation de rechargement du pistolet"),
        TEXT("animation pour equiper le fusil"), TEXT("animation de mort vers l'arriere")};
    const TArray<FString> Expected = {TEXT("MM_Pistol_Reload"), TEXT("MM_Rifle_Equip"), TEXT("MM_Death_Back_01")};
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (int32 Q = 0; Q < Queries.Num(); ++Q)
    {
        TArray<FGPAAPassage> Hits;
        if (!FGPAACore::Search(Queries[Q], TEXT("Assets"), 10, Model, Store, Task, Hits, Error))
        {
            AddError(Error);
            return false;
        }
        auto Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("query"), Queries[Q]);
        Row->SetStringField(TEXT("language"), Q < 3 ? TEXT("en") : TEXT("fr"));
        Row->SetStringField(TEXT("expectedAsset"), Expected[Q % 3]);
        const bool Available = Documents.ContainsByPredicate([&](const FGPAADocument& Document)
        {
            return Document.Path.EndsWith(TEXT(".") + Expected[Q % 3]);
        });
        Row->SetBoolField(TEXT("expectedAssetAvailable"), Available);
        TestTrue(TEXT("Expected asset is available in the registry; otherwise relevance measurement is invalid"), Available);
        int32 Rank = 0;
        TArray<TSharedPtr<FJsonValue>> Paths;
        for (int32 I = 0; I < Hits.Num(); ++I)
        {
            Paths.Add(MakeShared<FJsonValueString>(Hits[I].Path));
            if (Hits[I].Path.EndsWith(TEXT(".") + Expected[Q % 3])) Rank = I + 1;
        }
        Row->SetNumberField(TEXT("rankWithin10"), Rank);
        Row->SetArrayField(TEXT("top10"), Paths);
        Rows.Add(MakeShared<FJsonValueObject>(Row));
        AddInfo(FString::Printf(TEXT("%s: expected rank within 10 = %d (0 = absent)"), *Queries[Q], Rank));
        // Report relevance honestly; weak retrieval is a product finding, not a transport failure.
        TestTrue(TEXT("Asset search returns references"), Hits.Num() > 0);
    }
    Report->SetArrayField(TEXT("queries"), Rows);
    FString Json;
    FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
    TestTrue(TEXT("Acceptance report saved"), FFileHelper::SaveStringToFile(Json, *(Directory / TEXT("asset-search.json"))));
    return true;
}
#endif
