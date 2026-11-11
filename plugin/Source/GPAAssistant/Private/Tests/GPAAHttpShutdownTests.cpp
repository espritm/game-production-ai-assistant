// Copyright Game Production AI Assistant contributors 2026.
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "GPAACloudChat.h"
#include "HttpModule.h"
#include "HttpServerModule.h"
#include "IHttpRouter.h"
#include "HttpServerResponse.h"
#include "Modules/ModuleManager.h"
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "IPAddress.h"
#include "Misc/CommandLine.h"

struct FGPAAHttpShutdownState
{
    TSharedPtr<IHttpRouter> Router;
    FHttpRouteHandle Route;
    TSharedPtr<FHttpResultCallback> HeldReply;
    TSharedPtr<FGPAACloudChat, ESPMode::ThreadSafe> Completed, Pending;
    TWeakPtr<FGPAACloudChat, ESPMode::ThreadSafe> CallbackOnlyOwner;
    FGPAAChatRequest Request;
    int32 Calls=0, UnexpectedCallbacks=0, Phase=0;
    bool CompletedOk=false, CompletedDone=false, PendingSeen=false;
    FString Error;
    double Deadline=0, StoppedAt=0;
};
DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FGPAAWaitForHttpShutdown,TSharedPtr<FGPAAHttpShutdownState>,State,FAutomationTestBase*,Test);
bool FGPAAWaitForHttpShutdown::Update()
{
    if(FPlatformTime::Seconds()>State->Deadline)
    {
        State->Router->UnbindRoute(State->Route);
        FGPAACloudChat::ShutdownRequests();
        Test->AddError(TEXT("Loopback shutdown fixture timed out."));
        return true;
    }
    if(State->Phase==0)
    {
        if(!State->CompletedDone) return false;
        Test->TestTrue(TEXT("A real HTTP request completed before shutdown"),State->CompletedOk);
        if(!State->Error.IsEmpty()) Test->AddError(State->Error);
        State->Pending=MakeShared<FGPAACloudChat,ESPMode::ThreadSafe>();
        State->Pending->Start(State->Request,[SharedState=State](const FString&){++SharedState->UnexpectedCallbacks;},
            [SharedState=State, KeepAlive=State->Pending](bool,const FString&){(void)KeepAlive; ++SharedState->UnexpectedCallbacks;});
        State->Phase=1;
        return false;
    }
    if(State->Phase==1)
    {
        if(!State->PendingSeen) return false;
        State->Router->UnbindRoute(State->Route);
        State->CallbackOnlyOwner=State->Pending;
        State->Pending.Reset(); // Completion delegate now owns the last provider reference.
        auto* HttpBefore=FModuleManager::GetModulePtr<FHttpModule>(TEXT("HTTP"));
        FGPAACloudChat::ShutdownRequests();
        FGPAACloudChat::ShutdownRequests();
        Test->TestFalse(TEXT("Callback-only provider is released safely during cleanup"),State->CallbackOnlyOwner.IsValid());
        auto Rejected=MakeShared<bool>(false);
        auto Late=MakeShared<FGPAACloudChat,ESPMode::ThreadSafe>();
        Late->Start(State->Request,[](const FString&){},[Rejected](bool Ok,const FString& Error)
            {*Rejected=!Ok && Error.Contains(TEXT("shutting down"));});
        Test->TestTrue(TEXT("Late requests are rejected synchronously"),*Rejected);
        Test->TestTrue(TEXT("Cleanup never recreates HTTP"),HttpBefore==FModuleManager::GetModulePtr<FHttpModule>(TEXT("HTTP")));
        Late->Cancel(); State->Completed->Cancel();
        State->Pending.Reset(); State->Completed.Reset(); State->HeldReply.Reset();
        State->StoppedAt=FPlatformTime::Seconds(); State->Phase=2;
        return false;
    }
    if(FPlatformTime::Seconds()-State->StoppedAt<0.2) return false;
    Test->TestEqual(TEXT("No pending callbacks escape shutdown"),State->UnexpectedCallbacks,0);
    Test->TestEqual(TEXT("Only completed and pending fixtures reached the server"),State->Calls,2);
    return true;
}
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FGPAAHttpShutdownTest,"GPAA.Acceptance.HttpShutdown",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FGPAAHttpShutdownTest::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{
    // This intentionally stops the provider globally; run in its own process.
    if(FParse::Param(FCommandLine::Get(),TEXT("GPAAHttpShutdownAcceptance")))
    {Names.Add(TEXT("PendingAndLateRequests")); Commands.Add(TEXT("PendingAndLateRequests"));}
}
bool FGPAAHttpShutdownTest::RunTest(const FString& Parameters)
{
    auto* Sockets=ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    auto* Probe=Sockets->CreateSocket(NAME_Stream,TEXT("GPAA shutdown fixture"),false);
    auto Address=Sockets->CreateInternetAddr(); bool Valid=false;
    Address->SetIp(TEXT("127.0.0.1"),Valid); Address->SetPort(0);
    const int32 Port=Probe && Valid && Probe->Bind(*Address)?Probe->GetPortNo():0;
    if(Probe){Probe->Close();Sockets->DestroySocket(Probe);}
    if(!Port){AddError(TEXT("Could not allocate shutdown fixture port."));return false;}
    auto State=MakeShared<FGPAAHttpShutdownState>();
    State->Router=FHttpServerModule::Get().GetHttpRouter(Port,true);
    if(!State->Router){AddError(TEXT("Could not bind shutdown fixture."));return false;}
    State->Route=State->Router->BindRoute(FHttpPath(TEXT("/v1/responses")),EHttpServerRequestVerbs::VERB_POST,
        FHttpRequestHandler::CreateLambda([State](const FHttpServerRequest&,const FHttpResultCallback& Reply)
        {
            ++State->Calls;
            if(State->Calls==1)
                Reply(FHttpServerResponse::Create(TEXT("data: {\"type\":\"response.output_text.delta\",\"delta\":\"ok\"}\n\ndata: {\"type\":\"response.completed\"}\n\n"),TEXT("text/event-stream")));
            else {State->PendingSeen=true;State->HeldReply=MakeShared<FHttpResultCallback>(Reply);}
            return true;
        }));
    FHttpServerModule::Get().StartAllListeners();
    State->Request.Endpoint=FString::Printf(TEXT("http://127.0.0.1:%d/v1"),Port);
    State->Request.Key=TEXT("not-a-real-key"); State->Request.Model=TEXT("fake-model"); State->Request.Prompt=TEXT("shutdown fixture");
    State->Request.Timeout=20; State->Deadline=FPlatformTime::Seconds()+30;
    State->Completed=MakeShared<FGPAACloudChat,ESPMode::ThreadSafe>();
    State->Completed->Start(State->Request,[](const FString&){},[State](bool Ok,const FString& Error)
        {State->CompletedOk=Ok;State->CompletedDone=true;State->Error=Error;});
    ADD_LATENT_AUTOMATION_COMMAND(FGPAAWaitForHttpShutdown(State,this));
    return true;
}
#endif
