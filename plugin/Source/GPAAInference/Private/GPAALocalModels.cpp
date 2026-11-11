// Copyright Game Production AI Assistant contributors 2026.
#include "GPAALocalModels.h"
#include "GPAALocalChat.h"
#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <winhttp.h>
#include <bcrypt.h>
#include "Windows/HideWindowsPlatformTypes.h"

namespace
{
struct FInternetHandle
{
    HINTERNET Value = nullptr;
    ~FInternetHandle() { if (Value) WinHttpCloseHandle(Value); }
};
struct FModelCacheLock
{
    HANDLE Handle = nullptr;
    bool Acquired = false;
    explicit FModelCacheLock(const FString& Path)
    {
        const FString Name = TEXT("Local\\GPAA-Model-") + FGPAACore::Hash(FPaths::ConvertRelativePathToFull(Path).Replace(TEXT("\\"), TEXT("/")).ToLower());
        Handle = CreateMutexW(nullptr, false, *Name);
        const DWORD Result = Handle ? WaitForSingleObject(Handle, 0) : WAIT_FAILED;
        Acquired = Result == WAIT_OBJECT_0 || Result == WAIT_ABANDONED;
    }
    ~FModelCacheLock() { if (Handle) { if (Acquired) ReleaseMutex(Handle); CloseHandle(Handle); } }
};
bool Cancelled(const FGPAATaskPtr& Task) { return Task && Task->Cancelled.Load(); }
bool DownloadPart(const FGPAALocalModelSpec& Spec, const FString& Path, int64 First, int64 Last,
    const FGPAATaskPtr& Task, TFunction<void(int64)> Progress, FString& Error)
{
    URL_COMPONENTS Parts{};
    Parts.dwStructSize = sizeof(Parts);
    Parts.dwHostNameLength = Parts.dwUrlPathLength = Parts.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(*Spec.Url, Spec.Url.Len(), 0, &Parts) || Parts.nScheme != INTERNET_SCHEME_HTTPS)
    { Error = TEXT("Model catalogue URL must use HTTPS."); return false; }
    const FString Host(Parts.dwHostNameLength, Parts.lpszHostName);
    const FString Resource = FString(Parts.dwUrlPathLength, Parts.lpszUrlPath) +
        FString(Parts.dwExtraInfoLength, Parts.lpszExtraInfo);
    FInternetHandle Session{WinHttpOpen(L"GPAA-LocalModel/0.3", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)};
    if (!Session.Value) { Error = TEXT("Could not open the model download connection."); return false; }
    WinHttpSetTimeouts(Session.Value, 10000, 10000, 10000, 10000);
    FInternetHandle Connection{WinHttpConnect(Session.Value, *Host, Parts.nPort, 0)};
    FInternetHandle Request{Connection.Value ? WinHttpOpenRequest(Connection.Value, L"GET", *Resource, nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr};
    const FString Headers = FString::Printf(TEXT("Range: bytes=%lld-%lld\r\nAccept-Encoding: identity\r\n"), First, Last);
    if (!Request.Value || Cancelled(Task) || !WinHttpSendRequest(Request.Value, *Headers, Headers.Len(),
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(Request.Value, nullptr))
    { Error = TEXT("Model download failed. Check your connection and retry to resume."); return false; }
    DWORD Code = 0, Length = sizeof(Code);
    WinHttpQueryHeaders(Request.Value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &Code, &Length, WINHTTP_NO_HEADER_INDEX);
    wchar_t Range[160]{}; Length = sizeof(Range);
    if (Code != 206 || !WinHttpQueryHeaders(Request.Value, WINHTTP_QUERY_CONTENT_RANGE,
        WINHTTP_HEADER_NAME_BY_INDEX, Range, &Length, WINHTTP_NO_HEADER_INDEX) ||
        !FGPAALocalModels::ValidateRange(FString(Range), First, Last, Spec.Bytes))
    { Error = TEXT("The server did not return the expected model byte range. Partial data was not appended."); return false; }
    TUniquePtr<IFileHandle> File(FPlatformFileManager::Get().GetPlatformFile().OpenWrite(*Path, true));
    if (!File) { Error = TEXT("Could not write the local model cache."); return false; }
    uint8 Buffer[64 * 1024]; int64 Received = 0;
    while (!Cancelled(Task) && Received < Last - First + 1)
    {
        DWORD Read = 0;
        const DWORD Want = static_cast<DWORD>(FMath::Min<int64>(sizeof(Buffer), Last - First + 1 - Received));
        if (!WinHttpReadData(Request.Value, Buffer, Want, &Read) || Read == 0 || !File->Write(Buffer, Read))
        { Error = TEXT("Model transfer interrupted. Retry Download to resume."); return false; }
        Received += Read;
        Progress(First + Received);
    }
    File->Flush();
    return !Cancelled(Task) && Received == Last - First + 1;
}
}

FGPAALocalModels& FGPAALocalModels::Get() { static FGPAALocalModels Service; return Service; }
const TArray<FGPAALocalModelSpec>& FGPAALocalModels::Catalogue()
{
    static const TArray<FGPAALocalModelSpec> Models = {
        {TEXT("qwen2.5-coder-1.5b-q4km"), TEXT("Qwen2.5 Coder 1.5B Instruct — Light (Q4_K_M)"),
         TEXT("qwen2.5-coder-1.5b-instruct-q4_k_m.gguf"),
         TEXT("https://huggingface.co/Qwen/Qwen2.5-Coder-1.5B-Instruct-GGUF/resolve/f86cb2c1fa58255f8052cc32aeede1b7482d4361/qwen2.5-coder-1.5b-instruct-q4_k_m.gguf"),
         TEXT("cc324af070c2ecbfd324a30884d2f951a7ff756aba85cb811a6ec436933bb046"), 1117320768LL},
        {TEXT("qwen2.5-coder-7b-q4km"), TEXT("Qwen2.5 Coder 7B Instruct — Larger (Q4_K_M)"),
         TEXT("qwen2.5-coder-7b-instruct-q4_k_m.gguf"),
         TEXT("https://huggingface.co/Qwen/Qwen2.5-Coder-7B-Instruct-GGUF/resolve/13fb94bfda8c8cf22497dc57b78f391a9acb426a/qwen2.5-coder-7b-instruct-q4_k_m.gguf"),
         TEXT("509287f78cb4d4cf6b3843734733b914b2c158e43e22a7f4bf5e963800894d3c"), 4683073536LL}
    };
    return Models;
}
const FGPAALocalModelSpec* FGPAALocalModels::Find(const FString& Id)
{ for (const auto& Spec : Catalogue()) if (Spec.Id == Id) return &Spec; return nullptr; }
FString FGPAALocalModels::CacheDirectory()
{ return FPaths::ConvertRelativePathToFull(FPlatformProcess::UserSettingsDir()) / TEXT("GPAA/Models"); }
FString FGPAALocalModels::ModelPath(const FGPAALocalModelSpec& Spec, const FString& Directory)
{ return (Directory.IsEmpty() ? CacheDirectory() : Directory) / Spec.FileName; }
bool FGPAALocalModels::ValidateRange(const FString& Header, int64 First, int64 Last, int64 Total)
{ return Header.TrimStartAndEnd() == FString::Printf(TEXT("bytes %lld-%lld/%lld"), First, Last, Total); }
bool FGPAALocalModels::ValidateFile(const FString& Path, const FGPAALocalModelSpec* Spec,
    const FGPAATaskPtr& Task, FString& Error)
{
    TUniquePtr<IFileHandle> File(FPlatformFileManager::Get().GetPlatformFile().OpenRead(*Path));
    if (!File || File->Size() < 64 || File->Size() > 16LL * 1024 * 1024 * 1024)
    { Error = TEXT("Local model is missing or invalid. Download a model or select a supported GGUF in Settings."); return false; }
    uint8 Header[8]{};
    if (!File->Read(Header, sizeof(Header)) || FMemory::Memcmp(Header, "GGUF", 4) != 0 ||
        (Header[4] != 2 && Header[4] != 3) || Header[5] || Header[6] || Header[7])
    { Error = TEXT("The selected file is not a supported GGUF model."); return false; }
    if (!Spec) return true; // User-selected GGUF: runtime/template validation follows on the worker.
    if (File->Size() != Spec->Bytes)
    { Error = TEXT("Local model size does not match the pinned catalogue. Download it again."); return false; }
    File->Seek(0);
    BCRYPT_ALG_HANDLE Algorithm = nullptr; BCRYPT_HASH_HANDLE Hash = nullptr;
    if (BCryptOpenAlgorithmProvider(&Algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 ||
        BCryptCreateHash(Algorithm, &Hash, nullptr, 0, nullptr, 0, 0) < 0)
    { if (Algorithm) BCryptCloseAlgorithmProvider(Algorithm, 0); Error = TEXT("SHA256 verification is unavailable."); return false; }
    TArray<uint8> Buffer; Buffer.SetNumUninitialized(1024 * 1024);
    bool Ok = true; int64 Remaining = File->Size();
    while (Remaining > 0 && !Cancelled(Task))
    {
        const int32 Count = static_cast<int32>(FMath::Min<int64>(Remaining, Buffer.Num()));
        if (!File->Read(Buffer.GetData(), Count) || BCryptHashData(Hash, Buffer.GetData(), Count, 0) < 0) { Ok = false; break; }
        Remaining -= Count;
    }
    uint8 Digest[32]{};
    Ok = Ok && Remaining == 0 && BCryptFinishHash(Hash, Digest, sizeof(Digest), 0) >= 0;
    BCryptDestroyHash(Hash); BCryptCloseAlgorithmProvider(Algorithm, 0);
    Ok = Ok && !Cancelled(Task) && BytesToHex(Digest, sizeof(Digest)).Equals(Spec->Sha256, ESearchCase::IgnoreCase);
    if (!Ok) Error = Cancelled(Task) ? TEXT("Model verification cancelled.") : TEXT("Model SHA256 does not match. Remove the cached model and download it again.");
    return Ok;
}
void FGPAALocalModels::SetStatus(const FString& Value) { FScopeLock Lock(&Mutex); Message = Value; }
FString FGPAALocalModels::Status() const { FScopeLock Lock(&Mutex); return Message; }
bool FGPAALocalModels::IsBusy() const { return Work.IsValid() && !Work.IsReady(); }
void FGPAALocalModels::Cancel() { if (Task) Task->Cancelled.Store(true); if (IsBusy()) SetStatus(TEXT("Cancelling model download...")); }
void FGPAALocalModels::Shutdown() { Stopping = true; Cancel(); if (Work.IsValid()) Work.Wait(); }
bool FGPAALocalModels::Download(const FString& Id, const FString& Directory)
{
    const auto* Found = Find(Id);
    if (Stopping || IsBusy() || !Found) return false;
    const FGPAALocalModelSpec Spec = *Found;
    Task = MakeShared<FGPAATask, ESPMode::ThreadSafe>(); const auto Current = Task;
    SetStatus(TEXT("Preparing model download..."));
    Work = Async(EAsyncExecution::ThreadPool, [this, Spec, Current, Directory]()
    {
        const FString Path = ModelPath(Spec, Directory), Partial = Path + TEXT(".partial");
        FModelCacheLock CacheLock(Path);
        if (!CacheLock.Acquired) { SetStatus(TEXT("This model cache is being managed by another Unreal instance. Retry after it finishes.")); return; }
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
        FString Error;
        if (IFileManager::Get().FileExists(*Path))
        { SetStatus(ValidateFile(Path, &Spec, Current, Error) ? TEXT("Ready — model already verified.") : Error); return; }
        int64 Position = FMath::Max<int64>(0, IFileManager::Get().FileSize(*Partial));
        if (Position > Spec.Bytes) { SetStatus(TEXT("Partial file exceeds the model size. Use Remove Downloaded Model and retry.")); return; }
        uint64 Total = 0, Free = 0;
        if (FPlatformMisc::GetDiskTotalAndFreeSpace(FPaths::GetPath(Path), Total, Free) && Free < static_cast<uint64>(Spec.Bytes - Position + 256LL * 1024 * 1024))
        { SetStatus(TEXT("Not enough disk space for the selected model.")); return; }
        int32 LastPercent = -1;
        while (Position < Spec.Bytes && !Cancelled(Current))
        {
            const int64 Last = FMath::Min<int64>(Spec.Bytes - 1, Position + 8 * 1024 * 1024 - 1);
            if (!DownloadPart(Spec, Partial, Position, Last, Current, [this, &LastPercent, &Spec](int64 Bytes)
                {
                    const int32 Percent = static_cast<int32>(Bytes * 100 / Spec.Bytes);
                    if (Percent != LastPercent) { LastPercent = Percent; SetStatus(FString::Printf(TEXT("Downloading model: %d%% (%lld / %lld MiB)"), Percent, Bytes / 1048576, Spec.Bytes / 1048576)); }
                }, Error)) break;
            Position = Last + 1;
        }
        if (Cancelled(Current)) { SetStatus(TEXT("Download cancelled. Click Download to resume.")); return; }
        if (!Error.IsEmpty()) { SetStatus(Error); return; }
        SetStatus(TEXT("Verifying model SHA256..."));
        if (!ValidateFile(Partial, &Spec, Current, Error)) { SetStatus(Error); return; }
        if (!IFileManager::Get().Move(*Path, *Partial, false, false)) { SetStatus(TEXT("Could not activate the verified model.")); return; }
        SetStatus(TEXT("Ready — local model downloaded and verified."));
    });
    return true;
}
bool FGPAALocalModels::Remove(const FString& Id)
{
    const auto* Spec = Find(Id);
    if (IsBusy() || FGPAALocalChat::IsAnyBusy() || !Spec) return false;
    // Only fixed catalogue filenames beneath the managed cache can be removed.
    const FString Path = ModelPath(*Spec);
    FModelCacheLock CacheLock(Path);
    if (!CacheLock.Acquired) { SetStatus(TEXT("Another Unreal instance is managing this model. Retry later.")); return false; }
    const bool Ok = IFileManager::Get().Delete(*Path, false, true) && IFileManager::Get().Delete(*(Path + TEXT(".partial")), false, true);
    SetStatus(Ok ? TEXT("Downloaded model removed. User-selected model files were not deleted.") : TEXT("Could not remove the cached model."));
    return Ok;
}
