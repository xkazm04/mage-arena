#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Kernel/ArenaKernel.h"
#include "Kernel/Fire.h"
#include "Kernel/Games.h"
#include "Kernel/KernelData.h"
#include "Kernel/SimConstants.h"
#include "Kernel/VrRules.h"
#include "MageArenaVR.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include <cmath>

// T19 rival phases (DECISIONS 2026-10-07, DF-003 answered). Literals and where they come from:
// - stats.csv rank 1: max HP 80 + 15 = 95 (the same sum as MageArena.Session.ChainAdvance).
// - combat.vr.json rivals.brennic.phases: 0.66 and 0.33. 95 x 0.66 = 62.7 and 95 x 0.33 = 31.35.
// - spells-fire.csv fire_sunfall: tier 4, mana 70, cast_s 0.80, telegraph_s 1.20, unblockable.
//   combat.json simStepHz 60, so the cast is 48 ticks and the shadow 72 ticks.
// - kernel TierCap 4 (kernel.ts:216).

namespace
{
// Pre-change digest of DuelDigest, recorded from the build before any T19 kernel code (runs/T19/logs/probe-nullpath.log):
// "Rivals null-path digest=2b6273dbaeae8cfaec6d7cb44ebe65b7 seed=1 phase=lost tick=1602 events=158; seed=2 phase=lost
// tick=1376 events=133; seed=3 phase=lost tick=443 events=51;"
const TCHAR* const PreChangeDigest = TEXT("2b6273dbaeae8cfaec6d7cb44ebe65b7");

const FComposition* RivalReferencePreset()
{
	for (const FComposition& Candidate : KernelData().Presets)
	{
		if (Candidate.Name == KernelData().ReferencePreset)
		{
			return &Candidate;
		}
	}
	return nullptr;
}

// The Tiro semifinal (wave index 2) with Fire mages, the reference water player, and the VR overlay.
// Digest = MD5 of every event line plus the final state hash. Seeds 1-3.
// T22 adds the collar events (miss, dodge, reflectHit) on the ruleset path. They only report what happened, so the digest
// is taken with them filtered out of the log (and out of the hashed state): equal to the pinned digest means the duel
// itself, tick for tick, is unchanged. The summary counts them.
FString DuelDigest(const FVrRuleset& Rules, FString& Summary, int32* OutPhaseEvents = nullptr)
{
	FString Lines;
	Summary.Reset();
	int32 PhaseEvents = 0;
	for (uint32 Seed = 1; Seed <= 3; ++Seed)
	{
		FGames Games;
		if (!TryCreateGames(Games, Seed, RivalReferencePreset(), 2, true, true, &Rules))
		{
			return TEXT("create-failed");
		}
		const int32 Limit = SimTicks(KernelData().FightTimeoutS);
		while (Games.Phase == TEXT("active") && Games.State.Tick < Limit)
		{
			StepGames(Games);
		}
		FArenaState Filtered = Games.State;
		const int32 CollarEvents = Filtered.Events.RemoveAll([](const FArenaEvent& Event)
		{
			return Event.Kind == TEXT("miss") || Event.Kind == TEXT("dodge") || Event.Kind == TEXT("reflectHit");
		});
		for (const FArenaEvent& Event : Filtered.Events)
		{
			Lines += FString::Printf(TEXT("%d %s %d %.17g %d\n"), Event.Tick, *Event.Kind, Event.ActorId, Event.Value, Event.TargetId.Get(-1));
			PhaseEvents += Event.Kind == TEXT("phase") ? 1 : 0;
		}
		Lines += FString::Printf(TEXT("seed %u phase %s tick %d hash %s\n"), Seed, *Games.Phase, Filtered.Tick, *StateHash(Filtered));
		Summary += FString::Printf(TEXT("seed=%u phase=%s tick=%d events=%d collar=%d; "), Seed, *Games.Phase, Games.State.Tick,
			Filtered.Events.Num(), CollarEvents);
	}
	if (OutPhaseEvents)
	{
		*OutPhaseEvents = PhaseEvents;
	}
	return FMD5::HashAnsiString(*Lines);
}

FString VrDataFile(const TCHAR* Name)
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../data/vr"), Name));
}

// A copy of apps/vr/data/vr with combat.vr.json rewritten by Mutate. Empty on failure.
FString WriteVariant(FAutomationTestBase& Test, const TFunctionRef<void(FJsonObject&)>& Mutate)
{
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Text, *VrDataFile(TEXT("combat.vr.json")))
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		Test.AddError(TEXT("combat.vr.json could not be read"));
		return FString();
	}
	Mutate(*Root);
	const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MageArenaTests"), TEXT("rivals"), FGuid::NewGuid().ToString());
	IFileManager::Get().MakeDirectory(*Dir, true);
	FString Out;
	FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&Out));
	bool bWritten = FFileHelper::SaveStringToFile(Out, *FPaths::Combine(Dir, TEXT("combat.vr.json")));
	for (const TCHAR* Name : {TEXT("arena-layout.json"), TEXT("calibration-proposal.json")})
	{
		bWritten &= IFileManager::Get().Copy(*FPaths::Combine(Dir, Name), *VrDataFile(Name)) == COPY_OK;
	}
	if (!bWritten)
	{
		Test.AddError(TEXT("variant overlay could not be written"));
		return FString();
	}
	return Dir;
}

bool LoadFrom(const FString& Dir, FVrRuleset& Out, FString& Error)
{
	SetVrDataDirForTest(Dir);
	const bool bLoaded = LoadVrRuleset(Out, Error, false);
	SetVrDataDirForTest(FString());
	return bLoaded;
}

TSharedPtr<FJsonObject> Brennic(FJsonObject& Root)
{
	const TSharedPtr<FJsonObject>* Rivals = nullptr;
	const TSharedPtr<FJsonObject>* Entry = nullptr;
	if (Root.TryGetObjectField(TEXT("rivals"), Rivals) && Rivals && (*Rivals)->TryGetObjectField(TEXT("brennic"), Entry) && Entry)
	{
		return *Entry;
	}
	return nullptr;
}

TSharedPtr<FJsonObject> BrennicPhase(FJsonObject& Root, int32 Index)
{
	const TSharedPtr<FJsonObject> Entry = Brennic(Root);
	const TArray<TSharedPtr<FJsonValue>>* Phases = nullptr;
	if (!Entry.IsValid() || !Entry->TryGetArrayField(TEXT("phases"), Phases) || !Phases || !Phases->IsValidIndex(Index))
	{
		return nullptr;
	}
	return (*Phases)[Index]->AsObject();
}

struct FRivalRig
{
	FArenaState State;
	FVrRuleset Rules;
	int32 PlayerId = 0;
	int32 RivalId = 0;
};

// A player and a Fire rival with no brains. The rival is bound by hand, as SpawnWave would bind Brennic.
bool OpenRig(FAutomationTestBase& Test, FRivalRig& Rig)
{
	if (!KernelData().bReady)
	{
		Test.AddError(TEXT("kernel data is not ready"));
		return false;
	}
	FString Error;
	if (!LoadVrRuleset(Rig.Rules, Error, false))
	{
		Test.AddError(Error);
		return false;
	}
	const int32 Index = Rig.Rules.FindRival(TEXT("tiro"), 3, TEXT("semifinal"), TEXT("fire"));
	if (!Test.TestTrue(TEXT("Brennic is in the overlay"), Index != INDEX_NONE))
	{
		return false;
	}
	Rig.State = CreateArena(19);
	Rig.PlayerId = AddMage(Rig.State, 0, KernelData().PlayerSpawn, TEXT("Cassia")).Id;
	FActor& Rival = AddMage(Rig.State, 1, KernelData().ArenaCentre, TEXT("Brennic"));
	Rival.Fire.bSchool = true;
	Rig.RivalId = Rival.Id;
	Rig.Rules.RivalRuntime.ActorId = Rig.RivalId;
	Rig.Rules.RivalRuntime.RivalIndex = Index;
	return Test.TestEqual(TEXT("max HP 95 (stats.csv rank 1: 80 + 15)"), Rival.MaxHp, 95.0);
}

FActor& RivalOf(FRivalRig& Rig)
{
	return *SimFindActor(Rig.State, Rig.RivalId);
}

FActor& PlayerOf(FRivalRig& Rig)
{
	return *SimFindActor(Rig.State, Rig.PlayerId);
}

// One step in which a magic area hit of Damage lands on the rival. Returns the tick it landed on.
int32 HitRival(FRivalRig& Rig, double Damage)
{
	const FActor& Rival = RivalOf(Rig);
	FTelegraph Telegraph;
	Telegraph.Id = Rig.State.NextId++;
	Telegraph.ActivationId = Rig.State.NextId++;
	Telegraph.OwnerId = Rig.PlayerId;
	Telegraph.Family = TEXT("magic");
	Telegraph.Tier = 0;
	Telegraph.Damage = Damage;
	Telegraph.Kind = TEXT("area");
	Telegraph.Origin = Rival.Pos;
	Telegraph.Target = Rival.Pos;
	Telegraph.Source = Rival.Pos;
	Telegraph.WidthM = 0.5;
	Telegraph.StartTick = Rig.State.Tick;
	Telegraph.ResolveTick = Rig.State.Tick + 1;
	Telegraph.bSurvivesOwner = true;
	Telegraph.bCommitted = true;
	Rig.State.Telegraphs.Add(Telegraph);
	StepArena(Rig.State, TMap<int32, FInputFrame>(), &Rig.Rules);
	return Rig.State.Tick;
}

TArray<FArenaEvent> EventsAt(const FArenaState& State, int32 Tick, const TCHAR* Kind)
{
	TArray<FArenaEvent> Out;
	for (const FArenaEvent& Event : State.Events)
	{
		if (Event.Tick == Tick && Event.Kind == Kind)
		{
			Out.Add(Event);
		}
	}
	return Out;
}

bool PendingSunfall(const FActor& Actor)
{
	return Actor.Pending.IsSet() && Actor.Pending->SpellId.IsSet() && Actor.Pending->SpellId.GetValue() == TEXT("fire_sunfall");
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaRivalsLoads, "MageArena.Rivals.Loads",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaRivalsLoads::RunTest(const FString& Parameters)
{
	FVrRuleset Rules;
	FString Error;
	if (!TestTrue(TEXT("overlay loads"), LoadVrRuleset(Rules, Error, false)))
	{
		AddError(Error);
		return false;
	}
	bool bPass = TestEqual(TEXT("one rival"), Rules.Rivals.Num(), 1);
	if (!bPass)
	{
		return false;
	}
	const FVrRival& Rival = Rules.Rivals[0];
	bPass &= TestEqual(TEXT("id"), Rival.Id, FString(TEXT("brennic")));
	bPass &= TestEqual(TEXT("school"), Rival.School, FString(TEXT("fire")));
	// arena-tiers.json Tiro wave n 3 is the semifinal (one mage, competence 1).
	bPass &= TestEqual(TEXT("tier"), Rival.TierId, FString(TEXT("tiro")));
	bPass &= TestEqual(TEXT("wave n"), Rival.WaveN, 3);
	bPass &= TestEqual(TEXT("kind"), Rival.WaveKind, FString(TEXT("semifinal")));
	bPass &= TestEqual(TEXT("surge"), Rival.SurgeTiers, 1);
	bPass &= TestEqual(TEXT("two breaks"), Rival.Phases.Num(), 2);
	if (Rival.Phases.Num() == 2)
	{
		bPass &= TestEqual(TEXT("66 %"), Rival.Phases[0].AtHpFraction, 0.66);
		bPass &= TestTrue(TEXT("first break opens with nothing"), Rival.Phases[0].OpensWith.IsEmpty());
		bPass &= TestEqual(TEXT("33 %"), Rival.Phases[1].AtHpFraction, 0.33);
		bPass &= TestEqual(TEXT("last phase opens with Sunfall"), Rival.Phases[1].OpensWith, FString(TEXT("fire_sunfall")));
	}
	bPass &= TestEqual(TEXT("not the final"), Rules.FindRival(TEXT("tiro"), 4, TEXT("final"), TEXT("fire")), static_cast<int32>(INDEX_NONE));
	bPass &= TestEqual(TEXT("not a water proxy"), Rules.FindRival(TEXT("tiro"), 3, TEXT("semifinal"), TEXT("water")), static_cast<int32>(INDEX_NONE));

	struct FCase
	{
		const TCHAR* Name;
		bool bLoads;
		TFunction<void(FJsonObject&)> Mutate;
	};
	TArray<FCase> Cases;
	Cases.Add({TEXT("absent block"), true, [](FJsonObject& Root) { Root.RemoveField(TEXT("rivals")); }});
	Cases.Add({TEXT("rising threshold"), false, [](FJsonObject& Root) { if (auto P = BrennicPhase(Root, 1)) { P->SetNumberField(TEXT("atHpFraction"), 0.7); } }});
	Cases.Add({TEXT("threshold of 1"), false, [](FJsonObject& Root) { if (auto P = BrennicPhase(Root, 0)) { P->SetNumberField(TEXT("atHpFraction"), 1.0); } }});
	Cases.Add({TEXT("unknown signature"), false, [](FJsonObject& Root) { if (auto P = BrennicPhase(Root, 1)) { P->SetStringField(TEXT("opensWith"), TEXT("fire_nope")); } }});
	Cases.Add({TEXT("two signatures"), false, [](FJsonObject& Root) { if (auto P = BrennicPhase(Root, 0)) { P->SetStringField(TEXT("opensWith"), TEXT("fire_bolt")); } }});
	Cases.Add({TEXT("unknown key"), false, [](FJsonObject& Root) { if (auto E = Brennic(Root)) { E->SetNumberField(TEXT("hpScale"), 2.0); } }});
	Cases.Add({TEXT("kind is not the wave's"), false, [](FJsonObject& Root)
	{
		if (auto E = Brennic(Root))
		{
			const TSharedPtr<FJsonObject>* Applies = nullptr;
			if (E->TryGetObjectField(TEXT("appliesTo"), Applies) && Applies)
			{
				(*Applies)->SetStringField(TEXT("kind"), TEXT("final"));
			}
		}
	}});
	Cases.Add({TEXT("soldier wave"), false, [](FJsonObject& Root)
	{
		if (auto E = Brennic(Root))
		{
			const TSharedPtr<FJsonObject>* Applies = nullptr;
			if (E->TryGetObjectField(TEXT("appliesTo"), Applies) && Applies)
			{
				(*Applies)->SetNumberField(TEXT("waveN"), 1);
				(*Applies)->SetStringField(TEXT("kind"), TEXT("soldiers"));
			}
		}
	}});
	Cases.Add({TEXT("surge 1.5"), false, [](FJsonObject& Root) { if (auto E = Brennic(Root)) { E->SetNumberField(TEXT("surgeTiers"), 1.5); } }});
	Cases.Add({TEXT("missing surge"), false, [](FJsonObject& Root) { if (auto E = Brennic(Root)) { E->RemoveField(TEXT("surgeTiers")); } }});
	Cases.Add({TEXT("three breaks"), false, [](FJsonObject& Root)
	{
		if (auto E = Brennic(Root))
		{
			const TArray<TSharedPtr<FJsonValue>>* Phases = nullptr;
			if (E->TryGetArrayField(TEXT("phases"), Phases) && Phases)
			{
				TArray<TSharedPtr<FJsonValue>> Copy = *Phases;
				TSharedRef<FJsonObject> Extra = MakeShared<FJsonObject>();
				Extra->SetNumberField(TEXT("atHpFraction"), 0.1);
				Copy.Add(MakeShared<FJsonValueObject>(Extra));
				E->SetArrayField(TEXT("phases"), Copy);
			}
		}
	}});
	for (const FCase& Case : Cases)
	{
		const FString Dir = WriteVariant(*this, Case.Mutate);
		if (Dir.IsEmpty())
		{
			bPass = false;
			continue;
		}
		FVrRuleset Variant;
		FString VariantError;
		const bool bLoaded = LoadFrom(Dir, Variant, VariantError);
		bPass &= TestEqual(*FString::Printf(TEXT("%s loads=%d (%s)"), Case.Name, Case.bLoads ? 1 : 0, *VariantError), bLoaded, Case.bLoads);
		if (Case.bLoads && bLoaded)
		{
			bPass &= TestEqual(*FString::Printf(TEXT("%s has no rival"), Case.Name), Variant.Rivals.Num(), 0);
		}
		if (!Case.bLoads)
		{
			bPass &= TestFalse(*FString::Printf(TEXT("%s leaves the overlay inactive"), Case.Name), Variant.bActive);
		}
		IFileManager::Get().DeleteDirectory(*Dir, false, true);
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaRivalsThresholds, "MageArena.Rivals.Thresholds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaRivalsThresholds::RunTest(const FString& Parameters)
{
	FRivalRig Rig;
	if (!OpenRig(*this, Rig))
	{
		return false;
	}
	bool bPass = true;
	// 95 - 32.29 = 62.71, above 95 x 0.66 = 62.7: no break.
	int32 Tick = HitRival(Rig, 32.29);
	bPass &= TestTrue(TEXT("62.71 left"), std::abs(RivalOf(Rig).Hp - 62.71) < 1.0e-9);
	bPass &= TestEqual(TEXT("no break above 62.7"), EventsAt(Rig.State, Tick, TEXT("phase")).Num(), 0);
	bPass &= TestEqual(TEXT("still phase 1"), Rig.Rules.RivalRuntime.PhasesBroken, 0);
	// 62.71 - 0.02 = 62.69, at or below 62.7: the break lands on the hit's tick.
	Tick = HitRival(Rig, 0.02);
	TArray<FArenaEvent> Phase = EventsAt(Rig.State, Tick, TEXT("phase"));
	bPass &= TestEqual(TEXT("first break on the hit tick"), Phase.Num(), 1);
	if (Phase.Num() == 1)
	{
		bPass &= TestEqual(TEXT("phase 2 entered"), Phase[0].Value, 2.0);
		bPass &= TestEqual(TEXT("threshold index 0"), Phase[0].TargetId.Get(-1), 0);
		bPass &= TestEqual(TEXT("rival actor"), Phase[0].ActorId, Rig.RivalId);
	}
	bPass &= TestEqual(TEXT("hit and break share the tick"), EventsAt(Rig.State, Tick, TEXT("hit")).Num(), 1);
	bPass &= TestTrue(TEXT("break tick recorded"), Rig.Rules.RivalRuntime.BreakTicks.Num() == 1 && Rig.Rules.RivalRuntime.BreakTicks[0] == Tick);
	bPass &= TestFalse(TEXT("first break grants nothing"), Rig.Rules.RivalRuntime.bGrantPending || PendingSunfall(RivalOf(Rig)));
	// The same HP does not break twice.
	StepArena(Rig.State, TMap<int32, FInputFrame>(), &Rig.Rules);
	bPass &= TestEqual(TEXT("no second phase-2 event"), EventsAt(Rig.State, Rig.State.Tick, TEXT("phase")).Num(), 0);
	// 62.69 - 31.33 = 31.36, above 95 x 0.33 = 31.35: no break.
	Tick = HitRival(Rig, 31.33);
	bPass &= TestEqual(TEXT("no break above 31.35"), EventsAt(Rig.State, Tick, TEXT("phase")).Num(), 0);
	// 31.36 - 0.02 = 31.34, at or below 31.35.
	Tick = HitRival(Rig, 0.02);
	Phase = EventsAt(Rig.State, Tick, TEXT("phase"));
	bPass &= TestEqual(TEXT("second break on the hit tick"), Phase.Num(), 1);
	if (Phase.Num() == 1)
	{
		bPass &= TestEqual(TEXT("phase 3 entered"), Phase[0].Value, 3.0);
		bPass &= TestEqual(TEXT("threshold index 1"), Phase[0].TargetId.Get(-1), 1);
	}
	bPass &= TestEqual(TEXT("two breaks"), Rig.Rules.RivalRuntime.PhasesBroken, 2);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaRivalsDoubleBreak, "MageArena.Rivals.DoubleBreak",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaRivalsDoubleBreak::RunTest(const FString& Parameters)
{
	FRivalRig Rig;
	if (!OpenRig(*this, Rig))
	{
		return false;
	}
	// 95 - 70 = 25: below both 62.7 and 31.35 in one hit.
	const int32 Tick = HitRival(Rig, 70.0);
	bool bPass = TestTrue(TEXT("25 left"), std::abs(RivalOf(Rig).Hp - 25.0) < 1.0e-9);
	const TArray<FArenaEvent> Phase = EventsAt(Rig.State, Tick, TEXT("phase"));
	bPass &= TestEqual(TEXT("both breaks on the hit tick"), Phase.Num(), 2);
	if (Phase.Num() == 2)
	{
		bPass &= TestTrue(TEXT("in order: phase 2, then phase 3"), Phase[0].Value == 2.0 && Phase[1].Value == 3.0
			&& Phase[0].TargetId.Get(-1) == 0 && Phase[1].TargetId.Get(-1) == 1);
	}
	// Two breaks, one tier each: 1 -> 3 for both. Four unlock events (two each).
	bPass &= TestEqual(TEXT("rival tier 3"), RivalOf(Rig).Tier, 3);
	bPass &= TestEqual(TEXT("player tier 3"), PlayerOf(Rig).Tier, 3);
	bPass &= TestEqual(TEXT("four unlocks"), EventsAt(Rig.State, Tick, TEXT("unlock")).Num(), 4);
	bPass &= TestTrue(TEXT("the last break opens with Sunfall on the same tick"), PendingSunfall(RivalOf(Rig))
		&& RivalOf(Rig).Pending->StartTick == Tick && Rig.Rules.RivalRuntime.GrantTick == Tick);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaRivalsSurge, "MageArena.Rivals.Surge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaRivalsSurge::RunTest(const FString& Parameters)
{
	bool bPass = true;
	{
		FRivalRig Rig;
		if (!OpenRig(*this, Rig))
		{
			return false;
		}
		// 95 - 33 = 62 <= 62.7: one break. surgeTiers 1: tier 1 -> 2 on both, each a normal unlock.
		const int32 Tick = HitRival(Rig, 33.0);
		bPass &= TestEqual(TEXT("rival 1 -> 2"), RivalOf(Rig).Tier, 2);
		bPass &= TestEqual(TEXT("player 1 -> 2"), PlayerOf(Rig).Tier, 2);
		const TArray<FArenaEvent> Unlocks = EventsAt(Rig.State, Tick, TEXT("unlock"));
		bPass &= TestEqual(TEXT("two unlock events"), Unlocks.Num(), 2);
		for (const FArenaEvent& Event : Unlocks)
		{
			bPass &= TestEqual(TEXT("unlock value is the new tier"), Event.Value, 2.0);
		}
		bPass &= TestTrue(TEXT("surge is an unlock tick for both clocks"), RivalOf(Rig).LastUnlockTick == Tick && PlayerOf(Rig).LastUnlockTick == Tick
			&& RivalOf(Rig).UnlockTicks.Last() == Tick && PlayerOf(Rig).UnlockTicks.Last() == Tick);
		// combat.json tierClock: tier 2 unlocks at 15 s and the minimum gap is 6 s. The surge came at the first second,
		// so neither gate held it back, and the normal clock now waits for tier 3 at 30 s.
		bPass &= TestTrue(TEXT("surge came before the 15 s unlock"), Tick < SimTicks(15.0));
	}
	{
		FRivalRig Rig;
		if (!OpenRig(*this, Rig))
		{
			return false;
		}
		RivalOf(Rig).Tier = TierCap;
		PlayerOf(Rig).Tier = 3;
		// Two breaks in one hit: the rival stays at 4 with no unlock; the player rises once to 4 and stops there.
		const int32 Tick = HitRival(Rig, 70.0);
		bPass &= TestEqual(TEXT("rival capped at 4"), RivalOf(Rig).Tier, TierCap);
		bPass &= TestEqual(TEXT("player capped at 4"), PlayerOf(Rig).Tier, TierCap);
		const TArray<FArenaEvent> Unlocks = EventsAt(Rig.State, Tick, TEXT("unlock"));
		bPass &= TestTrue(TEXT("one unlock, the player's 3 -> 4"), Unlocks.Num() == 1 && Unlocks[0].ActorId == Rig.PlayerId && Unlocks[0].Value == 4.0);
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaRivalsGrantedSunfall, "MageArena.Rivals.GrantedSunfall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaRivalsGrantedSunfall::RunTest(const FString& Parameters)
{
	FRivalRig Rig;
	if (!OpenRig(*this, Rig))
	{
		return false;
	}
	const FFireSpell* Sun = FindFireSpell(TEXT("fire_sunfall"));
	if (!TestNotNull(TEXT("fire_sunfall row"), Sun))
	{
		return false;
	}
	bool bPass = TestEqual(TEXT("Sunfall is tier 4"), Sun->Tier, 4);
	bPass &= TestEqual(TEXT("Sunfall costs 70"), Sun->Mana, 70.0);
	// First break: 95 - 34 = 61. Rival tier 1 -> 2.
	HitRival(Rig, 34.0);
	bPass &= TestEqual(TEXT("rival at tier 2 before the last break"), RivalOf(Rig).Tier, 2);
	const int32 SunSlot = KernelData().FireSpells.IndexOfByPredicate([](const FFireSpell& Spell) { return Spell.Id == TEXT("fire_sunfall"); });
	bPass &= TestTrue(TEXT("tier 2 does not offer Sunfall"), SunSlot != INDEX_NONE && FireSpellFor(RivalOf(Rig), SunSlot) == nullptr);
	RivalOf(Rig).Mana = 10.0;
	// Last break: 61 - 30 = 31 <= 31.35.
	const int32 Tick = HitRival(Rig, 30.0);
	const FActor& Rival = RivalOf(Rig);
	bPass &= TestTrue(TEXT("Sunfall starts on the last break"), PendingSunfall(Rival) && Rival.Pending->StartTick == Tick);
	bPass &= TestTrue(TEXT("mana below 70 and not spent"), Rival.Mana < Sun->Mana && Rival.Mana >= 10.0);
	bPass &= TestEqual(TEXT("tier 2 + surge = 3, still under Sunfall's 4"), Rival.Tier, 3);
	const TArray<FArenaEvent> Casts = EventsAt(Rig.State, Tick, TEXT("cast"));
	bPass &= TestTrue(TEXT("one tier-4 cast event"), Casts.Num() == 1 && Casts[0].ActorId == Rig.RivalId && Casts[0].Value == 4.0);
	// cast_s 0.80 at 60 Hz is 48 ticks; then the shadow (telegraph_s 1.20) is 72 ticks.
	const int32 CastTicks = SimTicks(Sun->CastS);
	bPass &= TestEqual(TEXT("48-tick cast"), CastTicks, 48);
	bPass &= TestEqual(TEXT("release tick"), Rival.Pending.IsSet() ? Rival.Pending->ReleaseTick : -1, Tick + CastTicks);
	while (Rig.State.Tick < Tick + CastTicks)
	{
		StepArena(Rig.State, TMap<int32, FInputFrame>(), &Rig.Rules);
	}
	const FTelegraph* Meteor = Rig.State.Telegraphs.FindByPredicate([&](const FTelegraph& Telegraph)
	{
		return Telegraph.OwnerId == Rig.RivalId && Telegraph.Kind == TEXT("area");
	});
	bPass &= TestNotNull(TEXT("the shadow is down"), Meteor);
	if (Meteor)
	{
		bPass &= TestEqual(TEXT("unblockable"), Meteor->Family, FString(TEXT("unblockable")));
		bPass &= TestEqual(TEXT("tier 4"), Meteor->Tier, 4);
		bPass &= TestEqual(TEXT("72-tick shadow"), Meteor->ResolveTick - Rig.State.Tick, SimTicks(Sun->TelegraphS));
	}
	bPass &= TestTrue(TEXT("loosed"), RivalOf(Rig).Fire.bSunfallLoosed);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaRivalsOnce, "MageArena.Rivals.Once",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaRivalsOnce::RunTest(const FString& Parameters)
{
	bool bPass = true;
	{
		FRivalRig Rig;
		if (!OpenRig(*this, Rig))
		{
			return false;
		}
		// A Sunfall the AI already loosed, with its 25 s cooldown running, does not stop the granted one.
		RivalOf(Rig).Fire.bSunfallLoosed = true;
		RivalOf(Rig).Fire.Cooldowns.Add({TEXT("fire_sunfall"), Rig.State.Tick + SimTicks(25.0)});
		HitRival(Rig, 70.0);
		bPass &= TestTrue(TEXT("granted despite an earlier Sunfall"), PendingSunfall(RivalOf(Rig)));
		// Heal back above both thresholds, break nothing new, and run a minute: one Sunfall cast in all.
		RivalOf(Rig).Hp = RivalOf(Rig).MaxHp;
		for (int32 Step = 0; Step < SimTicks(60.0); ++Step)
		{
			StepArena(Rig.State, TMap<int32, FInputFrame>(), &Rig.Rules);
		}
		HitRival(Rig, 90.0);
		int32 Sunfalls = 0;
		int32 Phases = 0;
		for (const FArenaEvent& Event : Rig.State.Events)
		{
			Sunfalls += Event.Kind == TEXT("cast") && Event.ActorId == Rig.RivalId && Event.Value == 4.0 ? 1 : 0;
			Phases += Event.Kind == TEXT("phase") ? 1 : 0;
		}
		bPass &= TestEqual(TEXT("one granted Sunfall per duel"), Sunfalls, 1);
		bPass &= TestEqual(TEXT("each threshold breaks once"), Phases, 2);
		bPass &= TestFalse(TEXT("nothing pending"), Rig.Rules.RivalRuntime.bGrantPending || PendingSunfall(RivalOf(Rig)));
	}
	{
		FRivalRig Rig;
		if (!OpenRig(*this, Rig))
		{
			return false;
		}
		// Mid-cast: the rival holds an Ember Dart windup that releases in 30 ticks. The grant waits for it.
		FPendingCast Dart;
		Dart.Kind = TEXT("fire");
		Dart.StartTick = Rig.State.Tick;
		Dart.ReleaseTick = Rig.State.Tick + 30;
		Dart.Aim = KernelData().PlayerSpawn;
		Dart.ActivationId = Rig.State.NextId++;
		Dart.SpellId = FString(TEXT("fire_bolt"));
		RivalOf(Rig).Pending = Dart;
		const int32 Tick = HitRival(Rig, 70.0);
		bPass &= TestTrue(TEXT("still in the dart"), RivalOf(Rig).Pending.IsSet() && !PendingSunfall(RivalOf(Rig)));
		bPass &= TestTrue(TEXT("grant waits"), Rig.Rules.RivalRuntime.bGrantPending && Rig.Rules.RivalRuntime.GrantTick == -1);
		while (Rig.State.Tick < Dart.ReleaseTick)
		{
			StepArena(Rig.State, TMap<int32, FInputFrame>(), &Rig.Rules);
		}
		bPass &= TestTrue(TEXT("Sunfall starts on the tick the dart releases"), PendingSunfall(RivalOf(Rig))
			&& RivalOf(Rig).Pending->StartTick == Dart.ReleaseTick && Rig.Rules.RivalRuntime.GrantTick == Dart.ReleaseTick);
		bPass &= TestTrue(TEXT("the dart flew"), Rig.State.Projectiles.ContainsByPredicate([&](const FProjectile& Shot) { return Shot.OwnerId == Rig.RivalId; }));
		bPass &= TestTrue(TEXT("break came first"), Tick < Dart.ReleaseTick);
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaRivalsNullPath, "MageArena.Rivals.NullPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaRivalsNullPath::RunTest(const FString& Parameters)
{
	if (!KernelData().bReady)
	{
		AddError(TEXT("kernel data is not ready"));
		return false;
	}
	bool bPass = true;
	// 1. combat.vr.json without the rivals block: the duel is the pre-change duel, event for event.
	const FString Dir = WriteVariant(*this, [](FJsonObject& Root) { Root.RemoveField(TEXT("rivals")); });
	FVrRuleset Without;
	FString Error;
	if (Dir.IsEmpty() || !LoadFrom(Dir, Without, Error))
	{
		AddError(FString::Printf(TEXT("variant without rivals: %s"), *Error));
		return false;
	}
	IFileManager::Get().DeleteDirectory(*Dir, false, true);
	FString Summary;
	const FString NoBlock = DuelDigest(Without, Summary);
	UE_LOG(LogMageArena, Log, TEXT("Rivals null-path digest=%s %s"), *NoBlock, *Summary);
	bPass &= TestEqual(TEXT("no rivals block: byte-for-byte the pre-change duel"), NoBlock, FString(PreChangeDigest));

	// 2. The live overlay with Brennic: the same seeds now break phases, so the log differs.
	FVrRuleset With;
	if (!LoadVrRuleset(With, Error, false))
	{
		AddError(Error);
		return false;
	}
	int32 PhaseEvents = 0;
	const FString Block = DuelDigest(With, Summary, &PhaseEvents);
	UE_LOG(LogMageArena, Log, TEXT("Rivals with-block digest=%s phases=%d %s"), *Block, PhaseEvents, *Summary);
	if (PhaseEvents > 0)
	{
		bPass &= TestNotEqual(TEXT("phases change the duel"), Block, FString(PreChangeDigest));
	}
	else
	{
		bPass &= TestEqual(TEXT("no break reached: the duel is unchanged"), Block, FString(PreChangeDigest));
	}
	return bPass;
}

#endif
