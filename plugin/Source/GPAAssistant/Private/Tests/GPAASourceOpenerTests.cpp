// Copyright Game Production AI Assistant contributors 2026.
#include "Misc/AutomationTest.h"
#include "Services/GPAASourceOpener.h"
#include "Editor.h"
#include "Curves/CurveFloat.h"
#include "Subsystems/AssetEditorSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGPAAAssetReferenceTest, "GPAA.Plugin.SourceOpener.AssetReferences",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGPAAAssetReferenceTest::RunTest(const FString& Parameters)
{
    FGPAASourceReference Source;
    Source.SourceType = TEXT("Asset");
    const FString Expected = TEXT("/Game/Characters/Anims/MM_Pistol_Reload.MM_Pistol_Reload");
    Source.RelativePath = TEXT("asset:") + Expected;
    FString Path;
    TestTrue(TEXT("Native asset reference resolves"), FGPAASourceOpener::ResolveAssetObjectPath(Source, Path));
    TestEqual(TEXT("Object path retained exactly"), Path, Expected);
    Source.RelativePath = TEXT(".gpaa/unreal-assets/animation.json");
    Source.Content = TEXT("{\"objectPath\":\"") + Expected + TEXT("\"}");
    TestTrue(TEXT("Legacy metadata object path resolves"), FGPAASourceOpener::ResolveAssetObjectPath(Source, Path));
    TestEqual(TEXT("Legacy path matches native path"), Path, Expected);
    Source.Content = TEXT("{}");
    FString Status;
    TestFalse(TEXT("Unresolvable asset is reported as failure"), FGPAASourceOpener::OpenSource(Source, TEXT(""), TEXT("Auto"), TEXT(""), Status));
    TestTrue(TEXT("Failure explains invalid reference"), Status.Contains(TEXT("no valid Unreal object path")));
    Source.RelativePath = TEXT("asset:C:/Windows/not-an-asset.txt");
    TestFalse(TEXT("Filesystem paths cannot become asset objects"), FGPAASourceOpener::ResolveAssetObjectPath(Source, Path));
    TestTrue(TEXT("Failed resolution clears output"), Path.IsEmpty());
    // Exercise the real editor dispatcher with a disposable in-memory asset.
    UCurveFloat* Asset = NewObject<UCurveFloat>(GetTransientPackage(), NAME_None, RF_Transient);
    Source.RelativePath = TEXT("asset:") + Asset->GetPathName();
    TestTrue(TEXT("Open activates an Unreal asset editor"), FGPAASourceOpener::OpenSource(Source, TEXT(""), TEXT("Auto"), TEXT(""), Status));
    auto* Editors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
    TestTrue(TEXT("Asset has an associated editor"), Editors->FindEditorForAsset(Asset, false) != nullptr);
    Editors->CloseAllEditorsForAsset(Asset);
    return true;
}
#endif
