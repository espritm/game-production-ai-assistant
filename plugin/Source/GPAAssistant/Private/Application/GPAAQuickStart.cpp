// Copyright Game Production AI Assistant contributors 2026.
#include "Application/GPAAQuickStart.h"
#include "Application/GPAANativeRuntime.h"
#include "Settings/GPAAEditorSettings.h"
#include "GPAALocalModels.h"

TWeakPtr<FGPAAQuickStart> FGPAAQuickStart::Active;
bool FGPAAQuickStart::IsRunning() { auto Current = Active.Pin(); return Current && Current->Running(); }
FGPAAQuickStart::~FGPAAQuickStart() { Cancel(); }
bool FGPAAQuickStart::Start()
{
    if (IsRunning() || FGPAALocalModels::Get().IsBusy() || FGPAALocalChat::IsAnyBusy() || FGPAANativeRuntime::Get()->IsBusy())
    { Message = TEXT("Wait for the current task to finish, then retry Quick Start."); return false; }
    Active = AsShared(); bRunning = true; Step = 0; Answer.Reset();
    Message = TEXT("Configuring local AI...");
    return true;
}
void FGPAAQuickStart::Fail(const FString& Error) { bRunning = false; Message = Error + TEXT(" You can retry Quick Start."); }
void FGPAAQuickStart::Cancel()
{
    if (!bRunning) return;
    bRunning = false;
    if (bOwnDownload) FGPAALocalModels::Get().Cancel();
    if (Chat) Chat->Cancel();
    Message = TEXT("Quick Start cancelled. Any partial download can be resumed by retrying.");
}
void FGPAAQuickStart::Tick()
{
    if (!bRunning) return;
    auto* Settings = GetMutableDefault<UGPAAEditorSettings>();
    auto& Models = FGPAALocalModels::Get();
    if (Step > 0 && (Settings->ChatProvider != TEXT("Local") ||
        Settings->LocalModelId != TEXT("qwen2.5-coder-1.5b-q4km") || !Settings->LocalModelFile.IsEmpty()))
    { Cancel(); Message = TEXT("Quick Start stopped because AI settings changed. Retry to configure Local AI."); return; }
    if (Step == 0)
    {
        Settings->LocalModelId = TEXT("qwen2.5-coder-1.5b-q4km");
        Settings->LocalModelFile.Reset(); Settings->LocalContextTokens = 4096; Settings->LocalThreads = 4;
        Settings->SelectAI(true);
        if (!Models.Download(Settings->LocalModelId)) { Fail(TEXT("Could not start the model download.")); return; }
        bOwnDownload = true; Step = 1;
    }
    if (Step == 1)
    {
        Message = Models.Status();
        if (Models.IsBusy()) return;
        bOwnDownload = false;
        if (!Models.LastDownloadSucceeded()) { Fail(Models.Status()); return; }
        Step = 2; Message = TEXT("Running a local test prompt: Reply with exactly READY.");
        FGPAAChatRequest Request;
        Request.Local = true; Request.Model = Settings->LocalModelId;
        Request.LocalModelPath = FGPAALocalModels::ModelPath(*FGPAALocalModels::Find(Request.Model));
        Request.LocalContextTokens = Settings->LocalContextTokens; Request.LocalThreads = Settings->LocalThreads;
        Request.MaxOutputTokens = 64; Request.Timeout = 120;
        Request.Prompt = TEXT("Reply with exactly READY.");
        Chat = MakeShared<FGPAALocalChat, ESPMode::ThreadSafe>();
        const TWeakPtr<FGPAAQuickStart> Weak = AsShared();
        Chat->Start(Request, [Weak](const FString& Delta)
        { if (auto Self = Weak.Pin()) if (Self->Running()) Self->Answer += Delta; },
        [Weak](bool Ok, const FString& Error)
        {
            if (auto Self = Weak.Pin()) if (Self->Running())
            {
                if (!Ok || !Self->Answer.Contains(TEXT("READY"), ESearchCase::IgnoreCase))
                { Self->Fail(Error.IsEmpty() ? TEXT("The test prompt did not return READY.") : Error); return; }
                Self->Step = 3; Self->bRunning = false;
                Self->Message = TEXT("Your environment is ready! Prompt your local AI in Prompt IA, or use Search for advanced project search without AI generation.");
            }
        });
    }
}
