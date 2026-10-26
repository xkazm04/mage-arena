#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "ImportCreatureMeshCommandlet.generated.h"

/**
 * Imports one rigged creature FBX (skeletal mesh, skeleton, its clips) and its baked base colour into
 * /Game/Characters/<Name> (Greybox/CreatureImport.h), where they are committed (Git LFS), and builds its material in code:
 * the plain base colour, or for the hound (FCreatureSpec::bEmber) the hue/saturation/value crack key that drives an ember
 * emissive and tints the body toward dark red.
 * Usage: -run=ImportCreatureMesh -name=<Brennic|CinderHound|MireMaw> -fbx=<path> -texture=<path> [-replace]. The legacy
 * FBX factory is used (Interchange is skipped by naming the factory), at uniform scale 100 with the default -Y front
 * conversion. A folder that already holds packages is refused unless -replace, which deletes that one creature's package
 * files first and imports the same names fresh (T70; an import over them kept their old settings, T68). Run through
 * apps/vr/tools/capture-creature-import.ps1 -Import <Name> [-Replace]. Editor builds only.
 */
UCLASS()
class MAGEARENAVR_API UImportCreatureMeshCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UImportCreatureMeshCommandlet();
	virtual int32 Main(const FString& Params) override;
};
