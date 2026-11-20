// Copyright Game Production AI Assistant contributors 2026.
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "UI/SGPAAssistantWidget.h"
#include "Settings/GPAAEditorSettings.h"
#include "ImageUtils.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "TextureResource.h"
#include "RenderingThread.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Layout/SBorder.h"

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FGPAAReleaseMediaTest, "GPAA.Acceptance.ReleaseMedia",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
void FGPAAReleaseMediaTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
    if (FParse::Param(FCommandLine::Get(), TEXT("GPAAReleaseMedia")))
    { Names.Add(TEXT("CurrentSlatePanel")); Commands.Add(TEXT("CurrentSlatePanel")); }
}
bool FGPAAReleaseMediaTest::RunTest(const FString& Parameters)
{
    FString Path;
    if (!FParse::Value(FCommandLine::Get(), TEXT("GPAAMediaPath="), Path))
    { AddError(TEXT("An explicit preview output path is required.")); return false; }
    auto* Settings = GetMutableDefault<UGPAAEditorSettings>();
    const FName PreviousStyle = FAppStyle::GetAppStyleSetName();
    FModuleManager::LoadModuleChecked<IModuleInterface>(TEXT("EditorStyle"));
    FAppStyle::SetAppStyleSetName(TEXT("EditorStyle"));
    const FString Provider = Settings->ChatProvider;
    Settings->ChatProvider = TEXT("Local");
    auto Panel = SNew(SGPAAssistantWidget);
    Panel->BeginPromptTurn(TEXT("Find the pistol reload animation and show its exact asset path."));
    Panel->SetChatResult(true, TEXT("UI demonstration"),
        TEXT("The pistol reload animation is:\n/Game/Characters/Mannequins/Anims/Pistol/MM_Pistol_Reload.MM_Pistol_Reload\n\nVerify the referenced asset in your project."), {});
    TArray<FColor> Pixels;
    const FIntVector Size(1100, 800, 0);
    FWidgetRenderer Renderer(true);
    auto Preview = SNew(SBorder).Padding(0).BorderImage(FAppStyle::Get().GetBrush(TEXT("WhiteBrush")))
        .BorderBackgroundColor(FStyleColors::Background)[Panel];
    auto* Target = Renderer.DrawWidget(Preview, FVector2D(Size.X, Size.Y));
    if (Target) Renderer.DrawWidget(Target, Preview, FVector2D(Size.X, Size.Y), 1.f/60);
    FlushRenderingCommands();
    bool Captured = Target && Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
    if (Captured)
    {
        int32 Different = 0;
        for (const FColor& Pixel : Pixels) if (Pixel != Pixels[0]) ++Different;
        Captured = Different > 1000;
    }
    if (Captured)
    {
        TArray64<uint8> Png;
        FImageUtils::PNGCompressImageArray(Size.X, Size.Y, MakeArrayView(Pixels), Png);
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
        TestTrue(TEXT("Current Slate preview saved"), FFileHelper::SaveArrayToFile(Png, *Path));
    }
    else AddError(TEXT("Offscreen Slate capture failed; no synthetic screenshot was substituted."));
    Settings->ChatProvider = Provider;
    FAppStyle::SetAppStyleSetName(PreviousStyle);
    return Captured;
}
#endif
