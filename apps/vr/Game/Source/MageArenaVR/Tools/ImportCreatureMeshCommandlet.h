#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "ImportCreatureMeshCommandlet.generated.h"

/**
 * Imports one rigged creature FBX (skeletal mesh, skeleton, its clips) and its baked base colour into the scratch folder
 * /Game/CreatureImportScratch/<Name> (Greybox/CreatureImport.h), and builds its material in code: the plain base colour,
 * or for the hound (FCreatureSpec::bEmber) the hue/saturation/value crack key that drives an ember emissive and tints the
 * body toward dark red.
 * Usage: -run=ImportCreatureMesh -name=<Brennic|CinderHound> -fbx=<path> -texture=<path>. The legacy FBX factory is
 * used (Interchange is skipped by naming the factory), at uniform scale 100 with the default -Y front conversion.
 * Run through apps/vr/tools/capture-creature-import.ps1, which deletes the scratch folder before it ends: the DINOv3
 * licence hold stands, so none of these assets is ever committed. Editor builds only.
 */
UCLASS()
class MAGEARENAVR_API UImportCreatureMeshCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UImportCreatureMeshCommandlet();
	virtual int32 Main(const FString& Params) override;
};
