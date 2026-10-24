#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/AbsorbResolver.h"
#include "Dom/JsonObject.h"
#include "Kernel/ArenaThreats.h"
#include "Kernel/KernelData.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// T44. A combat-data cell the loaders do not understand must stop the load, named, instead of silently becoming magic
// (absorbable and perfect-eligible). Every fixture here is an in-memory edit of a pinned file; the files are not touched.
namespace
{
const TCHAR* const WaterFile = TEXT("docs/design/baseline-fourteen-nights/design/data/spells-water.csv");
const TCHAR* const FireFile = TEXT("docs/design/reference-the-ledger/design/data/spells-fire.csv");
const TCHAR* const RosterFile = TEXT("docs/design/baseline-fourteen-nights/design/data/enemies.json");
const TCHAR* const CombatFile = TEXT("docs/design/baseline-fourteen-nights/design/data/combat.json");

// The line KindFromFamily logs for a family it cannot name. No assertion text below contains it: AddExpectedError
// would otherwise swallow that assertion's failure as the expected line.
const TCHAR* const UnknownHitMarker = TEXT("treated as unblockable");

struct FCellCase
{
	const TCHAR* Name;
	const TCHAR* File;
	const TCHAR* From;
	const TCHAR* To;
	// Every one of these must appear in the load error: the file, the row id, the column and the bad cell.
	TArray<const TCHAR*> Named;
};

bool ReadPinned(FAutomationTestBase& Test, const TCHAR* Relative, FString& Out)
{
	return Test.TestTrue(*FString::Printf(TEXT("read %s"), Relative), FFileHelper::LoadFileToString(Out, *KernelDataPinnedPath(Relative)));
}

// Loads the kernel with one pinned file replaced by its text after one exact, single-site substitution.
bool RunCellCase(FAutomationTestBase& Test, const FCellCase& Case)
{
	FString Text;
	if (!ReadPinned(Test, Case.File, Text))
	{
		return false;
	}
	const FString Mutated = Text.Replace(Case.From, Case.To, ESearchCase::CaseSensitive);
	// FString's == ignores case, and one edit below is only a change of case.
	bool bPass = Test.TestFalse(FString::Printf(TEXT("%s: the edit applies"), Case.Name), Mutated.Equals(Text, ESearchCase::CaseSensitive));
	TMap<FString, FString> Overrides;
	Overrides.Add(KernelDataPinnedPath(Case.File), Mutated);
	const FKernelData Loaded = LoadKernelDataWithOverrides(Overrides);
	bPass &= Test.TestFalse(FString::Printf(TEXT("%s: the load fails"), Case.Name), Loaded.bReady);
	for (const TCHAR* Needle : Case.Named)
	{
		bPass &= Test.TestTrue(FString::Printf(TEXT("%s: the error names %s (%s)"), Case.Name, Needle, *Loaded.Error), Loaded.Error.Contains(Needle));
	}
	return bPass;
}

// Reads the first object of Root[Array], or null.
TSharedPtr<FJsonObject> FirstOf(const FJsonObject& Root, const TCHAR* Array)
{
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (!Root.TryGetArrayField(Array, List) || List->Num() == 0)
	{
		return nullptr;
	}
	return (*List)[0]->AsObject();
}

// The pinned roster with Mutate applied to its parsed object, written back as text.
bool MutatedRoster(FAutomationTestBase& Test, TFunctionRef<bool(FJsonObject&)> Mutate, FString& OutText)
{
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!ReadPinned(Test, RosterFile, Text) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid() || !Mutate(*Root))
	{
		return false;
	}
	OutText.Reset();
	return FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&OutText));
}

// The pinned combat.json with Mutate applied to its absorb object, written back as text.
bool MutatedAbsorb(FAutomationTestBase& Test, TFunctionRef<void(FJsonObject& Absorb, FJsonObject& Perfect, FJsonObject& Reduction)> Mutate, FString& OutText)
{
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!ReadPinned(Test, CombatFile, Text) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		return false;
	}
	const TSharedPtr<FJsonObject> Absorb = Root->GetObjectField(TEXT("absorb"));
	Mutate(*Absorb, *Absorb->GetObjectField(TEXT("perfect")), *Absorb->GetObjectField(TEXT("reduction")));
	OutText.Reset();
	return FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&OutText));
}

TArray<TSharedPtr<FJsonValue>> Names(std::initializer_list<const TCHAR*> List)
{
	TArray<TSharedPtr<FJsonValue>> Out;
	for (const TCHAR* Name : List)
	{
		Out.Add(MakeShared<FJsonValueString>(Name));
	}
	return Out;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaKernelDataUnknownFamilyFailsLoad, "MageArena.KernelData.UnknownFamilyFailsLoad",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaKernelDataUnknownFamilyFailsLoad::RunTest(const FString& Parameters)
{
	// The trailing space is the realistic typo: it is invisible in a sheet and used to read as magic.
	bool bPass = RunCellCase(*this, {TEXT("water blockable with a trailing space"), WaterFile, TEXT("Bubble Shot,projectile 12 m/s,0.15,2.5,10,7,absorbable,"), TEXT("Bubble Shot,projectile 12 m/s,0.15,2.5,10,7,absorbable ,"), {TEXT("spells-water.csv"), TEXT("tide_orb:1:base"), TEXT("blockable"), TEXT("'absorbable '")}});
	bPass &= RunCellCase(*this, {TEXT("water blockable in the wrong case"), WaterFile, TEXT("Drown Coil,grab 2.5 m; root 1.5 s,0.40,16.0,40,25,UNBLOCKABLE,"), TEXT("Drown Coil,grab 2.5 m; root 1.5 s,0.40,16.0,40,25,unblockable,"), {TEXT("spells-water.csv"), TEXT("lash:4:base"), TEXT("blockable"), TEXT("'unblockable'")}});
	bPass &= RunCellCase(*this, {TEXT("fire blockable with a trailing space"), FireFile, TEXT("fire_flick,Flick Flame,1,cone 60 deg x 4 m,0.15,3.0,12,20,absorbable,"), TEXT("fire_flick,Flick Flame,1,cone 60 deg x 4 m,0.15,3.0,12,20,absorbable ,"), {TEXT("spells-fire.csv"), TEXT("fire_flick"), TEXT("blockable"), TEXT("'absorbable '")}});

	// The roster's two family sites: an attack and the onDeath burst.
	FString Attack;
	const bool bAttack = MutatedRoster(*this, [](FJsonObject& Root)
	{
		const TSharedPtr<FJsonObject> Conscript = FirstOf(Root, TEXT("soldiers"));
		const TSharedPtr<FJsonObject> Lunge = Conscript.IsValid() ? FirstOf(*Conscript, TEXT("attacks")) : nullptr;
		if (!Lunge.IsValid())
		{
			return false;
		}
		Lunge->SetStringField(TEXT("family"), TEXT("physical "));
		return true;
	}, Attack);
	FString Death;
	const bool bDeath = MutatedRoster(*this, [](FJsonObject& Root)
	{
		const TSharedPtr<FJsonObject> Hound = FirstOf(Root, TEXT("creatures"));
		const TSharedPtr<FJsonObject>* Burst = nullptr;
		if (!Hound.IsValid() || !Hound->TryGetObjectField(TEXT("onDeath"), Burst))
		{
			return false;
		}
		(*Burst)->SetStringField(TEXT("family"), TEXT("arcane"));
		return true;
	}, Death);
	if (!TestTrue(TEXT("the roster edits apply"), bAttack && bDeath))
	{
		return false;
	}
	const struct
	{
		const TCHAR* Name;
		const FString& Text;
		TArray<const TCHAR*> Named;
	} RosterCases[] = {
	    {TEXT("roster attack family with a trailing space"), Attack, {TEXT("enemies.json"), TEXT("conscript.spear_lunge"), TEXT("family"), TEXT("'physical '")}},
	    {TEXT("roster onDeath family unknown"), Death, {TEXT("enemies.json"), TEXT("cinder_hound"), TEXT("ember_burst"), TEXT("family"), TEXT("'arcane'")}},
	};
	for (const auto& Case : RosterCases)
	{
		TMap<FString, FString> Overrides;
		Overrides.Add(KernelDataPinnedPath(RosterFile), Case.Text);
		const FKernelData Loaded = LoadKernelDataWithOverrides(Overrides);
		bPass &= TestFalse(FString::Printf(TEXT("%s: the load fails"), Case.Name), Loaded.bReady);
		for (const TCHAR* Needle : Case.Named)
		{
			bPass &= TestTrue(FString::Printf(TEXT("%s: the error names %s (%s)"), Case.Name, Needle, *Loaded.Error), Loaded.Error.Contains(Needle));
		}
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaKernelDataGarbageNumberFailsLoad, "MageArena.KernelData.GarbageNumberFailsLoad",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaKernelDataGarbageNumberFailsLoad::RunTest(const FString& Parameters)
{
	// FCString::Atod reads "x" as 0: a free cooldown. A number cell that is not a number must refuse the load instead.
	bool bPass = RunCellCase(*this, {TEXT("water cooldown_s not a number"), WaterFile, TEXT("Bubble Shot,projectile 12 m/s,0.15,2.5,"), TEXT("Bubble Shot,projectile 12 m/s,0.15,x,"), {TEXT("spells-water.csv"), TEXT("tide_orb:1:base"), TEXT("cooldown_s"), TEXT("'x'")}});
	bPass &= RunCellCase(*this, {TEXT("water mana with a unit"), WaterFile, TEXT("Tide Lash,cone 90 deg x 3 m,0.10,2.0,8,"), TEXT("Tide Lash,cone 90 deg x 3 m,0.10,2.0,8mp,"), {TEXT("spells-water.csv"), TEXT("lash:1:base"), TEXT("mana"), TEXT("'8mp'")}});
	bPass &= RunCellCase(*this, {TEXT("fire cooldown_s not a number"), FireFile, TEXT("fire_flick,Flick Flame,1,cone 60 deg x 4 m,0.15,3.0,"), TEXT("fire_flick,Flick Flame,1,cone 60 deg x 4 m,0.15,x,"), {TEXT("spells-fire.csv"), TEXT("fire_flick"), TEXT("cooldown_s"), TEXT("'x'")}});
	bPass &= RunCellCase(*this, {TEXT("fire tier empty"), FireFile, TEXT("fire_lance,Brand Lance,2,"), TEXT("fire_lance,Brand Lance,,"), {TEXT("spells-fire.csv"), TEXT("fire_lance"), TEXT("tier"), TEXT("''")}});
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaKernelDataAbsorbRulesRejectBadNotAgainst, "MageArena.KernelData.AbsorbRulesRejectBadNotAgainst",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaKernelDataAbsorbRulesRejectBadNotAgainst::RunTest(const FString& Parameters)
{
	// FAbsorbResolver::Init and the ward detector read these rules without KernelData, so the rules check their own
	// contract: notAgainst is exactly physical and unblockable, every reduction is a fraction, the costs are not negative.
	FString Pinned;
	if (!ReadPinned(*this, CombatFile, Pinned))
	{
		return false;
	}
	FAbsorbRules Clean;
	FString CleanError;
	const bool bClean = Clean.LoadFromText(Pinned, CombatFile, CleanError);
	bool bPass = TestTrue(FString::Printf(TEXT("the pinned absorb block parses (%s)"), *CleanError), bClean);
	using FMutate = TFunction<void(FJsonObject&, FJsonObject&, FJsonObject&)>;
	const struct
	{
		const TCHAR* Name;
		FMutate Mutate;
		const TCHAR* Named;
	} Cases[] = {
	    {TEXT("an empty notAgainst"), [](FJsonObject&, FJsonObject& Perfect, FJsonObject&)
	{ Perfect.SetArrayField(TEXT("notAgainst"), Names({})); }, TEXT("notAgainst")},
	    {TEXT("notAgainst with magic added"), [](FJsonObject&, FJsonObject& Perfect, FJsonObject&)
	{ Perfect.SetArrayField(TEXT("notAgainst"), Names({TEXT("unblockable"), TEXT("physical"), TEXT("magic")})); }, TEXT("magic")},
	    {TEXT("notAgainst with magic for physical"), [](FJsonObject&, FJsonObject& Perfect, FJsonObject&)
	{ Perfect.SetArrayField(TEXT("notAgainst"), Names({TEXT("unblockable"), TEXT("magic")})); }, TEXT("magic")},
	    {TEXT("notAgainst with physical twice"), [](FJsonObject&, FJsonObject& Perfect, FJsonObject&)
	{ Perfect.SetArrayField(TEXT("notAgainst"), Names({TEXT("physical"), TEXT("physical")})); }, TEXT("notAgainst")},
	    {TEXT("notAgainst with a trailing space"), [](FJsonObject&, FJsonObject& Perfect, FJsonObject&)
	{ Perfect.SetArrayField(TEXT("notAgainst"), Names({TEXT("unblockable "), TEXT("physical")})); }, TEXT("'unblockable '")},
	    {TEXT("reduction.magic above one"), [](FJsonObject&, FJsonObject&, FJsonObject& Reduction)
	{ Reduction.SetNumberField(TEXT("magic"), 1.5); }, TEXT("reduction.magic")},
	    {TEXT("reduction.physical below zero"), [](FJsonObject&, FJsonObject&, FJsonObject& Reduction)
	{ Reduction.SetNumberField(TEXT("physical"), -0.1); }, TEXT("reduction.physical")},
	    {TEXT("reduction.outsideArc above one"), [](FJsonObject&, FJsonObject&, FJsonObject& Reduction)
	{ Reduction.SetNumberField(TEXT("outsideArc"), 2.0); }, TEXT("reduction.outsideArc")},
	    {TEXT("perfect.reduction above one"), [](FJsonObject&, FJsonObject& Perfect, FJsonObject&)
	{ Perfect.SetNumberField(TEXT("reduction"), 1.2); }, TEXT("perfect.reduction")},
	    {TEXT("a negative raise cost"), [](FJsonObject& Absorb, FJsonObject&, FJsonObject&)
	{ Absorb.SetNumberField(TEXT("raiseCostMana"), -5.0); }, TEXT("raiseCostMana")},
	    {TEXT("a negative held-move multiplier"), [](FJsonObject& Absorb, FJsonObject&, FJsonObject&)
	{ Absorb.SetNumberField(TEXT("moveSpeedWhileHeldMult"), -0.5); }, TEXT("moveSpeedWhileHeldMult")},
	};
	for (const auto& Case : Cases)
	{
		FString Text;
		if (!TestTrue(FString::Printf(TEXT("%s: the edit applies"), Case.Name), MutatedAbsorb(*this, Case.Mutate, Text)))
		{
			bPass = false;
			continue;
		}
		FAbsorbRules Rules;
		FString Error;
		const bool bLoaded = Rules.LoadFromText(Text, CombatFile, Error);
		bPass &= TestFalse(FString::Printf(TEXT("%s is refused"), Case.Name), bLoaded);
		bPass &= TestTrue(FString::Printf(TEXT("%s: the error names %s (%s)"), Case.Name, Case.Named, *Error), Error.Contains(Case.Named));
		bPass &= TestTrue(FString::Printf(TEXT("%s: the error names the file (%s)"), Case.Name, *Error), Error.Contains(TEXT("combat.json")));
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaKernelDataPinnedDataStillLoads, "MageArena.KernelData.PinnedDataStillLoads",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaKernelDataPinnedDataStillLoads::RunTest(const FString& Parameters)
{
	// The strictness above must refuse nothing in today's pin. The counts are the pin's (baeac66) rows and entries.
	const FKernelData Loaded = LoadKernelDataWithOverrides(TMap<FString, FString>());
	if (!TestTrue(FString::Printf(TEXT("the pinned data loads (%s)"), *Loaded.Error), Loaded.bReady))
	{
		return false;
	}
	bool bPass = TestEqual(TEXT("water spells: the 24 spells-water.csv rows"), Loaded.Spells.Num(), 24);
	bPass &= TestEqual(TEXT("fire spells: the 10 spells-fire.csv rows"), Loaded.FireSpells.Num(), 10);
	bPass &= TestEqual(TEXT("air spells: the 10 VR spells-air.csv rows"), Loaded.AirSpells.Num(), 10);
	bPass &= TestEqual(TEXT("enemies: 4 soldiers and 4 creatures"), Loaded.Enemies.Num(), 8);
	int32 Attacks = 0;
	int32 Deaths = 0;
	for (const FEnemySpec& Enemy : Loaded.Enemies)
	{
		Attacks += Enemy.Attacks.Num();
		Deaths += Enemy.OnDeath.IsSet() ? 1 : 0;
	}
	bPass &= TestEqual(TEXT("enemy attacks"), Attacks, 9);
	bPass &= TestEqual(TEXT("onDeath bursts"), Deaths, 1);
	bPass &= TestEqual(TEXT("perfect.notAgainst entries"), Loaded.Absorb.NotAgainst.Num(), 2);
	bPass &= TestEqual(TEXT("the same pin hash as the process-wide load"), Loaded.PinHash, KernelData().PinHash);
	// The independent readers of the absorb block load it too.
	FAbsorbResolver Resolver;
	FString Error;
	const bool bInit = Resolver.Init(Error);
	bPass &= TestTrue(FString::Printf(TEXT("FAbsorbResolver::Init reads the pinned rules (%s)"), *Error), bInit);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaThreatsUnknownFamilyIsNotAbsorbable, "MageArena.Threats.UnknownFamilyIsNotAbsorbable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaThreatsUnknownFamilyIsNotAbsorbable::RunTest(const FString& Parameters)
{
	// Fail in the safe direction: a family that reaches the hit without a loader naming it is not a free perfect.
	const FAbsorbRules& Rules = KernelData().Absorb;
	if (!TestTrue(TEXT("kernel data is ready"), KernelData().bReady))
	{
		return false;
	}
	// Control: magic itself is perfect-eligible, so a pass below is not the rules refusing everything.
	const FAbsorbVerdict Magic = EvaluateAbsorb(Rules, true, true, KindFromFamily(TEXT("magic")), 1, 0.0);
	bool bPass = TestTrue(TEXT("a fresh guarded magic hit is perfect"), Magic.bPerfect);
	const TCHAR* const Unknown[] = {TEXT("magic "), TEXT("Magic"), TEXT("absorbable"), TEXT("n/a"), TEXT(""), TEXT("arcane")};
	AddExpectedError(UnknownHitMarker, EAutomationExpectedErrorFlags::Contains, UE_ARRAY_COUNT(Unknown));
	for (const TCHAR* Family : Unknown)
	{
		const EAbsorbHitKind Kind = KindFromFamily(Family);
		const FAbsorbVerdict Verdict = EvaluateAbsorb(Rules, true, true, Kind, 1, 0.0);
		bPass &= TestTrue(FString::Printf(TEXT("family '%s' blocks the perfect"), Family), Rules.BlocksPerfect(Kind));
		bPass &= TestFalse(FString::Printf(TEXT("family '%s' is not a perfect absorb"), Family), Verdict.bPerfect);
		bPass &= TestEqual(FString::Printf(TEXT("family '%s' returns no perfect mana"), Family), Verdict.ManaReturned, 0.0);
	}
	return bPass;
}

#endif
