#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "ImportSfxCommandlet.generated.h"

/**
 * T26. Imports the A05 starter pack (apps/vr/art/audio/sfx/<id>.wav, ids from manifest.json) as USoundWave assets
 * /Game/Audio/SFX/<id>, looping where the manifest says loop. Idempotent: a cue whose asset already records the same
 * source file hash (AssetImportData MD5) is left untouched, so a second run writes nothing.
 * Run through apps/vr/tools/import-sfx.ps1 (UnrealEditor-Cmd <uproject> -run=ImportSfx). Editor builds only.
 */
UCLASS()
class MAGEARENAVR_API UImportSfxCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UImportSfxCommandlet();
	virtual int32 Main(const FString& Params) override;
};
