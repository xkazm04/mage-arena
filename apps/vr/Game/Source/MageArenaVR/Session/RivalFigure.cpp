#include "Session/RivalFigure.h"

#include "Algo/Count.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "Greybox/CreatureImport.h"
#include "Kernel/KernelData.h"
#include "Kernel/SimTypes.h"
#include "MageArenaVR.h"
#include "Misc/PackageName.h"
#include "Rendering/SkeletalMeshRenderData.h"

namespace RivalClips
{
namespace
{
int32 LengthTicks(ERivalClip Clip, const FLengths& Lengths)
{
	const double Seconds = Clip == ERivalClip::Cast ? Lengths.CastS : Clip == ERivalClip::Hit ? Lengths.HitS
	                                                                                          : 0.0;
	return FMath::Max(1, FMath::CeilToInt(Seconds * Lengths.StepHz - 1.0e-6));
}

bool IsOneShot(ERivalClip Clip)
{
	return Clip == ERivalClip::Cast || Clip == ERivalClip::Hit;
}

void Begin(FState& State, ERivalClip Clip, int32 Tick, TArray<FChange>* OutChanges)
{
	State.Clip = Clip;
	State.StartTick = Tick;
	++State.Starts;
	if (OutChanges)
	{
		OutChanges->Add({Tick, Clip});
	}
}

/** A Cast or a Hit that has played to its end by Tick hands back to Idle on the tick it ended. */
void Expire(FState& State, int32 Tick, const FLengths& Lengths, TArray<FChange>* OutChanges)
{
	if (!IsOneShot(State.Clip))
	{
		return;
	}
	const int32 End = State.StartTick + LengthTicks(State.Clip, Lengths);
	if (Tick >= End)
	{
		Begin(State, ERivalClip::Idle, End, OutChanges);
	}
}
}

const TCHAR* Name(ERivalClip Clip)
{
	switch (Clip)
	{
	case ERivalClip::Cast:
		return TEXT("Cast");
	case ERivalClip::Hit:
		return TEXT("Hit");
	case ERivalClip::Death:
		return TEXT("Death");
	default:
		return TEXT("Idle");
	}
}

void Step(FState& State, TConstArrayView<FArenaEvent> Events, int32 RivalId, int32 NowTick, const FLengths& Lengths, TArray<FChange>* OutChanges)
{
	for (const FArenaEvent& Event : Events)
	{
		if (State.Clip == ERivalClip::Death)
		{
			return;
		}
		if (Event.ActorId != RivalId)
		{
			continue;
		}
		Expire(State, Event.Tick, Lengths, OutChanges);
		if (Event.Kind == TEXT("down"))
		{
			Begin(State, ERivalClip::Death, Event.Tick, OutChanges);
		}
		else if (Event.Kind == TEXT("hit") && Event.Value > 0.0)
		{
			Begin(State, ERivalClip::Hit, Event.Tick, OutChanges);
		}
		else if (Event.Kind == TEXT("cast") && State.Clip != ERivalClip::Cast)
		{
			// Brennic casts about twice a second (the seed-1 duel: 18 to 30 ticks apart); a Cast that restarted on each
			// one would never get past its first half second, so a cast during a Cast lets it play on.
			Begin(State, ERivalClip::Cast, Event.Tick, OutChanges);
		}
	}
	Expire(State, NowTick, Lengths, OutChanges);
}
}

bool URivalFigureComponent::Load(const FString& Folder, const FString& Asset)
{
	auto Find = [&Folder](const FString& AssetName) -> FString
	{
		const FString Package = Folder / AssetName;
		return FPackageName::DoesPackageExist(Package) ? FString::Printf(TEXT("%s.%s"), *Package, *AssetName) : FString();
	};
	const FString MeshPath = Find(FString::Printf(TEXT("SK_%s"), *Asset));
	TArray<FString> ClipPaths;
	for (int32 Index = 0; Index < RivalClips::Count; ++Index)
	{
		const TCHAR* Clip = RivalClips::Name(static_cast<ERivalClip>(Index));
		FString Path = Find(FString::Printf(TEXT("A_%s_%s"), *Asset, Clip));
		if (Path.IsEmpty())
		{
			Path = Find(FString::Printf(TEXT("SK_%s_Anim_%s"), *Asset, Clip));
		}
		ClipPaths.Add(Path);
	}
	const int32 ClipsFound = RivalClips::Count - static_cast<int32>(Algo::Count(ClipPaths, FString()));
	if (MeshPath.IsEmpty() || ClipsFound < RivalClips::Count)
	{
		UE_LOG(LogMageArena, Log, TEXT("MAGEVR_RIVAL_FIGURE none folder=%s asset=%s mesh=%d clips=%d/%d (the greybox body stands in)"), *Folder, *Asset,
		    MeshPath.IsEmpty() ? 0 : 1, ClipsFound, RivalClips::Count);
		return false;
	}
	USkeletalMesh* SkeletalMesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
	TArray<UAnimSequence*> Loaded;
	for (const FString& Path : ClipPaths)
	{
		Loaded.Add(LoadObject<UAnimSequence>(nullptr, *Path));
	}
	AActor* Owner = GetOwner();
	if (!SkeletalMesh || Loaded.Contains(nullptr) || !Owner)
	{
		UE_LOG(LogMageArena, Warning, TEXT("MAGEVR_RIVAL_FIGURE none folder=%s asset=%s: a package exists but did not load"), *Folder, *Asset);
		return false;
	}
	// The creature cast's draw-call policy (one section, generated LODs, no per-bone velocity), as SpawnCreature applies it.
	const CreatureImport::FCreatureBudget Budget = CreatureImport::BudgetFromCommandLine();
	if (const CreatureImport::FCreatureSpec* Spec = CreatureImport::FindSpec(Asset))
	{
		CreatureImport::PrepareMesh(*SkeletalMesh, *Spec, Budget);
	}
	Mesh = NewObject<USkeletalMeshComponent>(Owner, TEXT("RivalFigureMesh"));
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetupAttachment(Owner->GetRootComponent());
	Mesh->SetSkeletalMesh(SkeletalMesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->RegisterComponent();
	CreatureImport::ApplyComponentBudget(*Mesh, CreatureImport::OpponentDistanceM(), Budget);
	Mesh->SetVisibility(false);
	Clips.Reset();
	Clips.Append(Loaded);
	MeshForward = CreatureImport::MeshForward(*SkeletalMesh);
	if (MeshForward.IsNearlyZero())
	{
		MeshForward = FVector::ForwardVector;
	}
	Lengths.CastS = Clips[static_cast<int32>(ERivalClip::Cast)]->GetPlayLength();
	Lengths.HitS = Clips[static_cast<int32>(ERivalClip::Hit)]->GetPlayLength();
	Lengths.StepHz = KernelData().SimStepHz;
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_RIVAL_FIGURE loaded %s lod0tris=%d lods=%d forward=(%.2f, %.2f) idle=%.3f cast=%.3f hit=%.3f death=%.3f s"), *MeshPath,
	    GetLod0Triangles(), SkeletalMesh->GetLODNum(), MeshForward.X, MeshForward.Y, Clips[0]->GetPlayLength(), Lengths.CastS, Lengths.HitS, Clips[3]->GetPlayLength());
	return true;
}

int32 URivalFigureComponent::GetLod0Triangles() const
{
	const USkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	const FSkeletalMeshRenderData* RenderData = Asset ? Asset->GetResourceForRendering() : nullptr;
	return RenderData && RenderData->LODRenderData.Num() > 0 ? RenderData->LODRenderData[0].GetTotalFaces() : 0;
}

bool URivalFigureComponent::IsShown() const
{
	return Mesh && Mesh->IsVisible();
}

void URivalFigureComponent::Hide()
{
	if (Mesh)
	{
		Mesh->SetVisibility(false);
	}
}

void URivalFigureComponent::Sync(TConstArrayView<FArenaEvent> Events, int32 ActorId, int32 NowTick, const FVector& FeetCm, const FVector& FacingCm, bool bPaused)
{
	if (!Mesh)
	{
		return;
	}
	if (ActorId != FollowedId || Events.Num() < EventCursor || NowTick < LastTick)
	{
		// Another actor or a new bout: the figure starts over in Idle.
		FollowedId = ActorId;
		EventCursor = 0;
		State = RivalClips::FState();
		State.StartTick = NowTick;
		PlayedStarts = -1;
	}
	LastTick = NowTick;
	const ERivalClip Before = State.Clip;
	RivalClips::Step(State, Events.Slice(EventCursor, Events.Num() - EventCursor), ActorId, NowTick, Lengths);
	EventCursor = Events.Num();
	if (State.Clip != Before)
	{
		UE_LOG(LogMageArena, Log, TEXT("MAGEVR_RIVAL_CLIP %s tick=%d start=%d actor=%d starts=%d"), RivalClips::Name(State.Clip), NowTick, State.StartTick, ActorId, State.Starts);
	}

	FVector Facing = FacingCm;
	Facing.Z = 0.0;
	const double Yaw = Facing.IsNearlyZero() ? Mesh->GetComponentRotation().Yaw
	                                         : FMath::RadiansToDegrees(FMath::Atan2(Facing.Y, Facing.X) - FMath::Atan2(MeshForward.Y, MeshForward.X));
	Mesh->SetWorldLocationAndRotation(FeetCm, FRotator(0.0, Yaw, 0.0));
	Mesh->SetVisibility(true);
	Mesh->GlobalAnimRateScale = bPaused ? 0.f : 1.f;
	if (State.Starts != PlayedStarts)
	{
		Play(NowTick);
	}
}

void URivalFigureComponent::Play(int32 NowTick)
{
	PlayedStarts = State.Starts;
	UAnimSequence* Clip = Clips.IsValidIndex(static_cast<int32>(State.Clip)) ? Clips[static_cast<int32>(State.Clip)].Get() : nullptr;
	if (!Clip)
	{
		return;
	}
	// Idle loops; Cast and Hit play once (the pick hands back to Idle); Death plays once and its last pose holds.
	Mesh->PlayAnimation(Clip, State.Clip == ERivalClip::Idle);
	// The clip started on the kernel's tick; a frame that lands a step late starts it that far in.
	const double Into = FMath::Max(0.0, (NowTick - State.StartTick) / FMath::Max(1.0, Lengths.StepHz));
	if (Into > 0.0)
	{
		Mesh->SetPosition(static_cast<float>(FMath::Min<double>(Into, Clip->GetPlayLength())), false);
	}
}
