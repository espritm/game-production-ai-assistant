#include "Services/GPAAAssetMetadataRequestBuilder.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
PRAGMA_ENABLE_DEPRECATION_WARNINGS

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FString FGPAAAssetMetadataRequestBuilder::BuildSyncRequest(const FString& ProjectRoot)
{
    constexpr int32 MaxAssetsToSync = 1500;
    constexpr int32 MaxTagsPerAsset = 8;
    constexpr int32 MaxRelationsPerAsset = 12;
    constexpr int32 MaxPayloadCharacters = 4 * 1024 * 1024;

    TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
    RootObject->SetStringField(TEXT("projectRoot"), ProjectRoot);

    TArray<TSharedPtr<FJsonValue>> AssetValues;
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    TArray<FAssetData> AssetDataList;
    AssetRegistryModule.Get().GetAssetsByPath(FName(TEXT("/Game")), AssetDataList, true);

    auto BuildNameArray = [MaxRelationsPerAsset](const TArray<FName>& Names)
    {
        TArray<TSharedPtr<FJsonValue>> Values;
        int32 NameCount = 0;
        for (const FName& Name : Names)
        {
            if (NameCount >= MaxRelationsPerAsset)
            {
                break;
            }

            const FString Value = Name.ToString();
            if (Value.StartsWith(TEXT("/Game")) && !Value.IsEmpty())
            {
                Values.Add(MakeShared<FJsonValueString>(Value));
                NameCount++;
            }
        }

        return Values;
    };

    auto ShouldSyncTag = [](const FString& TagName)
    {
        return TagName.Equals(TEXT("ParentClass"), ESearchCase::IgnoreCase)
            || TagName.Equals(TEXT("GeneratedClass"), ESearchCase::IgnoreCase)
            || TagName.Equals(TEXT("BlueprintType"), ESearchCase::IgnoreCase)
            || TagName.Equals(TEXT("NativeParentClass"), ESearchCase::IgnoreCase)
            || TagName.Contains(TEXT("Class"), ESearchCase::IgnoreCase);
    };

    int32 SyncedAssetCount = 0;
    for (const FAssetData& AssetData : AssetDataList)
    {
        if (SyncedAssetCount >= MaxAssetsToSync)
        {
            break;
        }

        TSharedRef<FJsonObject> AssetObject = MakeShared<FJsonObject>();
        AssetObject->SetStringField(TEXT("objectPath"), AssetData.GetSoftObjectPath().ToString());
        AssetObject->SetStringField(TEXT("packageName"), AssetData.PackageName.ToString());
        AssetObject->SetStringField(TEXT("packagePath"), AssetData.PackagePath.ToString());
        AssetObject->SetStringField(TEXT("assetName"), AssetData.AssetName.ToString());
        AssetObject->SetStringField(TEXT("assetClass"), AssetData.AssetClassPath.ToString());

        TSharedRef<FJsonObject> TagsObject = MakeShared<FJsonObject>();
        int32 TagCount = 0;
        for (const TPair<FName, FAssetTagValueRef>& AssetTag : AssetData.TagsAndValues)
        {
            if (TagCount >= MaxTagsPerAsset)
            {
                break;
            }

            const FString Value = AssetTag.Value.GetValue();
            const FString TagName = AssetTag.Key.ToString();
            if (ShouldSyncTag(TagName) && !Value.IsEmpty() && Value.Len() <= 300)
            {
                TagsObject->SetStringField(TagName, Value);
                TagCount++;
            }
        }

        AssetObject->SetObjectField(TEXT("tags"), TagsObject);

        TArray<FName> Dependencies;
        AssetRegistryModule.Get().GetDependencies(
            AssetData.PackageName,
            Dependencies,
            UE::AssetRegistry::EDependencyCategory::Package);

        TArray<FName> Referencers;
        AssetRegistryModule.Get().GetReferencers(
            AssetData.PackageName,
            Referencers,
            UE::AssetRegistry::EDependencyCategory::Package);

        AssetObject->SetArrayField(TEXT("dependencies"), BuildNameArray(Dependencies));
        AssetObject->SetArrayField(TEXT("referencers"), BuildNameArray(Referencers));
        AssetValues.Add(MakeShared<FJsonValueObject>(AssetObject));
        SyncedAssetCount++;
    }

    RootObject->SetArrayField(TEXT("assets"), AssetValues);

    FString Output;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(RootObject, Writer);
    if (Output.Len() > MaxPayloadCharacters)
    {
        RootObject->SetArrayField(TEXT("assets"), TArray<TSharedPtr<FJsonValue>>());
        Output.Reset();
        const TSharedRef<TJsonWriter<>> EmptyWriter = TJsonWriterFactory<>::Create(&Output);
        FJsonSerializer::Serialize(RootObject, EmptyWriter);
    }

    return Output;
}
