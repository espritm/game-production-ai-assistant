// Copyright Game Production AI Assistant contributors 2026.
#include "Infrastructure/GPAANativeStorage.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "SQLitePreparedStatement.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <wincred.h>
#include "Windows/HideWindowsPlatformTypes.h"

namespace
{
bool ExecuteBatch(FSQLiteDatabase &Database, const TCHAR *Sql)
{
    TArray<FString> Statements;
    FString(Sql).ParseIntoArray(Statements, TEXT(";"), true);
    for (const FString &Statement : Statements)
        if (!Database.Execute(*Statement))
            return false;
    return true;
}
} // namespace

FGPAANativeStorage::FGPAANativeStorage(const FString &DatabasePath)
{
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(DatabasePath), true);
    if (!Database.Open(*DatabasePath))
    {
        InitializationError = Database.GetLastError();
        return;
    }
    if (!ExecuteBatch(Database, TEXT("PRAGMA busy_timeout=5000; PRAGMA journal_mode=WAL; PRAGMA foreign_keys=ON;")) ||
        !ExecuteBatch(
            Database,
            TEXT("CREATE TABLE IF NOT EXISTS metadata (id INTEGER PRIMARY KEY CHECK(id=1), identity TEXT NOT NULL);"
                 "CREATE TABLE IF NOT EXISTS files (path TEXT PRIMARY KEY, hash TEXT NOT NULL);"
                 "CREATE TABLE IF NOT EXISTS passages (path TEXT NOT NULL, content TEXT NOT NULL, kind TEXT NOT NULL, "
                 "first_line INTEGER, last_line INTEGER, vector BLOB NOT NULL);"
                 "CREATE TABLE IF NOT EXISTS history (id INTEGER PRIMARY KEY, question TEXT NOT NULL, answer TEXT NOT "
                 "NULL, created TEXT NOT NULL, complete INTEGER NOT NULL);")))
        InitializationError = Database.GetLastError();
    int32 Version = 0;
    if (!Database.GetUserVersion(Version))
        InitializationError = Database.GetLastError();
    if (Version > 4)
        InitializationError = TEXT("This index was created by a newer plugin. Restore that version before opening it.");
    else if (Version < 2 && InitializationError.IsEmpty())
    {
        if (!Database.Execute(TEXT("BEGIN IMMEDIATE;")))
        {
            InitializationError = Database.GetLastError();
            return;
        }
        if (!Database.Execute(TEXT("ALTER TABLE history ADD COLUMN sources TEXT NOT NULL DEFAULT '[]';")) ||
            !Database.SetUserVersion(2) || !Database.Execute(TEXT("COMMIT;")))
        {
            InitializationError = Database.GetLastError();
            Database.Execute(TEXT("ROLLBACK;"));
        }
        else Version = 2;
    }
    if (Version < 3 && InitializationError.IsEmpty())
    {
        if (!Database.Execute(TEXT("BEGIN IMMEDIATE;")))
        {
            InitializationError = Database.GetLastError();
            return;
        }
        if (!ExecuteBatch(Database, TEXT("ALTER TABLE history ADD COLUMN kind TEXT NOT NULL DEFAULT 'Prompt';"
                                        "ALTER TABLE history ADD COLUMN scope TEXT NOT NULL DEFAULT 'All';")) ||
            !Database.SetUserVersion(3) || !Database.Execute(TEXT("COMMIT;")))
        {
            InitializationError = Database.GetLastError();
            Database.Execute(TEXT("ROLLBACK;"));
        }
        else Version = 3;
    }
    if (Version < 4 && InitializationError.IsEmpty())
    {
        if (!Database.Execute(TEXT("BEGIN IMMEDIATE;")))
        { InitializationError = Database.GetLastError(); return; }
        if (!ExecuteBatch(Database, TEXT("ALTER TABLE history ADD COLUMN provider TEXT NOT NULL DEFAULT '';"
                                        "ALTER TABLE history ADD COLUMN model TEXT NOT NULL DEFAULT '';")) ||
            !Database.SetUserVersion(4) || !Database.Execute(TEXT("COMMIT;")))
        { InitializationError = Database.GetLastError(); Database.Execute(TEXT("ROLLBACK;")); }
    }
}
bool FGPAANativeStorage::IsReady(FString &Error) const
{
    Error = InitializationError;
    return Database.IsValid() && Error.IsEmpty();
}
FGPAANativeStorage::~FGPAANativeStorage()
{
    if (Database.IsValid())
        Database.Close();
}
bool FGPAANativeStorage::Load(FGPAASnapshot &Result, FString &Error)
{
    if (!IsReady(Error))
        return false;
    if (!Database.Execute(TEXT("BEGIN;")))
    {
        Error = Database.GetLastError();
        return false;
    }
    struct FReadTransaction
    {
        FSQLiteDatabase &Db;
        ~FReadTransaction()
        {
            Db.Execute(TEXT("ROLLBACK;"));
        }
    } ReadTransaction{Database};
    Result = {};
    auto Meta = Database.PrepareStatement(TEXT("SELECT identity FROM metadata WHERE id=1"));
    if (!Meta.IsValid())
    {
        Error = Database.GetLastError();
        return false;
    }
    if (Meta.Step() == ESQLitePreparedStatementStepResult::Row)
        Meta.GetColumnValueByIndex(0, Result.Identity);
    auto Files = Database.PrepareStatement(TEXT("SELECT path, hash FROM files ORDER BY path"));
    if (!Files.IsValid())
    {
        Error = Database.GetLastError();
        return false;
    }
    ESQLitePreparedStatementStepResult Step;
    while ((Step = Files.Step()) == ESQLitePreparedStatementStepResult::Row)
    {
        FString Path;
        FGPAAStoredFile File;
        Files.GetColumnValueByIndex(0, Path);
        Files.GetColumnValueByIndex(1, File.Hash);
        Result.Files.Add(Path, MoveTemp(File));
    }
    if (Step != ESQLitePreparedStatementStepResult::Done)
    {
        Error = Database.GetLastError();
        return false;
    }
    auto Rows = Database.PrepareStatement(
        TEXT("SELECT path,content,kind,first_line,last_line,vector FROM passages ORDER BY path,first_line"));
    if (!Rows.IsValid())
    {
        Error = Database.GetLastError();
        return false;
    }
    while ((Step = Rows.Step()) == ESQLitePreparedStatementStepResult::Row)
    {
        FGPAAPassage P;
        TArray<uint8> Bytes;
        Rows.GetColumnValueByIndex(0, P.Path);
        Rows.GetColumnValueByIndex(1, P.Content);
        Rows.GetColumnValueByIndex(2, P.Kind);
        Rows.GetColumnValueByIndex(3, P.FirstLine);
        Rows.GetColumnValueByIndex(4, P.LastLine);
        Rows.GetColumnValueByIndex(5, Bytes);
        if (Bytes.Num() != 384 * sizeof(float) || !Result.Files.Contains(P.Path))
        {
            Error = TEXT("Local index is inconsistent. Rebuild its database.");
            return false;
        }
        P.Vector.SetNum(384);
        FMemory::Memcpy(P.Vector.GetData(), Bytes.GetData(), Bytes.Num());
        Result.Files[P.Path].Passages.Add(MoveTemp(P));
    }
    if (Step != ESQLitePreparedStatementStepResult::Done)
    {
        Error = Database.GetLastError();
        return false;
    }
    return true;
}
bool FGPAANativeStorage::Replace(const FGPAASnapshot &Value, FString &Error)
{
    if (!IsReady(Error))
        return false;
    if (!Database.Execute(TEXT("BEGIN IMMEDIATE;")))
    {
        Error = Database.GetLastError();
        return false;
    }
    bool Ok = ExecuteBatch(Database, TEXT("DELETE FROM passages; DELETE FROM files; DELETE FROM metadata;"));
    {
        auto Meta = Database.PrepareStatement(TEXT("INSERT INTO metadata(id,identity) VALUES(1,?1)"));
        Ok = Ok && Meta.IsValid() && Meta.SetBindingValueByIndex(1, Value.Identity) &&
             Meta.Step() == ESQLitePreparedStatementStepResult::Done;
        auto File = Database.PrepareStatement(TEXT("INSERT INTO files(path,hash) VALUES(?1,?2)"));
        auto Row = Database.PrepareStatement(
            TEXT("INSERT INTO passages(path,content,kind,first_line,last_line,vector) VALUES(?1,?2,?3,?4,?5,?6)"));
        Ok = Ok && File.IsValid() && Row.IsValid();
        for (const auto &Pair : Value.Files)
        {
            if (!Ok)
                break;
            File.Reset();
            Ok = File.SetBindingValueByIndex(1, Pair.Key) && File.SetBindingValueByIndex(2, Pair.Value.Hash) &&
                 File.Step() == ESQLitePreparedStatementStepResult::Done;
            for (const FGPAAPassage &P : Pair.Value.Passages)
            {
                if (!Ok)
                    break;
                Row.Reset();
                Ok = P.Vector.Num() == 384 && Row.SetBindingValueByIndex(1, P.Path) &&
                     Row.SetBindingValueByIndex(2, P.Content) && Row.SetBindingValueByIndex(3, P.Kind) &&
                     Row.SetBindingValueByIndex(4, P.FirstLine) && Row.SetBindingValueByIndex(5, P.LastLine) &&
                     Row.SetBindingValueByIndex(6, P.Vector.GetData(), P.Vector.Num() * sizeof(float)) &&
                     Row.Step() == ESQLitePreparedStatementStepResult::Done;
            }
        }
    }
    if (Ok && Database.Execute(TEXT("COMMIT;")))
        return true;
    Error = Database.GetLastError();
    if (Error.IsEmpty())
        Error = TEXT("Index replacement was rejected because its vector layout is incompatible.");
    Database.Execute(TEXT("ROLLBACK;"));
    return false;
}
bool FGPAANativeStorage::AddHistory(const FString &Question, const FString &Answer, bool Complete, FString &Error,
                                    const FString &SourcesJson, const FString& Kind, const FString& Scope, const FString& Provider, const FString& Model)
{
    if (!IsReady(Error))
        return false;
    auto Row = Database.PrepareStatement(
        TEXT("INSERT INTO history(question,answer,created,complete,sources,kind,scope,provider,model) VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9)"));
    if (Row.IsValid() && Row.SetBindingValueByIndex(1, Question) && Row.SetBindingValueByIndex(2, Answer) &&
        Row.SetBindingValueByIndex(3, FDateTime::UtcNow().ToIso8601()) &&
        Row.SetBindingValueByIndex(4, Complete ? 1 : 0) && Row.SetBindingValueByIndex(5, SourcesJson) &&
        Row.SetBindingValueByIndex(6, Kind) && Row.SetBindingValueByIndex(7, Scope) &&
        Row.SetBindingValueByIndex(8, Provider) && Row.SetBindingValueByIndex(9, Model) &&
        Row.Step() == ESQLitePreparedStatementStepResult::Done)
        return true;
    Error = Database.GetLastError();
    return false;
}
FString FGPAANativeStorage::History(FString &Error)
{
    if (!IsReady(Error))
        return {};
    auto Rows = Database.PrepareStatement(
        TEXT("SELECT question,answer,created,complete,sources,kind,scope,provider,model FROM history ORDER BY id DESC LIMIT 30"));
    if (!Rows.IsValid())
    {
        Error = Database.GetLastError();
        return {};
    }
    FString Result;
    ESQLitePreparedStatementStepResult Step;
    while ((Step = Rows.Step()) == ESQLitePreparedStatementStepResult::Row)
    {
        FString Q, A, Time, Kind, Scope, Provider, Model;
        int32 Complete = 0;
        Rows.GetColumnValueByIndex(0, Q);
        Rows.GetColumnValueByIndex(1, A);
        Rows.GetColumnValueByIndex(2, Time);
        Rows.GetColumnValueByIndex(3, Complete);
        Rows.GetColumnValueByIndex(5, Kind);
        Rows.GetColumnValueByIndex(6, Scope);
        Rows.GetColumnValueByIndex(7, Provider);
        Rows.GetColumnValueByIndex(8, Model);
        const bool Search = Kind == TEXT("Search");
        const FString ScopeLabel = Scope == TEXT("Assets") ? TEXT("Asset") : (Scope == TEXT("Code") ? TEXT("Code") : TEXT("Asset and Code"));
        Result += Time + (Search ? TEXT(" [Search — ") + ScopeLabel + TEXT("]") : TEXT(" [Prompt]")) +
                  (Complete ? TEXT("") : TEXT(" [interrupted]")) +
                  (Search ? TEXT("\nSearch: ") : TEXT("\nQ: ")) + Q +
                  (Search ? TEXT("\nResults: ") : TEXT("\nA: ")) + A + TEXT("\n\n");
        if (!Search && !Provider.IsEmpty()) Result += TEXT("Provider: ") + Provider + TEXT(" | Model: ") + Model + TEXT("\n\n");
        FString Sources;
        Rows.GetColumnValueByIndex(4, Sources);
        if (Sources != TEXT("[]"))
            Result += TEXT("Sources: ") + Sources + TEXT("\n\n");
    }
    if (Step != ESQLitePreparedStatementStepResult::Done)
        Error = Database.GetLastError();
    return Result.IsEmpty() ? TEXT("No local activity yet.") : Result;
}

bool FGPAANativeStorage::ImportLegacyHistory(const FString &JsonPath, FString &Error)
{
    FString Json;
    TSharedPtr<FJsonObject> Document;
    if (IFileManager::Get().FileSize(*JsonPath) > 16 * 1024 * 1024 || !FFileHelper::LoadFileToString(Json, *JsonPath) ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Document) || !Document)
    {
        Error = TEXT("Select a valid legacy chat-history.json file under 16 MiB.");
        return false;
    }
    const TArray<TSharedPtr<FJsonValue>> *Entries = nullptr;
    if (!Document->TryGetArrayField(TEXT("entries"), Entries) || Entries->Num() > 100)
    {
        Error = TEXT("Legacy history format is invalid.");
        return false;
    }
    if (!IsReady(Error))
        return false;
    if (!Database.Execute(TEXT("BEGIN IMMEDIATE;")))
    {
        Error = Database.GetLastError();
        return false;
    }
    bool Ok = true;
    for (int32 I = Entries->Num() - 1; I >= 0 && Ok; --I)
    {
        auto Entry = (*Entries)[I]->AsObject();
        FString Q, A;
        if (!Entry || !Entry->TryGetStringField(TEXT("question"), Q) || !Entry->TryGetStringField(TEXT("answer"), A))
        {
            Error = TEXT("Legacy history entry is invalid.");
            Ok = false;
            break;
        }
        Ok = AddHistory(Q, A, true, Error);
    }
    if (Ok && Database.Execute(TEXT("COMMIT;")))
        return true;
    if (Error.IsEmpty())
        Error = Database.GetLastError();
    Database.Execute(TEXT("ROLLBACK;"));
    return false;
}

FString FGPAANativeSecrets::Scope(const FString &Endpoint)
{
    FString Normalized = Endpoint.TrimStartAndEnd().ToLower();
    Normalized.RemoveFromEnd(TEXT("/"));
    return TEXT("GPAA/Provider/") + FGPAACore::Hash(Normalized);
}
bool FGPAANativeSecrets::Write(const FString &Target, const FString &Secret, FString &Error)
{
    if (Secret.IsEmpty())
        return Remove(Target, Error);
    CREDENTIALW Credential{};
    Credential.Type = CRED_TYPE_GENERIC;
    Credential.TargetName = const_cast<LPWSTR>(*Target);
    Credential.UserName = const_cast<LPWSTR>(L"GPAA");
    Credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<TCHAR *>(*Secret));
    Credential.CredentialBlobSize = Secret.Len() * sizeof(TCHAR);
    Credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    if (Credential.CredentialBlobSize > CRED_MAX_CREDENTIAL_BLOB_SIZE || !CredWriteW(&Credential, 0))
    {
        Error = TEXT("Could not save the personal key in Windows Credential Manager.");
        return false;
    }
    return true;
}
FString FGPAANativeSecrets::Read(const FString &Target)
{
    PCREDENTIALW Credential = nullptr;
    if (!CredReadW(*Target, CRED_TYPE_GENERIC, 0, &Credential))
        return {};
    FString Secret(Credential->CredentialBlobSize / sizeof(TCHAR),
                   reinterpret_cast<const TCHAR *>(Credential->CredentialBlob));
    CredFree(Credential);
    return Secret;
}
bool FGPAANativeSecrets::Remove(const FString &Target, FString &Error)
{
    if (CredDeleteW(*Target, CRED_TYPE_GENERIC, 0) || GetLastError() == ERROR_NOT_FOUND)
        return true;
    Error = TEXT("Could not remove the personal key from Windows Credential Manager.");
    return false;
}
