// Copyright Game Production AI Assistant contributors 2026.
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Infrastructure/GPAANativeStorage.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPAAHistoryMigrationTest,"GPAA.Native.Storage.MixedHistoryMigration",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGPAAHistoryMigrationTest::RunTest(const FString& Parameters)
{
    const FString Directory=FPaths::ProjectSavedDir()/TEXT("GPAA/HistoryMigration")/FGuid::NewGuid().ToString();
    IFileManager::Get().MakeDirectory(*Directory,true);
    const FString Path=Directory/TEXT("index.sqlite");
    {
        FSQLiteDatabase Old;
        if(!Old.Open(*Path)) return false;
        TestTrue(TEXT("Create version-2 history fixture"),Old.Execute(TEXT("CREATE TABLE history(id INTEGER PRIMARY KEY,question TEXT NOT NULL,answer TEXT NOT NULL,created TEXT NOT NULL,complete INTEGER NOT NULL,sources TEXT NOT NULL DEFAULT '[]')")));
        TestTrue(TEXT("Preserve an existing prompt"),Old.Execute(TEXT("INSERT INTO history VALUES(1,'Old prompt','Old answer','2026-01-01',1,'[]')")));
        Old.SetUserVersion(2); Old.Close();
    }
    FString Error;
    {
        FGPAANativeStorage Store(Path);
        TestTrue(TEXT("Upgrade opens existing history"),Store.IsReady(Error));
        TestTrue(TEXT("Old prompt retained after migration"),Store.History(Error).Contains(TEXT("Old answer")));
        TestTrue(TEXT("Local prompt preserves provider/model identity"), Store.AddHistory(TEXT("Local question"), TEXT("Local answer"), true, Error, TEXT("[]"), TEXT("Prompt"), TEXT("All"), TEXT("Local"), TEXT("qwen-fixture")));
        TestTrue(TEXT("Search entry writes query, scope and references"),Store.AddHistory(TEXT("pistol reload"),TEXT("1 result(s)."),true,Error,TEXT("[\"asset:/Game/Pistol.Pistol\"]"),TEXT("Search"),TEXT("Assets")));
        TestTrue(TEXT("Empty search is recorded too"),Store.AddHistory(TEXT("no match"),TEXT("0 result(s)."),true,Error,TEXT("[]"),TEXT("Search"),TEXT("Code")));
    }
    {
        FGPAANativeStorage Reopened(Path);
        const FString History=Reopened.History(Error);
        TestTrue(TEXT("Local model identity survives migration and reopen"), History.Contains(TEXT("Provider: Local | Model: qwen-fixture")));
        TestTrue(TEXT("Prompts distinguish their kind"),History.Contains(TEXT("[Prompt]")));
        TestTrue(TEXT("Search scope and query survive reopen"),History.Contains(TEXT("[Search — Asset]")) && History.Contains(TEXT("pistol reload")));
        TestTrue(TEXT("Search references persist"),History.Contains(TEXT("asset:/Game/Pistol.Pistol")));
        TestTrue(TEXT("No-result searches persist"),History.Contains(TEXT("[Search — Code]")) && History.Contains(TEXT("0 result(s).")));
        TestTrue(TEXT("Earlier conversation still readable"),History.Contains(TEXT("Old prompt")) && History.Contains(TEXT("Old answer")));
    }
    return true;
}
#endif
