#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RivalFigure.generated.h"

class UAnimSequence;
class USkeletalMeshComponent;
struct FArenaEvent;

/**
 * T38: the first animated opponent (apps/vr/tasks/T38-first-animated-opponent.md; docs/research/T38-RIVAL-2026-10-25.md).
 * Brennic's rigged figure stands in for his greybox column and plays one of four clips, picked from the kernel's events
 * alone: Idle by default, Cast on his cast event, Hit on a hit that removed HP, Death on his down event (his 0 HP, the end
 * of the duel), held. The pick is a pure function over the event log (RivalClips::Step); the component only plays what it
 * returns. Nothing here writes into the kernel.
 * The figure is the scratch import of the creature cast (Greybox/CreatureImport.h): the DINOv3 hold keeps it out of the
 * repo, so a clean clone has no figure and the presentation draws the greybox body.
 */
enum class ERivalClip : uint8
{
	Idle,
	Cast,
	Hit,
	Death
};

namespace RivalClips
{
inline constexpr int32 Count = 4;
/** The rival the figure stands in for (FVrRival::Id) and the scratch asset name (FCreatureSpec::Name). */
inline const TCHAR* BrennicRivalId = TEXT("brennic");
inline const TCHAR* BrennicAsset = TEXT("Brennic");

/** The clip's take name in the FBX (rig_anim.py): Idle, Cast, Hit, Death. */
MAGEARENAVR_API const TCHAR* Name(ERivalClip Clip);

/** How long the one-shot clips play (s) and the kernel's step rate (Hz). The defaults are Brennic's import record. */
struct FLengths
{
	double CastS = 1.533;
	double HitS = 0.8;
	double StepHz = 60.0;
};

struct FState
{
	ERivalClip Clip = ERivalClip::Idle;
	/** The kernel tick the clip started on. */
	int32 StartTick = 0;
	/** Counts every start, so a Hit during a Hit plays again from its first frame. */
	int32 Starts = 0;
};

struct FChange
{
	int32 Tick = 0;
	ERivalClip Clip = ERivalClip::Idle;
};

/**
 * Advances State over Events (kernel order; the caller passes only the ones it has not passed before), then to NowTick.
 * Only RivalId's events count: "down" starts Death, and nothing replaces it; "hit" with a Value above 0 (the HP it
 * removed) starts Hit, again from its first frame when one is playing; "cast" starts Cast unless a Cast is playing, which
 * plays on. A Cast or a Hit falls back to Idle once its length has played. Any other actor's event, any other kind, and a
 * hit that removed no HP change nothing. Each change goes to OutChanges when given.
 */
MAGEARENAVR_API void Step(FState& State, TConstArrayView<FArenaEvent> Events, int32 RivalId, int32 NowTick, const FLengths& Lengths, TArray<FChange>* OutChanges = nullptr);
}

/**
 * The figure on its owner: one skeletal mesh component, made when the mesh and all four clips load, and none otherwise.
 * The owner places it at the rival's kernel position each frame and hands it the kernel's event log.
 */
UCLASS()
class MAGEARENAVR_API URivalFigureComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/**
	 * Loads <Folder>/SK_<Asset> and its four clips (A_<Asset>_<Clip>, or the FBX import's SK_<Asset>_Anim_<Clip>). A
	 * missing package is checked before any load, so a clean clone logs no load warning. False, with no mesh made, when
	 * any of the five is missing.
	 */
	bool Load(const FString& Folder, const FString& Asset);
	bool IsLoaded() const { return Mesh != nullptr; }

	/**
	 * Shows the figure for kernel actor ActorId with its feet at FeetCm, turned along FacingCm, and plays the clip that
	 * the events pick. Events is the whole log of the bout; the component keeps its own cursor, and starts over when the
	 * actor or the bout changes (a shorter log or an earlier tick).
	 */
	void Sync(TConstArrayView<FArenaEvent> Events, int32 ActorId, int32 NowTick, const FVector& FeetCm, const FVector& FacingCm, bool bPaused);
	void Hide();

	ERivalClip GetClip() const { return State.Clip; }
	int32 GetClipStarts() const { return State.Starts; }
	bool IsShown() const;
	USkeletalMeshComponent* GetMesh() const { return Mesh; }
	/** LOD0 triangles of the loaded mesh (0 when none is loaded). */
	int32 GetLod0Triangles() const;

private:
	void Play(int32 NowTick);

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> Mesh;

	/** By ERivalClip. */
	UPROPERTY()
	TArray<TObjectPtr<UAnimSequence>> Clips;

	RivalClips::FState State;
	RivalClips::FLengths Lengths;
	FVector MeshForward = FVector::ForwardVector;
	int32 EventCursor = 0;
	int32 FollowedId = INDEX_NONE;
	int32 LastTick = -1;
	int32 PlayedStarts = -1;
};
