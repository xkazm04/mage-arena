#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Kernel/ArenaKernel.h"
#include "Kernel/Enemies.h"
#include "Kernel/KernelData.h"
#include "Kernel/SimTypes.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Session/ElementColour.h"

// T26, element-correct colour. ElementLook is the one function the presentation (SessionPresentation LookOf) calls for
// every projectile, telegraph and cast, so these tests hold the drawn colours. Expected values are the greybox palette
// literals (ElementColour.cpp): Water turquoise 0.009721/0.745404/0.745404, Fire ember orange 0.90/0.26/0.02, Air wind
// green 0.20/0.85/0.22, steel 0.68 grey, unblockable body 0.006995/0.006995/0.008023 with rim 0.745404/0.014444/0.009134.

namespace
{
const FLinearColor Water(0.009721f, 0.745404f, 0.745404f);
const FLinearColor Fire(0.90f, 0.26f, 0.02f);
const FLinearColor Air(0.20f, 0.85f, 0.22f);
const FLinearColor Steel(0.68f, 0.68f, 0.68f);
const FLinearColor Black(0.006995f, 0.006995f, 0.008023f);
const FLinearColor Red(0.745404f, 0.014444f, 0.009134f);

bool Same(FAutomationTestBase& Test, const FString& What, const FLinearColor& Actual, const FLinearColor& Expected)
{
	const bool bOk = Actual.Equals(Expected, 1.0e-5f);
	if (!bOk)
	{
		Test.AddError(FString::Printf(TEXT("%s: expected %s got %s"), *What, *Expected.ToString(), *Actual.ToString()));
	}
	return bOk;
}

struct FCast
{
	FArenaState State;
	int32 Player = 0;
	int32 WaterMage = 0;
	int32 FireMage = 0;
	int32 AirMage = 0;
	int32 Hound = 0;
	int32 Maw = 0;
	int32 Conscript = 0;
};

// A player, a Water mage, Brennic's school (Fire), Lio's school (Air), a cinder hound, a mire maw and a conscript.
FCast MakeCast()
{
	FCast Cast;
	Cast.State = CreateArena(26);
	const FSimVec Centre = KernelData().ArenaCentre;
	Cast.Player = AddMage(Cast.State, 0, Centre, TEXT("Player")).Id;
	Cast.WaterMage = AddMage(Cast.State, 1, FSimVec{Centre.X + 8.0, Centre.Y}, TEXT("Water mage")).Id;
	FActor& FireMage = AddMage(Cast.State, 1, FSimVec{Centre.X + 8.0, Centre.Y + 2.0}, TEXT("Brennic"));
	FireMage.Fire.bSchool = true;
	Cast.FireMage = FireMage.Id;
	FActor& AirMage = AddMage(Cast.State, 1, FSimVec{Centre.X + 8.0, Centre.Y - 2.0}, TEXT("Lio"));
	AirMage.Air.bSchool = true;
	Cast.AirMage = AirMage.Id;
	Cast.Hound = AddEnemy(Cast.State, TEXT("cinder_hound"), FSimVec{Centre.X + 5.0, Centre.Y + 4.0}).Id;
	Cast.Maw = AddEnemy(Cast.State, TEXT("mire_maw"), FSimVec{Centre.X + 5.0, Centre.Y - 4.0}).Id;
	Cast.Conscript = AddEnemy(Cast.State, TEXT("conscript"), FSimVec{Centre.X + 6.0, Centre.Y}).Id;
	return Cast;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaColourSchools, "MageArena.Colour.Schools",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaColourSchools::RunTest(const FString& Parameters)
{
	const FElementPalette Palette = FElementPalette::Greybox();
	const FCast Cast = MakeCast();
	bool bPass = true;
	struct FRow
	{
		const TCHAR* Who;
		int32 Owner;
		FLinearColor Expected;
	};
	const FRow Rows[] = {
		{TEXT("Water mage"), Cast.WaterMage, Water},
		{TEXT("Fire mage (Brennic)"), Cast.FireMage, Fire},
		{TEXT("air mage (Lio)"), Cast.AirMage, Air},
		{TEXT("mire maw"), Cast.Maw, Water},
	};
	// A projectile and a telegraph go through the same rule; both are checked so a split path would show.
	for (const FRow& Row : Rows)
	{
		const FThreatLook Projectile = ElementLook(Palette, Cast.State, Cast.Player, Row.Owner, TEXT("magic"));
		const FThreatLook Telegraph = ElementLook(Palette, Cast.State, Cast.Player, Row.Owner, TEXT("magic"));
		bPass &= Same(*this, FString::Printf(TEXT("%s projectile"), Row.Who), Projectile.Body, Row.Expected);
		bPass &= Same(*this, FString::Printf(TEXT("%s telegraph"), Row.Who), Telegraph.Body, Row.Expected);
		bPass &= TestFalse(*FString::Printf(TEXT("%s magic has no rim"), Row.Who), Projectile.bRim);
	}
	// Casts the player owns are the player's school (Water) whatever the family: a bolt, a reflected bolt, the player's
	// own Leviathan Orb (T20: its lane never reads as a threat to the seat).
	for (const TCHAR* Family : {TEXT("magic"), TEXT("unblockable"), TEXT("physical")})
	{
		const FThreatLook Look = ElementLook(Palette, Cast.State, Cast.Player, Cast.Player, Family);
		bPass &= Same(*this, FString::Printf(TEXT("player cast (%s)"), Family), Look.Body, Water);
		bPass &= TestFalse(*FString::Printf(TEXT("player cast (%s) no rim"), Family), Look.bRim);
	}
	// Fire is not drawn in Water any more, and the three absorb colours are three different colours.
	bPass &= TestFalse(TEXT("fire is not water"), Fire.Equals(Water, 0.05f));
	bPass &= TestFalse(TEXT("air is not water"), Air.Equals(Water, 0.05f));
	bPass &= TestFalse(TEXT("fire is not air"), Fire.Equals(Air, 0.05f));
	// The absorb colour of fire is kept off the leave rim: green channel 0.26 against 0.014.
	bPass &= TestTrue(TEXT("fire is clear of the unblockable rim red"), FMath::Abs(Palette.Fire.G - Palette.UnblockableRim.G) > 0.2f);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaColourUnblockable, "MageArena.Colour.Unblockable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaColourUnblockable::RunTest(const FString& Parameters)
{
	const FElementPalette Palette = FElementPalette::Greybox();
	const FCast Cast = MakeCast();
	bool bPass = true;
	const int32 Owners[] = {Cast.WaterMage, Cast.FireMage, Cast.AirMage, Cast.Hound, Cast.Maw, Cast.Conscript, 9999};
	for (const int32 Owner : Owners)
	{
		const FThreatLook Leave = ElementLook(Palette, Cast.State, Cast.Player, Owner, TEXT("unblockable"));
		bPass &= Same(*this, FString::Printf(TEXT("owner %d unblockable body"), Owner), Leave.Body, Black);
		bPass &= Same(*this, FString::Printf(TEXT("owner %d unblockable rim"), Owner), Leave.Rim, Red);
		bPass &= TestTrue(*FString::Printf(TEXT("owner %d unblockable has a rim"), Owner), Leave.bRim);
		const FThreatLook Dodge = ElementLook(Palette, Cast.State, Cast.Player, Owner, TEXT("physical"));
		bPass &= Same(*this, FString::Printf(TEXT("owner %d physical is steel"), Owner), Dodge.Body, Steel);
		bPass &= TestFalse(*FString::Printf(TEXT("owner %d physical has no rim"), Owner), Dodge.bRim);
	}
	for (const EElement Element : {EElement::Water, EElement::Fire, EElement::Air, EElement::Earth})
	{
		const FThreatLook Leave = ThreatLook(Palette, Element, TEXT("unblockable"));
		bPass &= Same(*this, FString::Printf(TEXT("school %d unblockable body"), static_cast<int32>(Element)), Leave.Body, Black);
		bPass &= Same(*this, FString::Printf(TEXT("school %d unblockable rim"), static_cast<int32>(Element)), Leave.Rim, Red);
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaColourHoundEmber, "MageArena.Colour.HoundEmber",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaColourHoundEmber::RunTest(const FString& Parameters)
{
	const FElementPalette Palette = FElementPalette::Greybox();
	FCast Cast = MakeCast();
	bool bPass = true;
	// The spit ember and its windup ring (magic, owned by the hound) are Fire-school magic: ember orange.
	bPass &= Same(*this, TEXT("hound ember"), ElementLook(Palette, Cast.State, Cast.Player, Cast.Hound, TEXT("magic")).Body, Fire);
	// The death ember is thrown by a hound that is down; it is still the hound's, still Fire.
	SimFindActor(Cast.State, Cast.Hound)->bDown = true;
	bPass &= Same(*this, TEXT("death ember"), ElementLook(Palette, Cast.State, Cast.Player, Cast.Hound, TEXT("magic")).Body, Fire);
	// Its bite stays steel.
	bPass &= Same(*this, TEXT("hound bite"), ElementLook(Palette, Cast.State, Cast.Player, Cast.Hound, TEXT("physical")).Body, Steel);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaColourCreatureSchools, "MageArena.Colour.CreatureSchools",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaColourCreatureSchools::RunTest(const FString& Parameters)
{
	if (!KernelData().bReady)
	{
		AddError(TEXT("kernel data failed to load"));
		return false;
	}
	// Every creature in the pinned enemies.json that throws magic (an attack or its death) has a school, so no magic
	// threat ever reaches the "no school" fallback.
	bool bPass = true;
	int32 Magic = 0;
	for (const FEnemySpec& Spec : KernelData().Enemies)
	{
		bool bThrowsMagic = Spec.OnDeath.IsSet() && Spec.OnDeath->Family == TEXT("magic");
		for (const FAttackSpec& Attack : Spec.Attacks)
		{
			bThrowsMagic |= Attack.Family == TEXT("magic");
		}
		if (!bThrowsMagic)
		{
			continue;
		}
		++Magic;
		bPass &= TestTrue(*FString::Printf(TEXT("%s throws magic and has a school"), *Spec.Id), CreatureElement(Spec.Id).IsSet());
	}
	bPass &= TestEqual(TEXT("magic creatures in enemies.json (cinder_hound, mire_maw)"), Magic, 2);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaColourStyleOverride, "MageArena.Colour.StyleOverride",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaColourStyleOverride::RunTest(const FString& Parameters)
{
	// The T16 style schema (apps/vr/data/vr/styles/style-<id>.json, "projectiles"): "magic" is the Water colour.
	const FString Json = TEXT(R"({"projectiles":{"magic":[0.1,0.2,0.3],"fire":[0.4,0.5,0.6],"air":[0.7,0.8,0.9],"steel":[0.25,0.25,0.25],)"
		R"("unblockableBody":[0.01,0.02,0.03],"unblockableRim":[0.9,0.1,0.8]}})");
	TSharedPtr<FJsonObject> Style;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Style) || !Style.IsValid())
	{
		AddError(TEXT("test JSON did not parse"));
		return false;
	}
	FElementPalette Palette = FElementPalette::Greybox();
	bool bPass = TestEqual(TEXT("six colours applied"), ApplyStylePalette(Palette, *Style), 6);
	const FCast Cast = MakeCast();
	// Through the active palette, the one the presentation draws with; restored afterwards.
	SetActiveElementPalette(Palette);
	const FElementPalette& Active = ActiveElementPalette();
	bPass &= Same(*this, TEXT("style water"), ElementLook(Active, Cast.State, Cast.Player, Cast.WaterMage, TEXT("magic")).Body, FLinearColor(0.1f, 0.2f, 0.3f));
	bPass &= Same(*this, TEXT("style player cast"), ElementLook(Active, Cast.State, Cast.Player, Cast.Player, TEXT("magic")).Body, FLinearColor(0.1f, 0.2f, 0.3f));
	bPass &= Same(*this, TEXT("style fire"), ElementLook(Active, Cast.State, Cast.Player, Cast.FireMage, TEXT("magic")).Body, FLinearColor(0.4f, 0.5f, 0.6f));
	bPass &= Same(*this, TEXT("style hound ember"), ElementLook(Active, Cast.State, Cast.Player, Cast.Hound, TEXT("magic")).Body, FLinearColor(0.4f, 0.5f, 0.6f));
	bPass &= Same(*this, TEXT("style air"), ElementLook(Active, Cast.State, Cast.Player, Cast.AirMage, TEXT("magic")).Body, FLinearColor(0.7f, 0.8f, 0.9f));
	bPass &= Same(*this, TEXT("style steel"), ElementLook(Active, Cast.State, Cast.Player, Cast.Conscript, TEXT("physical")).Body, FLinearColor(0.25f, 0.25f, 0.25f));
	const FThreatLook Leave = ElementLook(Active, Cast.State, Cast.Player, Cast.FireMage, TEXT("unblockable"));
	bPass &= Same(*this, TEXT("style unblockable body"), Leave.Body, FLinearColor(0.01f, 0.02f, 0.03f));
	bPass &= Same(*this, TEXT("style unblockable rim"), Leave.Rim, FLinearColor(0.9f, 0.1f, 0.8f));
	// A style that names no colour keeps the greybox palette.
	FElementPalette Untouched = FElementPalette::Greybox();
	bPass &= TestEqual(TEXT("an empty style applies nothing"), ApplyStylePalette(Untouched, FJsonObject()), 0);
	bPass &= Same(*this, TEXT("empty style keeps fire"), Untouched.Fire, Fire);
	ResetActiveElementPalette();
	bPass &= Same(*this, TEXT("reset restores the greybox fire"), ActiveElementPalette().Fire, Fire);
	return bPass;
}

#endif
