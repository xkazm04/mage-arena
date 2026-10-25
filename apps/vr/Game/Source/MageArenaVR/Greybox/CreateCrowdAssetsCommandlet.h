#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "CreateCrowdAssetsCommandlet.generated.h"

/**
 * T57: builds the two crowd assets under /Game/Environment/ArenaKit/Crowd (Greybox/ArenaStands.h, ArenaCrowd):
 * T_Crowd_Atlas (apps/vr/art/environment-kit/crowd/crowd_atlas.png, sRGB, mips) and M_Crowd_Atlas (lit, masked at 0.5,
 * one-sided, the atlas rect from per-instance custom data). The card itself is the engine's 1 m plane, so no mesh asset.
 * Idempotent: an asset that already loads is left alone; -force rebuilds both.
 * Run through apps/vr/tools/crowd/create-crowd-assets.ps1 (UnrealEditor-Cmd <uproject> -run=CreateCrowdAssets).
 * Editor builds only.
 */
UCLASS()
class MAGEARENAVR_API UCreateCrowdAssetsCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCreateCrowdAssetsCommandlet();
	virtual int32 Main(const FString& Params) override;
};
