#include "Tools/ImportSfxCommandlet.h"

#include "MageArenaVR.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_EDITOR
#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "EditorFramework/AssetImportData.h"
#include "IAssetTools.h"
#include "Misc/SecureHash.h"
#include "Sound/SoundWave.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

UImportSfxCommandlet::UImportSfxCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UImportSfxCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	FString SourceDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("../art/audio/sfx"));
	FParse::Value(*Params, TEXT("source="), SourceDir);
	FPaths::CollapseRelativeDirectories(SourceDir);
	SourceDir = FPaths::ConvertRelativePathToFull(SourceDir);
	const FString DestinationPath = TEXT("/Game/Audio/SFX");

	FString ManifestText;
	TSharedPtr<FJsonObject> Manifest;
	const FString ManifestPath = FPaths::Combine(SourceDir, TEXT("manifest.json"));
	if (!FFileHelper::LoadFileToString(ManifestText, *ManifestPath) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ManifestText), Manifest)
		|| !Manifest.IsValid())
	{
		UE_LOG(LogMageArena, Error, TEXT("MAGEVR_IMPORT_SFX fail manifest %s"), *ManifestPath);
		return 1;
	}
	const TArray<TSharedPtr<FJsonValue>>* Cues = nullptr;
	if (!Manifest->TryGetArrayField(TEXT("cues"), Cues) || !Cues || Cues->Num() == 0)
	{
		UE_LOG(LogMageArena, Error, TEXT("MAGEVR_IMPORT_SFX fail manifest has no cues"));
		return 1;
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	int32 Imported = 0;
	int32 Unchanged = 0;
	int32 Failed = 0;
	for (const TSharedPtr<FJsonValue>& Value : *Cues)
	{
		const TSharedPtr<FJsonObject> Cue = Value.IsValid() ? Value->AsObject() : nullptr;
		FString Id;
		if (!Cue.IsValid() || !Cue->TryGetStringField(TEXT("id"), Id))
		{
			++Failed;
			continue;
		}
		bool bLoop = false;
		Cue->TryGetBoolField(TEXT("loop"), bLoop);
		const FString Wav = FPaths::Combine(SourceDir, Id + TEXT(".wav"));
		TArray<uint8> Head;
		if (!FFileHelper::LoadFileToArray(Head, *Wav) || Head.Num() < 44 || FMemory::Memcmp(Head.GetData(), "RIFF", 4) != 0)
		{
			// A Git LFS pointer is text, not RIFF: run git lfs pull first.
			UE_LOG(LogMageArena, Error, TEXT("MAGEVR_IMPORT_SFX fail %s is not a RIFF WAV (LFS pointer?)"), *Wav);
			++Failed;
			continue;
		}
		const FMD5Hash Hash = FMD5Hash::HashFile(*Wav);
		const FString ObjectPath = FString::Printf(TEXT("%s/%s.%s"), *DestinationPath, *Id, *Id);
		if (USoundWave* Existing = LoadObject<USoundWave>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			const UAssetImportData* Data = Existing->AssetImportData;
			const bool bSameSource = Data && Data->GetSourceFileCount() == 1 && Data->GetSourceData().SourceFiles[0].FileHash == Hash;
			if (bSameSource && Existing->bLooping == bLoop)
			{
				++Unchanged;
				UE_LOG(LogMageArena, Display, TEXT("MAGEVR_IMPORT_SFX unchanged %s md5=%s"), *Id, *LexToString(Hash));
				continue;
			}
		}

		UAssetImportTask* Task = NewObject<UAssetImportTask>();
		Task->Filename = Wav;
		Task->DestinationPath = DestinationPath;
		Task->DestinationName = Id;
		Task->bReplaceExisting = true;
		Task->bReplaceExistingSettings = true;
		Task->bAutomated = true;
		Task->bSave = false;
		AssetTools.ImportAssetTasks({Task});
		const TArray<UObject*> Objects = Task->GetObjects();
		USoundWave* Wave = Objects.Num() > 0 ? Cast<USoundWave>(Objects[0]) : nullptr;
		if (!Wave)
		{
			UE_LOG(LogMageArena, Error, TEXT("MAGEVR_IMPORT_SFX fail import %s"), *Wav);
			++Failed;
			continue;
		}
		Wave->bLooping = bLoop;
		Wave->MarkPackageDirty();
		UPackage* Package = Wave->GetOutermost();
		const FString PackageFile = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		if (!UPackage::SavePackage(Package, Wave, *PackageFile, SaveArgs))
		{
			UE_LOG(LogMageArena, Error, TEXT("MAGEVR_IMPORT_SFX fail save %s"), *PackageFile);
			++Failed;
			continue;
		}
		++Imported;
		UE_LOG(LogMageArena, Display, TEXT("MAGEVR_IMPORT_SFX imported %s loop=%d seconds=%.3f channels=%d rate=%d md5=%s -> %s"), *Id, bLoop ? 1 : 0,
			Wave->GetDuration(), Wave->NumChannels, static_cast<int32>(Wave->GetSampleRateForCurrentPlatform()), *LexToString(Hash), *PackageFile);
	}
	UE_LOG(LogMageArena, Display, TEXT("MAGEVR_IMPORT_SFX done imported=%d unchanged=%d failed=%d"), Imported, Unchanged, Failed);
	return Failed == 0 ? 0 : 1;
#else
	UE_LOG(LogMageArena, Error, TEXT("MAGEVR_IMPORT_SFX needs an editor build"));
	return 1;
#endif
}
