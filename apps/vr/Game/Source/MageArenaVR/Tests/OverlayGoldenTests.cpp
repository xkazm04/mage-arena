#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Kernel/ArenaKernel.h"
#include "Kernel/Fire.h"
#include "Kernel/Games.h"
#include "Kernel/KernelData.h"
#include "Kernel/SimConstants.h"
#include "Kernel/VrRules.h"
#include "MageArenaVR.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include <cmath>

// T54 (ARCHITECTURE-REVIEW-2026-10-21 A1): five fixed-seed bouts on the LIVE overlay, each recorded as an end state under
// apps/vr/data/scenarios-vr/overlay/. The 45 conformance vectors and Fire.Golden run with no ruleset and cannot see an
// overlay change; these can, so every T39 DF commit re-records them and its diff names the bouts it moved.
// -MageArenaRecordOverlayGolden rewrites the files; the default run only compares. It never writes fire-duel-competence-*.json.

namespace
{
// The overlay files a bout reads (combat.vr.json and arena-layout.json through LoadVrRuleset, spells-air.csv through
// KernelData) plus collar.json, which T39's DF-009 changes. Compared before any bout runs.
const TCHAR* const GoldenOverlayFiles[] = {
    TEXT("combat.vr.json"), TEXT("arena-layout.json"), TEXT("collar.json"), TEXT("schools/spells-air.csv")};

struct FGoldenBout
{
	const TCHAR* Name;
	const TCHAR* Exercises;
	uint32 Seed;
	int32 Wave;
	bool bFire;
	bool bAirFinal;
};

// Tiro waves (arena-tiers.json): index 0 soldiers, 1 creatures, 2 semifinal, 3 final. The reference water player in each.
const FGoldenBout GoldenBouts[] = {
    {TEXT("wave1-split"), TEXT("soldiers; the reference player casts with split hands under the latch"), 54001, 0, false, false},
    {TEXT("creatures-spit"), TEXT("cinder hounds and the ember spit"), 54002, 1, false, false},
    {TEXT("brennic-c1"), TEXT("Brennic (fire) and the HP-gated rival phases"), 54003, 2, true, false},
    {TEXT("lio-c1"), TEXT("Lio, the Air final (bAirFinal)"), 54004, 3, true, true},
    {TEXT("firewall-duel"), TEXT("the fire final; the mage AI raises the fire wall against the player's bolts"), 54005, 3, true, false},
};

struct FGoldenResult
{
	FString Phase;
	FString Outcome;
	int32 Ticks = 0;
	double DurationS = 0.0;
	FString Hash;
	double Hp = 0.0;
	double Mana = 0.0;
	int32 SunfallCast = 0;
	int32 SunfallRelease = 0;
	bool bSunfall = false;
	int32 WavesCleared = 0;
	int32 WallsRaised = 0;
	FString Kind;
	TMap<FString, int32> Events;
};

FString GoldenOutcome(const FString& Phase)
{
	if (Phase == TEXT("lost"))
	{
		return TEXT("loss");
	}
	if (Phase == TEXT("intermission") || Phase == TEXT("complete"))
	{
		return TEXT("win");
	}
	return TEXT("timeout");
}

const FComposition* GoldenPreset()
{
	return KernelData().Presets.FindByPredicate([](const FComposition& C)
	{ return C.Name == KernelData().ReferencePreset; });
}

bool RunGoldenBout(FAutomationTestBase& Test, const FVrRuleset& Rules, const FGoldenBout& Bout, FGoldenResult& Out)
{
	FGames Games;
	if (!Test.TestTrue(*FString::Printf(TEXT("%s creates"), Bout.Name),
	        TryCreateGames(Games, Bout.Seed, GoldenPreset(), Bout.Wave, true, Bout.bFire, &Rules, Bout.bAirFinal)))
	{
		return false;
	}
	const int32 Limit = SimTicks(KernelData().FightTimeoutS);
	while (Games.Phase == TEXT("active") && Games.State.Tick < Limit)
	{
		StepGames(Games);
	}
	const FActor* Player = SimFindActor(Games.State, Games.PlayerId);
	if (!Test.TestNotNull(TEXT("player"), Player))
	{
		return false;
	}
	Out.Phase = Games.Phase;
	Out.Outcome = GoldenOutcome(Games.Phase);
	Out.Ticks = Games.State.Tick;
	Out.DurationS = SimSeconds(Games.State.Tick);
	Out.Hash = StateHash(Games.State);
	Out.Hp = Player->Hp;
	Out.Mana = Player->Mana;
	Out.WavesCleared = Games.WavesCleared;
	// No kernel event marks a raised fire wall; the ruleset counts them.
	Out.WallsRaised = Games.VrRules.IsSet() ? Games.VrRules->WallsRaised : 0;
	Out.Kind = Games.SpawnLog.Num() > 0 ? Games.SpawnLog[0].Kind : FString();
	for (const FActor& Actor : Games.State.Actors)
	{
		if (Actor.Fire.bSchool)
		{
			Out.bSunfall = Actor.Fire.bSunfallLoosed;
			Out.SunfallCast = Actor.Fire.SunfallCastTick;
			Out.SunfallRelease = Actor.Fire.SunfallTick;
		}
	}
	for (const FArenaEvent& Event : Games.State.Events)
	{
		++Out.Events.FindOrAdd(Event.Kind);
	}
	Out.Events.KeySort([](const FString& Left, const FString& Right)
	{ return FCString::Strcmp(*Left, *Right) < 0; });
	return true;
}

FString GoldenPath(const FGoldenBout& Bout)
{
	return FPaths::ConvertRelativePathToFull(
	    FPaths::Combine(FPaths::ProjectDir(), TEXT("../data/scenarios-vr/overlay"), FString(Bout.Name) + TEXT(".json")));
}

// One field per line in a fixed order, so a re-record diffs line by line.
FString GoldenJson(const FGoldenBout& Bout, const FGoldenResult& Result)
{
	const int32 FileCount = UE_ARRAY_COUNT(GoldenOverlayFiles);
	FString Text = TEXT("{\n");
	Text += TEXT("  \"card\": \"T54\",\n");
	Text += FString::Printf(TEXT("  \"variant\": \"%s\",\n"), VrOverlayVariant());
	Text += FString::Printf(TEXT("  \"overlaySha256\": \"%s\",\n"), *VrOverlaySha256());
	Text += TEXT("  \"files\": {\n");
	for (int32 Index = 0; Index < FileCount; ++Index)
	{
		Text += FString::Printf(TEXT("    \"%s\": \"%s\"%s\n"), GoldenOverlayFiles[Index], *VrOverlayFileSha256(GoldenOverlayFiles[Index]),
		    Index + 1 < FileCount ? TEXT(",") : TEXT(""));
	}
	Text += TEXT("  },\n");
	Text += FString::Printf(TEXT("  \"bout\": \"%s\",\n"), Bout.Name);
	Text += FString::Printf(TEXT("  \"exercises\": \"%s\",\n"), Bout.Exercises);
	Text += FString::Printf(TEXT("  \"seed\": %u,\n"), Bout.Seed);
	Text += FString::Printf(TEXT("  \"wave\": %d,\n"), Bout.Wave);
	Text += FString::Printf(TEXT("  \"fire\": %d,\n"), Bout.bFire ? 1 : 0);
	Text += FString::Printf(TEXT("  \"airFinal\": %d,\n"), Bout.bAirFinal ? 1 : 0);
	Text += FString::Printf(TEXT("  \"outcome\": \"%s\",\n"), *Result.Outcome);
	Text += FString::Printf(TEXT("  \"phase\": \"%s\",\n"), *Result.Phase);
	Text += FString::Printf(TEXT("  \"ticks\": %d,\n"), Result.Ticks);
	Text += FString::Printf(TEXT("  \"durationS\": %.6f,\n"), Result.DurationS);
	Text += FString::Printf(TEXT("  \"hash\": \"%s\",\n"), *Result.Hash);
	Text += FString::Printf(TEXT("  \"sunfallCast\": %d,\n"), Result.SunfallCast);
	Text += FString::Printf(TEXT("  \"sunfallRelease\": %d,\n"), Result.SunfallRelease);
	Text += FString::Printf(TEXT("  \"sunfall\": %d,\n"), Result.bSunfall ? 1 : 0);
	Text += FString::Printf(TEXT("  \"hp\": %.6f,\n"), Result.Hp);
	Text += FString::Printf(TEXT("  \"mana\": %.6f,\n"), Result.Mana);
	Text += FString::Printf(TEXT("  \"wavesCleared\": %d,\n"), Result.WavesCleared);
	Text += FString::Printf(TEXT("  \"wallsRaised\": %d,\n"), Result.WallsRaised);
	Text += FString::Printf(TEXT("  \"kind\": \"%s\",\n"), *Result.Kind);
	Text += TEXT("  \"events\": {\n");
	int32 Written = 0;
	for (const TPair<FString, int32>& Pair : Result.Events)
	{
		++Written;
		Text += FString::Printf(TEXT("    \"%s\": %d%s\n"), *Pair.Key, Pair.Value, Written < Result.Events.Num() ? TEXT(",") : TEXT(""));
	}
	Text += TEXT("  }\n}\n");
	return Text;
}

TSharedPtr<FJsonObject> LoadGolden(FAutomationTestBase& Test, const FGoldenBout& Bout)
{
	const FString Path = GoldenPath(Bout);
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		Test.AddError(TEXT("Missing overlay golden ") + Path + TEXT(" (record with -MageArenaRecordOverlayGolden)"));
		return nullptr;
	}
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		Test.AddError(TEXT("Invalid overlay golden ") + Path);
		return nullptr;
	}
	return Root;
}

bool SameNumber(FAutomationTestBase& Test, const FGoldenBout& Bout, const FJsonObject& Golden, const FString& Field, double Actual)
{
	double Recorded = 0.0;
	// The files keep six decimals.
	if (Golden.TryGetNumberField(Field, Recorded) && std::abs(Recorded - Actual) <= 5.0e-7)
	{
		return true;
	}
	Test.AddError(FString::Printf(TEXT("%s: %s recorded %.6f now %.6f"), Bout.Name, *Field, Recorded, Actual));
	return false;
}

bool SameString(FAutomationTestBase& Test, const FGoldenBout& Bout, const FJsonObject& Golden, const FString& Field, const FString& Actual)
{
	FString Recorded;
	if (Golden.TryGetStringField(Field, Recorded) && Recorded == Actual)
	{
		return true;
	}
	Test.AddError(FString::Printf(TEXT("%s: %s recorded '%s' now '%s'"), Bout.Name, *Field, *Recorded, *Actual));
	return false;
}

// False, with one error per moved file, when any overlay file a bout reads differs from the bytes the goldens were recorded on.
bool SameOverlay(FAutomationTestBase& Test)
{
	bool bSame = true;
	for (const FGoldenBout& Bout : GoldenBouts)
	{
		const TSharedPtr<FJsonObject> Golden = LoadGolden(Test, Bout);
		const TSharedPtr<FJsonObject>* Files = nullptr;
		if (!Golden.IsValid() || !Golden->TryGetObjectField(TEXT("files"), Files))
		{
			bSame = false;
			continue;
		}
		for (const TCHAR* File : GoldenOverlayFiles)
		{
			FString Recorded;
			(*Files)->TryGetStringField(File, Recorded);
			const FString Now = VrOverlayFileSha256(File);
			if (Recorded != Now)
			{
				Test.AddError(FString::Printf(TEXT("overlay changed since recording: %s recorded %s now %s (%s); re-record with -MageArenaRecordOverlayGolden"),
				    File, *Recorded, *Now, Bout.Name));
				bSame = false;
			}
		}
	}
	return bSame;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaOverlayGolden, "MageArena.Overlay.Golden",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaOverlayGolden::RunTest(const FString& Parameters)
{
	if (!KernelData().bReady)
	{
		AddError(KernelData().Error.IsEmpty() ? TEXT("kernel data failed to load") : KernelData().Error);
		return false;
	}
	if (FCString::Strcmp(VrOverlayVariant(), TEXT("live")) != 0)
	{
		AddError(TEXT("the overlay goldens are recorded and compared live; unset -MageArenaProposal / MageArenaProposal"));
		return false;
	}
	FVrRuleset Rules;
	FString Error;
	if (!LoadVrRuleset(Rules, Error, false))
	{
		AddError(Error);
		return false;
	}
	const bool bRecord = FParse::Param(FCommandLine::Get(), TEXT("MageArenaRecordOverlayGolden"));
	AddInfo(FString::Printf(TEXT("overlay sha256 %s (%s)"), *VrOverlaySha256(), VrOverlayVariant()));
	// The recorded overlay first: a bout compared on other overlay bytes says nothing about the bout.
	if (!bRecord && !SameOverlay(*this))
	{
		return false;
	}
	bool bPass = true;
	for (const FGoldenBout& Bout : GoldenBouts)
	{
		FGoldenResult First;
		FGoldenResult Second;
		if (!RunGoldenBout(*this, Rules, Bout, First) || !RunGoldenBout(*this, Rules, Bout, Second))
		{
			return false;
		}
		bPass &= TestEqual(FString::Printf(TEXT("%s replay hash"), Bout.Name), Second.Hash, First.Hash);
		bPass &= TestEqual(FString::Printf(TEXT("%s replay ticks"), Bout.Name), Second.Ticks, First.Ticks);
		const FString Line = FString::Printf(TEXT("OVERLAY_GOLDEN bout=%s seed=%u wave=%d outcome=%s ticks=%d hash=%s hp=%.6f mana=%.6f walls=%d kinds=%d"),
		    Bout.Name, Bout.Seed, Bout.Wave, *First.Outcome, First.Ticks, *First.Hash, First.Hp, First.Mana, First.WallsRaised, First.Events.Num());
		UE_LOG(LogMageArena, Display, TEXT("%s"), *Line);
		AddInfo(Line);
		if (bRecord)
		{
			const FString Path = GoldenPath(Bout);
			if (!FFileHelper::SaveStringToFile(GoldenJson(Bout, First), *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				AddError(TEXT("could not write ") + Path);
				bPass = false;
			}
			continue;
		}
		const TSharedPtr<FJsonObject> Golden = LoadGolden(*this, Bout);
		if (!Golden.IsValid())
		{
			bPass = false;
			continue;
		}
		bPass &= SameString(*this, Bout, *Golden, TEXT("outcome"), First.Outcome);
		bPass &= SameString(*this, Bout, *Golden, TEXT("phase"), First.Phase);
		bPass &= SameString(*this, Bout, *Golden, TEXT("hash"), First.Hash);
		bPass &= SameString(*this, Bout, *Golden, TEXT("kind"), First.Kind);
		bPass &= SameNumber(*this, Bout, *Golden, TEXT("seed"), Bout.Seed);
		bPass &= SameNumber(*this, Bout, *Golden, TEXT("wave"), Bout.Wave);
		bPass &= SameNumber(*this, Bout, *Golden, TEXT("ticks"), First.Ticks);
		bPass &= SameNumber(*this, Bout, *Golden, TEXT("durationS"), First.DurationS);
		bPass &= SameNumber(*this, Bout, *Golden, TEXT("sunfallCast"), First.SunfallCast);
		bPass &= SameNumber(*this, Bout, *Golden, TEXT("sunfallRelease"), First.SunfallRelease);
		bPass &= SameNumber(*this, Bout, *Golden, TEXT("sunfall"), First.bSunfall ? 1.0 : 0.0);
		bPass &= SameNumber(*this, Bout, *Golden, TEXT("hp"), First.Hp);
		bPass &= SameNumber(*this, Bout, *Golden, TEXT("mana"), First.Mana);
		bPass &= SameNumber(*this, Bout, *Golden, TEXT("wavesCleared"), First.WavesCleared);
		bPass &= SameNumber(*this, Bout, *Golden, TEXT("wallsRaised"), First.WallsRaised);
		const TSharedPtr<FJsonObject>* Events = nullptr;
		if (!TestTrue(FString::Printf(TEXT("%s events field"), Bout.Name), Golden->TryGetObjectField(TEXT("events"), Events)))
		{
			bPass = false;
			continue;
		}
		bPass &= TestEqual(FString::Printf(TEXT("%s event kinds"), Bout.Name), First.Events.Num(), (*Events)->Values.Num());
		for (const TPair<FString, int32>& Pair : First.Events)
		{
			bPass &= SameNumber(*this, Bout, **Events, Pair.Key, Pair.Value);
		}
	}
	if (bRecord)
	{
		AddWarning(TEXT("recorded: apps/vr/data/scenarios-vr/overlay/*.json rewritten (-MageArenaRecordOverlayGolden)"));
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaOverlaySha256, "MageArena.Overlay.Sha256",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaOverlaySha256::RunTest(const FString& Parameters)
{
	// FIPS 180-4 vectors: the boot log and the goldens must name the same bytes as check-overlay.mjs.
	const auto Digest = [](const char* Text)
	{
		TArray<uint8> Bytes;
		Bytes.Append(reinterpret_cast<const uint8*>(Text), FCStringAnsi::Strlen(Text));
		return VrSha256Hex(Bytes);
	};
	bool bPass = TestEqual(TEXT("empty"), Digest(""), FString(TEXT("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")));
	bPass &= TestEqual(TEXT("abc"), Digest("abc"), FString(TEXT("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")));
	bPass &= TestEqual(TEXT("two blocks"), Digest("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
	    FString(TEXT("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1")));
	bPass &= TestEqual(TEXT("overlay hash is 64 hex"), VrOverlaySha256().Len(), 64);
	return bPass;
}

#endif
