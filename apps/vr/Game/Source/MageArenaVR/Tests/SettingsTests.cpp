#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "Gestures/BlinkDetector.h"
#include "Gestures/SigilRecognizerSubsystem.h"
#include "Gestures/SigilStrokeBuilder.h"
#include "Gestures/StaffDetector.h"
#include "Gestures/WardDetector.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Hands/HandClip.h"
#include "Hands/HandClipPlayer.h"
#include "Hands/HandInputSubsystem.h"
#include "Hands/MageSettings.h"
#include "Kernel/ArenaKernel.h"
#include "Kernel/Catalog.h"
#include "Kernel/Enemies.h"
#include "Kernel/Games.h"
#include "Kernel/KernelData.h"
#include "Kernel/VrRules.h"
#include "MageArenaVR.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Session/ArenaSession.h"

#include <cmath>

namespace
{
struct FSettingsGuard
{
	FMageSettingsState Saved;
	FString PreviousOverride;
	FString TempPath;

	FSettingsGuard()
	{
		Saved = FMageSettings::Get();
		PreviousOverride = FMageSettings::FileOverride();
		TempPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MageArenaTests"), FGuid::NewGuid().ToString() + TEXT(".json"));
		FMageSettings::SetFileOverride(TempPath);
	}

	~FSettingsGuard()
	{
		IFileManager::Get().Delete(*TempPath, false, true, true);
		FMageSettings::SetFileOverride(PreviousOverride);
		FMageSettings::Restore(Saved);
		FMageSettings::SetDeviceFovForTest(0.f);
	}
};

struct FSessionRig
{
	UGameInstance* Instance = nullptr;
	UHandInputSubsystem* Hands = nullptr;
	USigilRecognizerSubsystem* Sigils = nullptr;
	UWardDetectorSubsystem* Wards = nullptr;
	UBlinkDetectorSubsystem* Blinks = nullptr;
	UStaffDetectorSubsystem* Staff = nullptr;

	bool Open(FAutomationTestBase& Test)
	{
		Instance = NewObject<UGameInstance>(GetTransientPackage());
		Instance->AddToRoot();
		Hands = NewObject<UHandInputSubsystem>(Instance);
		Hands->AddToRoot();
		Sigils = NewObject<USigilRecognizerSubsystem>(Instance);
		Sigils->AddToRoot();
		Sigils->LoadTemplatesFromDirectory(FPaths::Combine(FPaths::ProjectDir(), TEXT("Clips"), TEXT("templates")));
		Sigils->BindToHands(Hands);
		Wards = NewObject<UWardDetectorSubsystem>(Instance);
		Wards->AddToRoot();
		FString Error;
		if (!Wards->InitDetector(Error))
		{
			Test.AddError(Error);
			return false;
		}
		Wards->BindToHands(Hands);
		Blinks = NewObject<UBlinkDetectorSubsystem>(Instance);
		Blinks->AddToRoot();
		Blinks->BindToHands(Hands);
		Staff = NewObject<UStaffDetectorSubsystem>(Instance);
		Staff->AddToRoot();
		Staff->BindToHands(Hands);
		return true;
	}

	void Close()
	{
		if (Staff)
		{
			Staff->BindToHands(nullptr);
			Staff->RemoveFromRoot();
			Staff->MarkAsGarbage();
			Staff = nullptr;
		}
		if (Blinks)
		{
			Blinks->BindToHands(nullptr);
			Blinks->RemoveFromRoot();
			Blinks->MarkAsGarbage();
			Blinks = nullptr;
		}
		if (Wards)
		{
			Wards->BindToHands(nullptr);
			Wards->RemoveFromRoot();
			Wards->MarkAsGarbage();
			Wards = nullptr;
		}
		if (Sigils)
		{
			Sigils->BindToHands(nullptr);
			Sigils->RemoveFromRoot();
			Sigils->MarkAsGarbage();
			Sigils = nullptr;
		}
		if (Hands)
		{
			Hands->StopAll();
			Hands->RemoveFromRoot();
			Hands->MarkAsGarbage();
			Hands = nullptr;
		}
		if (Instance)
		{
			Instance->RemoveFromRoot();
			Instance->MarkAsGarbage();
			Instance = nullptr;
		}
	}
};

const TCHAR* GActions[] = {
	TEXT("sigil-line1"),
	TEXT("sigil-line2"),
	TEXT("sigil-line3"),
	TEXT("mudra"),
	TEXT("bolt"),
	TEXT("ward-raise"),
	TEXT("blink-left"),
	TEXT("blink-right"),
	TEXT("blink-back"),
	TEXT("staff-plant"),
	TEXT("staff-lift"),
	TEXT("both-palms"),
};

const TCHAR* GVariantNames[] = {TEXT("normal"), TEXT("slow"), TEXT("sloppy")};
const EClipVariant GVariants[] = {EClipVariant::Normal, EClipVariant::Slow, EClipVariant::Sloppy};

bool PalmsUp(UHandInputSubsystem* Hands)
{
	FHandFrame Left;
	FHandFrame Right;
	if (!Hands->GetLatest(EControllerHand::Left, Left) || !Hands->GetLatest(EControllerHand::Right, Right))
	{
		return false;
	}
	if (Left.Confidence < 0.2f || Right.Confidence < 0.2f)
	{
		return false;
	}
	const FVector Forward(1.0, 0.0, 0.0);
	auto Raised = [&Forward](const FHandFrame& Frame)
	{
		return FWardDetector::PalmHeightMetres(Frame) >= FWardThresholds::RaiseHeightM
			&& FWardDetector::AngleToFacingDeg(FWardDetector::PalmNormal(Frame), Forward) <= FWardThresholds::RaiseAngleDeg;
	};
	return Raised(Left) && Raised(Right);
}

struct FSnap
{
	bool bPlayed = false;
	int32 Blinks = 0;
	int32 Bolts = 0;
	int32 Pad = -1;
	double Yaw = 0.0;
	int32 Raises = 0;
	bool bWardUp = false;
	int32 Plants = 0;
	int32 Lifts = 0;
	int32 Casts = 0;
	FName Line = NAME_None;
	bool bPalms = false;
	bool bHasCasting = false;
	bool bHasWard = false;
};

bool StepClip(UHandInputSubsystem* Hands, bool& bPalms)
{
	UHandClipPlayer* Player = Hands->GetClipPlayer();
	if (!Player || Player->GetClip().GetTracks().Num() == 0)
	{
		return false;
	}
	const double Duration = Player->GetDuration();
	const FHandClipTrack& Track = Player->GetClip().GetTracks()[0];
	for (int32 Index = 1; Index < Track.Frames.Num(); ++Index)
	{
		if (Player->GetTime() + 1.0e-9 >= Duration)
		{
			break;
		}
		const double Dt = Track.Frames[Index].TimeSeconds - Player->GetTime();
		if (Dt > 1.0e-9)
		{
			Hands->Step(Dt);
		}
		if (Player->GetTime() >= Duration * 0.55 && PalmsUp(Hands))
		{
			bPalms = true;
		}
	}
	if (PalmsUp(Hands))
	{
		bPalms = true;
	}
	return true;
}

FSnap PlayOnce(FSessionRig& Rig, bool bLeft, const TCHAR* Action, EClipVariant Variant, int32& Casts, FName& Line)
{
	FSnap Snap;
	FMageSettings::SetHand(bLeft ? EMageHand::Left : EMageHand::Right, false);
	Casts = 0;
	Line = NAME_None;
	Rig.Hands->SetWardHeld(false, Variant);
	Rig.Hands->StopAll();
	Rig.Blinks->ResetDetector();
	Rig.Blinks->SetActivePad(1);
	Rig.Staff->GetDetector().Reset();
	const int32 RaisesBefore = Rig.Wards->GetRaiseCount();
	if (FCString::Strcmp(Action, TEXT("ward-raise")) == 0)
	{
		Rig.Hands->SetWardHeld(true, Variant);
	}
	else
	{
		Rig.Hands->PlayQuickAction(Action, Variant);
	}
	bool bPalms = false;
	if (!StepClip(Rig.Hands, bPalms))
	{
		return Snap;
	}
	Rig.Blinks->FlushPending();
	Snap.bPlayed = true;
	Snap.bPalms = bPalms;
	Snap.Blinks = Rig.Blinks->GetDetector().GetBlinkCount();
	Snap.Bolts = Rig.Blinks->GetDetector().GetBoltCount();
	Snap.Yaw = Snap.Blinks > 0 ? Rig.Blinks->GetDetector().GetLastBlink().YawDegrees : Rig.Blinks->GetDetector().GetLastBolt().YawDegrees;
	Snap.Pad = Snap.Blinks > 0 ? Rig.Blinks->GetDetector().GetLastBlink().PadIndex : -1;
	Snap.Raises = Rig.Wards->GetRaiseCount() - RaisesBefore;
	Snap.bWardUp = Rig.Wards->GetState().bRaised;
	Snap.Plants = Rig.Staff->GetDetector().GetPlantCount();
	Snap.Lifts = Rig.Staff->GetDetector().GetLiftCount();
	Snap.Casts = Casts;
	Snap.Line = Line;
	const FHandClip& Clip = Rig.Hands->GetClipPlayer()->GetClip();
	Snap.bHasCasting = Clip.GetHands().Contains(FMageSettings::CastingHand());
	Snap.bHasWard = Clip.GetHands().Contains(FMageSettings::WardHand());
	return Snap;
}

bool SameSnap(FAutomationTestBase& Test, const TCHAR* Label, const FSnap& Left, const FSnap& Right)
{
	bool bPass = Test.TestTrue(*FString::Printf(TEXT("%s played"), Label), Left.bPlayed && Right.bPlayed);
	bPass &= Test.TestEqual(*FString::Printf(TEXT("%s blinks"), Label), Left.Blinks, Right.Blinks);
	bPass &= Test.TestEqual(*FString::Printf(TEXT("%s bolts"), Label), Left.Bolts, Right.Bolts);
	bPass &= Test.TestEqual(*FString::Printf(TEXT("%s pad"), Label), Left.Pad, Right.Pad);
	bPass &= Test.TestEqual(*FString::Printf(TEXT("%s raises"), Label), Left.Raises, Right.Raises);
	bPass &= Test.TestTrue(*FString::Printf(TEXT("%s ward up"), Label), Left.bWardUp == Right.bWardUp);
	bPass &= Test.TestEqual(*FString::Printf(TEXT("%s plants"), Label), Left.Plants, Right.Plants);
	bPass &= Test.TestEqual(*FString::Printf(TEXT("%s lifts"), Label), Left.Lifts, Right.Lifts);
	bPass &= Test.TestEqual(*FString::Printf(TEXT("%s casts"), Label), Left.Casts, Right.Casts);
	bPass &= Test.TestEqual(*FString::Printf(TEXT("%s line"), Label), Left.Line, Right.Line);
	if (Left.Blinks > 0 || Left.Bolts > 0)
	{
		bPass &= Test.TestTrue(*FString::Printf(TEXT("%s yaw"), Label), std::abs(Left.Yaw - Right.Yaw) < 0.05);
	}
	return bPass;
}

bool InArc(const FArenaSession& Session, int32 Pad, const FSimVec& Point, double ArcDeg, FString& Why)
{
	// The pad faces where arena-layout.json authors it (toward the arena centre, not radially out), so the forward
	// comes from the layout data. The bearing test itself is written here: atan2 against that forward, +/- ArcDeg.
	const FSimVec Origin = Session.PadKernel(Pad);
	FSimVec Forward{1.0, 0.0};
	if (Session.HasLayout())
	{
		if (const FRunePad* RunePad = Session.GetLayout().FindPad(Pad))
		{
			const FVector Flat = Session.GetLayout().FlatForwardM(*RunePad);
			if (Flat.Size2D() > 1.0e-6)
			{
				Forward = FSimVec{Flat.X / Flat.Size2D(), Flat.Y / Flat.Size2D()};
			}
		}
	}
	const double Dx = Point.X - Origin.X;
	const double Dy = Point.Y - Origin.Y;
	if (std::hypot(Dx, Dy) <= 1.0e-6)
	{
		return true;
	}
	const double Ahead = Dx * Forward.X + Dy * Forward.Y;
	const double Side = Dy * Forward.X - Dx * Forward.Y;
	const double Bearing = std::atan2(Side, Ahead);
	const double Limit = ArcDeg * (PI / 180.0);
	if (std::abs(Bearing) <= Limit + 1.0e-4)
	{
		return true;
	}
	Why = FString::Printf(TEXT("pad %d bearing %.3f deg limit %.3f at (%.3f, %.3f)"),
		Pad, Bearing * (180.0 / PI), ArcDeg, Point.X, Point.Y);
	return false;
}

// Expected windups, hand-computed from the pinned data at 60 Hz (ticks = ceil(seconds * 60)), Gentle doubles them.
// enemies.json: conscript spear_lunge 0.6 s -> 36 / 72; slinger sling_stone 0.5 s -> 30 / 60;
// shieldman shield_bash 0.7 s -> 42 / 84; netter net_cast 0.8 s -> 48 / 96.
bool EnemyWindup(const FActor& Actor, int32 Span, double Mult)
{
	const bool bDouble = Mult > 1.5;
	const FString Id = Actor.Enemy->Id;
	if (Id == TEXT("conscript")) return Span == (bDouble ? 72 : 36);
	if (Id == TEXT("slinger")) return Span == (bDouble ? 60 : 30);
	if (Id == TEXT("shieldman")) return Span == (bDouble ? 84 : 42);
	if (Id == TEXT("netter")) return Span == (bDouble ? 96 : 48);
	return false;
}

// Mage area telegraphs that outlive the cast (spells-fire.csv telegraph_s): Pyre Circle 0.80 s -> 48 / 96,
// Sunfall 1.20 s -> 72 / 144. The telegraph does not name its spell, so either value is accepted.
bool SpellTelegraph(int32 Span, double Mult)
{
	const bool bDouble = Mult > 1.5;
	return bDouble ? (Span == 96 || Span == 144) : (Span == 48 || Span == 72);
}

// Fire cast windups (spells-fire.csv): windup = cast_s, or max(cast_s, telegraph_s) unless the notes say the
// shadow grows (Sunfall: sequential, so cast_s 0.80). Ticks single / doubled:
// bolt 0.00 -> 0..1, flick 0.15 -> 9 / 18, cinderstep 0.00 -> 0..1, kindle 0.40 -> 24 / 48, lance 0.60 -> 36 / 72,
// ring 0.30 -> 18 / 36, pyre max(0.50, 0.80) -> 48 / 96, furnace 0.50 -> 30 / 60, sunfall 0.80 -> 48 / 96,
// wrath 0.20 -> 12 / 24.
bool FirePending(const FActor& Actor, double Mult)
{
	if (!Actor.Pending.IsSet() || (Actor.Pending->Kind != TEXT("fire") && Actor.Pending->Kind != TEXT("air")) || !Actor.Pending->SpellId.IsSet())
	{
		return false;
	}
	const bool bDouble = Mult > 1.5;
	const int32 Span = Actor.Pending->ReleaseTick - Actor.Pending->StartTick;
	const FString Id = Actor.Pending->SpellId.GetValue();
	// T23 air rows (spells-air.csv): max(cast_s, telegraph_s) at 60 Hz, and the gentle x2 seconds ceil'd to ticks.
	if (Id == TEXT("air_bolt") || Id == TEXT("air_slipstream")) return Span == 0 || Span == 1;
	if (Id == TEXT("air_shear")) return Span == (bDouble ? 15 : 8);
	if (Id == TEXT("air_veer")) return Span == (bDouble ? 60 : 30);
	if (Id == TEXT("air_downdraft")) return Span == (bDouble ? 84 : 42);
	if (Id == TEXT("air_form")) return Span == (bDouble ? 18 : 9);
	if (Id == TEXT("air_squall")) return Span == (bDouble ? 36 : 18);
	if (Id == TEXT("air_eye")) return Span == (bDouble ? 48 : 24);
	if (Id == TEXT("air_tempest")) return Span == (bDouble ? 144 : 72);
	if (Id == TEXT("air_cyclone")) return Span == (bDouble ? 24 : 12);
	if (Id == TEXT("fire_bolt") || Id == TEXT("fire_cinderstep")) return Span == 0 || Span == 1;
	if (Id == TEXT("fire_flick")) return Span == (bDouble ? 18 : 9);
	if (Id == TEXT("fire_kindle")) return Span == (bDouble ? 48 : 24);
	if (Id == TEXT("fire_lance")) return Span == (bDouble ? 72 : 36);
	if (Id == TEXT("fire_ring")) return Span == (bDouble ? 36 : 18);
	if (Id == TEXT("fire_pyre")) return Span == (bDouble ? 96 : 48);
	if (Id == TEXT("fire_furnace")) return Span == (bDouble ? 60 : 30);
	if (Id == TEXT("fire_sunfall")) return Span == (bDouble ? 96 : 48);
	if (Id == TEXT("fire_wrath")) return Span == (bDouble ? 24 : 12);
	return false;
}

bool UnmultipliedPending(const FActor& Actor)
{
	if (!Actor.Pending.IsSet())
	{
		return true;
	}
	const int32 Span = Actor.Pending->ReleaseTick - Actor.Pending->StartTick;
	if (Actor.Pending->Kind == TEXT("staff"))
	{
		return Span == SimTicks(KernelData().StaffWindupS);
	}
	if (Actor.Pending->Kind == TEXT("spell"))
	{
		const FSpell* Spell = Actor.Pending->SpellId.IsSet() ? FindSpellById(Actor.Pending->SpellId.GetValue()) : nullptr;
		if (!Spell)
		{
			return false;
		}
		return Span == SimTicks(std::max(Spell->CastS, Spell->TelegraphS));
	}
	if (Actor.Pending->Kind == TEXT("fire"))
	{
		return FirePending(Actor, 1.0);
	}
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaSettingsFile, "MageArena.Settings.File",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaSettingsFile::RunTest(const FString& Parameters)
{
	FSettingsGuard Guard;
	IFileManager::Get().Delete(*Guard.TempPath, false, true, true);
	FMageSettings::Reload();
	bool bPass = TestTrue(TEXT("missing file stays missing"), !FPaths::FileExists(Guard.TempPath));
	bPass &= TestFalse(TEXT("missing hand"), FMageSettings::IsLeftHanded());
	bPass &= TestFalse(TEXT("missing narrow"), FMageSettings::IsNarrow());
	bPass &= TestFalse(TEXT("missing gentle"), FMageSettings::IsGentle());

	const FString Valid = TEXT("{\"hand\":\"left\",\"fovMode\":\"narrow\",\"gentle\":true}\n");
	bPass &= TestTrue(TEXT("wrote valid settings"), FFileHelper::SaveStringToFile(Valid, *Guard.TempPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
	FMageSettings::Reload();
	bPass &= TestTrue(TEXT("loaded left"), FMageSettings::IsLeftHanded());
	bPass &= TestTrue(TEXT("loaded narrow"), FMageSettings::IsNarrow());
	bPass &= TestTrue(TEXT("loaded gentle"), FMageSettings::IsGentle());
	bPass &= TestTrue(TEXT("casting hand"), FMageSettings::CastingHand() == EControllerHand::Left);
	bPass &= TestTrue(TEXT("ward hand"), FMageSettings::WardHand() == EControllerHand::Right);

	const FString Corrupt = TEXT("{not json");
	bPass &= TestTrue(TEXT("wrote corrupt settings"), FFileHelper::SaveStringToFile(Corrupt, *Guard.TempPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
	FMageSettings::Reload();
	bPass &= TestFalse(TEXT("corrupt hand"), FMageSettings::IsLeftHanded());
	bPass &= TestFalse(TEXT("corrupt narrow"), FMageSettings::IsNarrow());
	bPass &= TestFalse(TEXT("corrupt gentle"), FMageSettings::IsGentle());
	FString LeftBehind;
	bPass &= TestTrue(TEXT("corrupt file still readable"), FFileHelper::LoadFileToString(LeftBehind, *Guard.TempPath));
	bPass &= TestEqual(TEXT("corrupt file was not overwritten"), LeftBehind, Corrupt);

	IConsoleManager::Get().ProcessUserConsoleInput(TEXT("MageArena.Settings.Hand left"), *GLog, nullptr);
	IConsoleManager::Get().ProcessUserConsoleInput(TEXT("MageArena.Settings.Fov narrow"), *GLog, nullptr);
	IConsoleManager::Get().ProcessUserConsoleInput(TEXT("MageArena.Settings.Gentle on"), *GLog, nullptr);
	FString Written;
	bPass &= TestTrue(TEXT("command wrote the override file"), FFileHelper::LoadFileToString(Written, *Guard.TempPath));
	bPass &= TestEqual(TEXT("command file"), Written, TEXT("{\"hand\":\"left\",\"fovMode\":\"narrow\",\"gentle\":true}\n"));
	bPass &= TestTrue(TEXT("command left"), FMageSettings::IsLeftHanded());
	bPass &= TestTrue(TEXT("command narrow"), FMageSettings::IsNarrow());
	bPass &= TestTrue(TEXT("command gentle"), FMageSettings::IsGentle());

	IFileManager::Get().Delete(*Guard.TempPath, false, true, true);
	const FString OriginalCmd = FCommandLine::Get();
	FCommandLine::Set(*FString::Printf(TEXT("%s -MageArenaHand=right -MageArenaFov=wide -MageArenaGentle=0"), *OriginalCmd));
	FMageSettings::Reload();
	bPass &= TestFalse(TEXT("cmdline hand"), FMageSettings::IsLeftHanded());
	bPass &= TestFalse(TEXT("cmdline narrow"), FMageSettings::IsNarrow());
	bPass &= TestFalse(TEXT("cmdline gentle"), FMageSettings::IsGentle());
	bPass &= TestFalse(TEXT("cmdline did not write file"), FPaths::FileExists(Guard.TempPath));
	FCommandLine::Set(*OriginalCmd);

	FMageSettings::SetFov(EMageFovMode::Wide, false);
	FMageSettings::SetDeviceFovForTest(89.f);
	bPass &= TestTrue(TEXT("offer under 90"), FMageSettings::ShouldOfferNarrow());
	FMageSettings::SetDeviceFovForTest(90.f);
	bPass &= TestFalse(TEXT("90 does not offer"), FMageSettings::ShouldOfferNarrow());
	FMageSettings::SetDeviceFovForTest(70.f);
	FMageSettings::SetFov(EMageFovMode::Narrow, false);
	bPass &= TestFalse(TEXT("already narrow does not offer"), FMageSettings::ShouldOfferNarrow());
	bPass &= TestTrue(TEXT("spawn arc"), FMageSettings::SpawnArcDeg() == 40.f);
	bPass &= TestTrue(TEXT("offer threshold"), FMageSettings::OfferBelowDeg() == 90.f);
	bPass &= TestTrue(TEXT("narrow camera"), FMageSettings::NarrowCameraFovDeg() == 70.f);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaSettingsMirror, "MageArena.Settings.Mirror",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaSettingsMirror::RunTest(const FString& Parameters)
{
	FSettingsGuard Guard;
	FMageSettings::SetHand(EMageHand::Right, false);
	FMageSettings::SetFov(EMageFovMode::Wide, false);
	FMageSettings::SetGentle(false, false);
	FSessionRig Rig;
	if (!Rig.Open(*this))
	{
		Rig.Close();
		return false;
	}
	int32 Casts = 0;
	FName Line = NAME_None;
	Rig.Sigils->OnSigilCast.AddLambda([&Casts, &Line](FName InLine, float, double)
	{
		++Casts;
		Line = InLine;
	});

	bool bPass = true;
	for (int32 VariantIndex = 0; VariantIndex < 3; ++VariantIndex)
	{
		for (const TCHAR* Action : GActions)
		{
			const FString Label = FString::Printf(TEXT("%s.%s"), Action, GVariantNames[VariantIndex]);
			const FSnap Right = PlayOnce(Rig, false, Action, GVariants[VariantIndex], Casts, Line);
			const FSnap Left = PlayOnce(Rig, true, Action, GVariants[VariantIndex], Casts, Line);
			bPass &= SameSnap(*this, *Label, Left, Right);
			if (FCString::Strncmp(Action, TEXT("sigil-"), 6) == 0 || FCString::Strcmp(Action, TEXT("bolt")) == 0
				|| FCString::Strncmp(Action, TEXT("blink-"), 6) == 0)
			{
				bPass &= TestTrue(*FString::Printf(TEXT("%s uses the casting hand"), *Label), Right.bHasCasting && Left.bHasCasting);
				bPass &= TestFalse(*FString::Printf(TEXT("%s leaves the ward hand"), *Label), Right.bHasWard || Left.bHasWard);
			}
			if (FCString::Strcmp(Action, TEXT("ward-raise")) == 0)
			{
				bPass &= TestTrue(*FString::Printf(TEXT("%s uses the ward hand"), *Label), Right.bHasWard && Left.bHasWard);
				bPass &= TestTrue(*FString::Printf(TEXT("%s raised"), *Label), Left.Raises >= 1 && Left.bWardUp);
			}
			if (FCString::Strcmp(Action, TEXT("blink-left")) == 0)
			{
				bPass &= TestEqual(*FString::Printf(TEXT("%s left pad"), *Label), Left.Pad, 0);
				bPass &= TestEqual(*FString::Printf(TEXT("%s one blink"), *Label), Left.Blinks, 1);
			}
			else if (FCString::Strcmp(Action, TEXT("blink-right")) == 0)
			{
				bPass &= TestEqual(*FString::Printf(TEXT("%s right pad"), *Label), Left.Pad, 2);
			}
			else if (FCString::Strcmp(Action, TEXT("blink-back")) == 0)
			{
				bPass &= TestEqual(*FString::Printf(TEXT("%s centre pad"), *Label), Left.Pad, 1);
			}
			else if (FCString::Strcmp(Action, TEXT("bolt")) == 0)
			{
				bPass &= TestEqual(*FString::Printf(TEXT("%s one bolt"), *Label), Left.Bolts, 1);
			}
			else if (FCString::Strcmp(Action, TEXT("staff-plant")) == 0)
			{
				bPass &= TestEqual(*FString::Printf(TEXT("%s plant"), *Label), Left.Plants, 1);
				bPass &= TestEqual(*FString::Printf(TEXT("%s plant does not lift"), *Label), Left.Lifts, 0);
			}
			else if (FCString::Strcmp(Action, TEXT("both-palms")) == 0)
			{
				bPass &= TestTrue(*FString::Printf(TEXT("%s both palms"), *Label), Right.bPalms && Left.bPalms);
			}
		}
	}

	UGameInstance* Accuracy = NewObject<UGameInstance>(GetTransientPackage());
	Accuracy->AddToRoot();
	USigilRecognizerSubsystem* RecognizerHost = NewObject<USigilRecognizerSubsystem>(Accuracy);
	RecognizerHost->LoadTemplatesFromDirectory(FPaths::Combine(FPaths::ProjectDir(), TEXT("Clips"), TEXT("templates")));
	const FQPointCloudRecognizer& Recognizer = RecognizerHost->GetRecognizer();
	const TCHAR* Lines[] = {TEXT("sigil-line1"), TEXT("sigil-line2"), TEXT("sigil-line3")};
	int32 OriginalCorrect = 0;
	int32 MirrorCorrect = 0;
	int32 Compared = 0;
	for (const TCHAR* Action : Lines)
	{
		for (int32 VariantIndex = 0; VariantIndex < 3; ++VariantIndex)
		{
			FHandClip Original;
			FHandClip Mirror;
			FString Error;
			const FString OriginalPath = FHandClip::MakeFilePath(Action, GVariants[VariantIndex], false);
			const FString MirrorPath = FHandClip::MakeFilePath(Action, GVariants[VariantIndex], true);
			if (!FHandClip::LoadFromFile(OriginalPath, Original, Error) || !FHandClip::LoadFromFile(MirrorPath, Mirror, Error))
			{
				AddError(Error);
				bPass = false;
				continue;
			}
			TArray<FQPoint> OriginalPoints;
			TArray<FQPoint> MirrorPoints;
			const bool bOriginalGesture = ExtractFirstGesture(Original, ESigilDrawStyle::StyleA, OriginalPoints, EControllerHand::Right);
			const bool bMirrorGesture = ExtractFirstGesture(Mirror, ESigilDrawStyle::StyleA, MirrorPoints, EControllerHand::Left);
			bPass &= TestEqual(*FString::Printf(TEXT("%s.%s gesture"), Action, GVariantNames[VariantIndex]), bMirrorGesture, bOriginalGesture);
			FQClassifyResult OriginalResult;
			FQClassifyResult MirrorResult;
			if (bOriginalGesture)
			{
				OriginalResult = Recognizer.Classify(OriginalPoints);
			}
			if (bMirrorGesture)
			{
				MirrorResult = Recognizer.Classify(MirrorPoints);
			}
			const FName Expected(*FString(Action).RightChop(6));
			if (bOriginalGesture && !OriginalResult.bReject && OriginalResult.Label == Expected)
			{
				++OriginalCorrect;
			}
			if (bMirrorGesture && !MirrorResult.bReject && MirrorResult.Label == Expected)
			{
				++MirrorCorrect;
			}
			++Compared;
			bPass &= TestEqual(*FString::Printf(TEXT("%s.%s label"), Action, GVariantNames[VariantIndex]), MirrorResult.Label, OriginalResult.Label);
			bPass &= TestEqual(*FString::Printf(TEXT("%s.%s reject"), Action, GVariantNames[VariantIndex]), MirrorResult.bReject, OriginalResult.bReject);
		}
	}
	bPass &= TestEqual(TEXT("nine sigil clips"), Compared, 9);
	bPass &= TestEqual(TEXT("mirrored sigil accuracy"), MirrorCorrect, OriginalCorrect);
	Accuracy->RemoveFromRoot();
	Accuracy->MarkAsGarbage();

	FMageSettings::SetHand(EMageHand::Left, false);
	Rig.Hands->StopAll();
	Rig.Wards->GetDetector().ResetStream();
	const int32 RaisesBeforeSplit = Rig.Wards->GetRaiseCount();
	Rig.Hands->SetWardHeld(true, EClipVariant::Normal);
	const double Frame = 1.0 / 72.0;
	int32 RaiseSteps = 0;
	while (Rig.Wards->GetRaiseCount() == RaisesBeforeSplit && RaiseSteps < 720)
	{
		Rig.Hands->Step(Frame);
		++RaiseSteps;
	}
	bPass &= TestTrue(TEXT("left-hand mode raises the right ward"), Rig.Wards->GetRaiseCount() > RaisesBeforeSplit && Rig.Wards->GetState().bRaised);
	const int32 Lowers = Rig.Wards->GetLowerCount();
	int32 SplitCasts = 0;
	FName SplitLine = NAME_None;
	Rig.Sigils->OnSigilCast.AddLambda([&SplitCasts, &SplitLine](FName InLine, float, double)
	{
		++SplitCasts;
		SplitLine = InLine;
	});
	Rig.Hands->PlayQuickAction(TEXT("sigil-line2"), EClipVariant::Normal);
	bPass &= TestTrue(TEXT("ward stays held"), Rig.Hands->IsWardHeld());
	bool bUnused = false;
	if (!StepClip(Rig.Hands, bUnused))
	{
		AddError(TEXT("split sigil did not play"));
		Rig.Close();
		return false;
	}
	bPass &= TestTrue(TEXT("ward still held after the sigil"), Rig.Hands->IsWardHeld() && Rig.Wards->GetState().bRaised);
	bPass &= TestEqual(TEXT("sigil did not lower the ward"), Rig.Wards->GetLowerCount(), Lowers);
	bPass &= TestEqual(TEXT("one split cast"), SplitCasts, 1);
	bPass &= TestTrue(TEXT("split line2"), SplitLine == FName(TEXT("line2")));

	for (int32 VariantIndex = 0; VariantIndex < 3; ++VariantIndex)
	{
		Rig.Hands->StopAll();
		Rig.Staff->GetDetector().Reset();
		Rig.Hands->PlayQuickAction(TEXT("staff-plant"), GVariants[VariantIndex]);
		if (!StepClip(Rig.Hands, bUnused))
		{
			AddError(TEXT("staff plant did not play"));
			bPass = false;
			break;
		}
		bPass &= TestEqual(*FString::Printf(TEXT("left %s plant"), GVariantNames[VariantIndex]), Rig.Staff->GetDetector().GetPlantCount(), 1);
		bPass &= TestEqual(*FString::Printf(TEXT("left %s no lift yet"), GVariantNames[VariantIndex]), Rig.Staff->GetDetector().GetLiftCount(), 0);
		Rig.Hands->PlayQuickAction(TEXT("staff-lift"), GVariants[VariantIndex]);
		if (!StepClip(Rig.Hands, bUnused))
		{
			AddError(TEXT("staff lift did not play"));
			bPass = false;
			break;
		}
		bPass &= TestEqual(*FString::Printf(TEXT("left %s lift"), GVariantNames[VariantIndex]), Rig.Staff->GetDetector().GetLiftCount(), 1);
		bPass &= TestEqual(*FString::Printf(TEXT("left %s plant stays"), GVariantNames[VariantIndex]), Rig.Staff->GetDetector().GetPlantCount(), 1);
	}

	Rig.Close();
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaSettingsNarrow, "MageArena.Settings.Narrow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaSettingsNarrow::RunTest(const FString& Parameters)
{
	FSettingsGuard Guard;
	FString Error;
	FVrRuleset Base;
	if (!LoadVrRuleset(Base, Error))
	{
		AddError(Error);
		return false;
	}
	bool bPass = TestFalse(TEXT("loaded flags stay off"), Base.bNarrow || Base.bGentle);
	bPass &= TestTrue(TEXT("overlay arc"), Base.Narrow.SpawnArcDeg == 40.0);
	FVrRuleset Alt = Base;
	Alt.Narrow.SpawnArcDeg = 10.0;
	Alt.Gentle.TelegraphMult = 7.0;
	Alt.Gentle.Competence = 0.0;
	FGames IdleA;
	FGames IdleB;
	if (!TryCreateGames(IdleA, 1, nullptr, 0, false, false, &Base) || !TryCreateGames(IdleB, 1, nullptr, 0, false, false, &Alt))
	{
		AddError(TEXT("idle games did not start"));
		return false;
	}
	for (int32 Step = 0; Step < 300; ++Step)
	{
		StepGames(IdleA);
		StepGames(IdleB);
	}
	bPass &= TestEqual(TEXT("off path state hash"), StateHash(IdleA.State), StateHash(IdleB.State));

	FMageSettings::SetHand(EMageHand::Right, false);
	FMageSettings::SetGentle(false, false);
	FMageSettings::SetFov(EMageFovMode::Narrow, false);
	FSessionRig Rig;
	if (!Rig.Open(*this))
	{
		Rig.Close();
		return false;
	}
	auto RunBout = [&](int32 Wave, bool bFire, const TCHAR* Label) -> bool
	{
		FArenaSession Session;
		Session.Bind(Rig.Hands, Rig.Sigils, Rig.Wards, Rig.Blinks, Rig.Staff);
		Session.SetScripted(true);
		Session.SetBout(Wave, bFire);
		if (!Session.Start(1))
		{
			AddError(FString::Printf(TEXT("%s start failed"), Label));
			Session.Unbind();
			return false;
		}
		const FVrRuleset* Rules = Session.GetVrRules();
		if (!Rules || !Rules->bNarrow)
		{
			AddError(FString::Printf(TEXT("%s narrow flag"), Label));
			Session.Unbind();
			return false;
		}
		const double Arc = Rules->Narrow.SpawnArcDeg;
		TSet<int32> SeenTelegraphs;
		TSet<int32> SeenAims;
		int32 SeenSpawns = 0;
		int32 SpawnChecks = 0;
		int32 OriginChecks = 0;
		int32 HoldChecks = 0;
		int32 WalkBacks = 0;
		TMap<int32, double> OutOfArc;
		bool bBout = true;
		const double Dt = 1.0 / 72.0;
		int32 GuardCount = 0;
		while (Session.GetGames().Phase == TEXT("active") && Session.GetSimSeconds() < 120.0 && GuardCount < 14000)
		{
			const int32 Pad = Session.GetActivePad();
			const int32 TickBefore = Session.GetGames().State.Tick;
			Session.Advance(Dt, true);
			++GuardCount;
			// 72 Hz sampling against a 60 Hz sim. A frame that does not step still shows the previous pad's arc,
			// and a blink updates the pad only after that step's compress. Judge the step that just ran.
			if (Session.GetGames().State.Tick == TickBefore)
			{
				continue;
			}
			const TArray<FSpawnRecord>& Spawns = Session.GetGames().SpawnLog;
			for (int32 Index = SeenSpawns; Index < Spawns.Num(); ++Index)
			{
				FString Why;
				const FSimVec At{Spawns[Index].X, Spawns[Index].Y};
				if (!InArc(Session, Pad, At, Arc, Why))
				{
					AddError(FString::Printf(TEXT("%s spawn %s"), Label, *Why));
					bBout = false;
				}
				++SpawnChecks;
			}
			SeenSpawns = Spawns.Num();
			for (const FActor& Actor : Session.GetGames().State.Actors)
			{
				if (Actor.Team == 0 || Actor.bDown)
				{
					continue;
				}
				FString Why;
				// Within 0.1 m of the arc counts as inside: a holder parked on the edge jitters by millimetres as its hold
				// walk and the arc walk-back meet. Anything farther out must be walking back.
				const double EdgeOut = SimLength(SimSub(Rules->CompressToArc(Actor.Pos), Actor.Pos));
				if (InArc(Session, Pad, Actor.Pos, Arc, Why) || EdgeOut < 0.1)
				{
					OutOfArc.Remove(Actor.Id);
				}
				else
				{
					// Outside the arc is legal only while walking back: after a blink (or a knockback) an opponent may
					// be out for some ticks, but its distance to the arc must shrink every step - never a snap, never a drift.
					const double Out = SimLength(SimSub(Rules->CompressToArc(Actor.Pos), Actor.Pos));
					if (const double* Prev = OutOfArc.Find(Actor.Id))
					{
						if (!(Out < *Prev - 1.0e-9))
						{
							AddError(FString::Printf(TEXT("%s hold %s not walking back (%.4f m after %.4f m) %s"), Label, *Actor.Label, Out, *Prev, *Why));
							bBout = false;
						}
					}
					OutOfArc.Add(Actor.Id, Out);
					++WalkBacks;
				}
				++HoldChecks;
			}
			for (const FTelegraph& Telegraph : Session.GetGames().State.Telegraphs)
			{
				if (SeenTelegraphs.Contains(Telegraph.Id))
				{
					continue;
				}
				const FActor* Owner = SimFindActor(Session.GetGames().State, Telegraph.OwnerId);
				if (!Owner || Owner->Team == 0)
				{
					SeenTelegraphs.Add(Telegraph.Id);
					continue;
				}
				FString Why;
				if (!InArc(Session, Pad, Telegraph.Origin, Arc, Why))
				{
					AddError(FString::Printf(TEXT("%s origin %s"), Label, *Why));
					bBout = false;
				}
				if (Telegraph.Kind == TEXT("area") && !InArc(Session, Pad, Telegraph.Target, Arc, Why))
				{
					AddError(FString::Printf(TEXT("%s target %s"), Label, *Why));
					bBout = false;
				}
				SeenTelegraphs.Add(Telegraph.Id);
				++OriginChecks;
			}
			for (const FActor& Actor : Session.GetGames().State.Actors)
			{
				if (Actor.Team == 0 || !Actor.Pending.IsSet() || Actor.Pending->Kind != TEXT("fire") || !Actor.Pending->SpellId.IsSet())
				{
					continue;
				}
				if (SeenAims.Contains(Actor.Pending->ActivationId))
				{
					continue;
				}
				const FFireSpell* Spell = FindFireSpell(Actor.Pending->SpellId.GetValue());
				if (!Spell || (Spell->Kind != TEXT("zone") && Spell->Kind != TEXT("meteor")))
				{
					continue;
				}
				SeenAims.Add(Actor.Pending->ActivationId);
				FString Why;
				if (!InArc(Session, Pad, Actor.Pending->Aim, Arc, Why))
				{
					AddError(FString::Printf(TEXT("%s aim %s"), Label, *Why));
					bBout = false;
				}
				++OriginChecks;
			}
		}
		bBout &= TestTrue(*FString::Printf(TEXT("%s spawns"), Label), SpawnChecks > 0);
		bBout &= TestTrue(*FString::Printf(TEXT("%s holds"), Label), HoldChecks > 0);
		if (!bFire)
		{
			bBout &= TestTrue(*FString::Printf(TEXT("%s origins"), Label), OriginChecks > 0);
		}
		UE_LOG(LogMageArena, Log, TEXT("Narrow %s phase=%s t=%.2f spawns=%d origins=%d walkbacks=%d"),
			Label, *Session.GetGames().Phase, Session.GetSimSeconds(), SpawnChecks, OriginChecks, WalkBacks);
		Session.Unbind();
		return bBout;
	};
	bPass &= RunBout(0, false, TEXT("wave1"));
	bPass &= RunBout(2, true, TEXT("fire-c1"));
	bPass &= RunBout(3, true, TEXT("fire-c15"));

	// A blink mid-wave moves the narrow arc. Each tick of KeepInView may move
	// an opponent at most its walk speed / 60 Hz toward the new arc, never snap it, and everyone ends inside.
	{
		FArenaSession Session;
		Session.Bind(Rig.Hands, Rig.Sigils, Rig.Wards, Rig.Blinks, Rig.Staff);
		Session.SetScripted(true);
		Session.SetBout(0, false);
		if (!Session.Start(1))
		{
			AddError(TEXT("blink walk start failed"));
			Session.Unbind();
			Rig.Close();
			return false;
		}
		for (int32 Frame = 0; Frame < 240; ++Frame)
		{
			Session.Advance(1.0 / 60.0, true);
		}
		FArenaState State = Session.GetGames().State;
		FVrRuleset Moved = *Session.GetVrRules();
		// A blink moves the arc. Rotate the current arc 100 degrees about the active pad: every opponent that was
		// in front of the player is now outside it, which is the worst case a pad change can produce.
		const double Turn = 100.0 * (PI / 180.0);
		const FSimVec F0 = Moved.NarrowForward;
		Moved.NarrowForward = FSimVec{F0.X * std::cos(Turn) - F0.Y * std::sin(Turn), F0.X * std::sin(Turn) + F0.Y * std::cos(Turn)};
		int32 Outside = 0;
		for (const FActor& Actor : State.Actors)
		{
			FString Why;
			if (Actor.Team != 0 && !Actor.bDown && SimLength(SimSub(Moved.CompressToArc(Actor.Pos), Actor.Pos)) > 1.0e-6)
			{
				++Outside;
			}
		}
		bPass &= TestTrue(TEXT("blink leaves opponents outside the new arc"), Outside > 0);
		bool bWalk = true;
		for (int32 Tick = 0; Tick < 1200 && bWalk; ++Tick)
		{
			TMap<int32, FSimVec> Before;
			for (const FActor& Actor : State.Actors)
			{
				Before.Add(Actor.Id, Actor.Pos);
			}
			Moved.KeepInView(State);
			for (const FActor& Actor : State.Actors)
			{
				if (Actor.Team == 0 || Actor.bDown)
				{
					continue;
				}
				// Walk speed bound: the pinned speed (or the walk default) over one 60 Hz tick.
				const double Cap = Actor.SpeedMps.Get(KernelData().WalkMps) / 60.0 + 1.0e-9;
				const double Step = SimLength(SimSub(Actor.Pos, Before[Actor.Id]));
				if (Step > Cap)
				{
					AddError(FString::Printf(TEXT("blink walk %s moved %.4f m in one tick, cap %.4f"), *Actor.Label, Step, Cap));
					bWalk = false;
				}
			}
			// The kernel runs KeepOut after KeepInView (Games.cpp); the walk back must never end inside the dais hold.
			Moved.KeepOut(State);
			for (const FActor& Actor : State.Actors)
			{
				if (Actor.Team != 0 && !Actor.bDown && Moved.InsideHold(Actor.Pos))
				{
					AddError(FString::Printf(TEXT("blink walk %s ended inside the dais hold"), *Actor.Label));
					bWalk = false;
				}
			}
		}
		bPass &= bWalk;
		int32 StillOut = 0;
		for (const FActor& Actor : State.Actors)
		{
			FString Why;
			if (Actor.Team != 0 && !Actor.bDown && SimLength(SimSub(Moved.CompressToArc(Actor.Pos), Actor.Pos)) > 1.0e-6)
			{
				++StillOut;
			}
		}
		bPass &= TestEqual(TEXT("everyone walked into the new arc"), StillOut, 0);
		UE_LOG(LogMageArena, Log, TEXT("Narrow blink-walk outside=%d stillOut=%d"), Outside, StillOut);
		Session.Unbind();
	}
	Rig.Close();
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaSettingsGentle, "MageArena.Settings.Gentle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaSettingsGentle::RunTest(const FString& Parameters)
{
	FSettingsGuard Guard;
	FMageSettings::SetHand(EMageHand::Right, false);
	FMageSettings::SetFov(EMageFovMode::Wide, false);
	FSessionRig Rig;
	if (!Rig.Open(*this))
	{
		Rig.Close();
		return false;
	}

	auto RunBout = [&](int32 Wave, bool bFire, bool bGentle, const TCHAR* Label) -> bool
	{
		FMageSettings::SetGentle(bGentle, false);
		FArenaSession Session;
		Session.Bind(Rig.Hands, Rig.Sigils, Rig.Wards, Rig.Blinks, Rig.Staff);
		Session.SetScripted(true);
		Session.SetBout(Wave, bFire);
		if (!Session.Start(1))
		{
			AddError(FString::Printf(TEXT("%s start failed"), Label));
			Session.Unbind();
			return false;
		}
		const FVrRuleset* Rules = Session.GetVrRules();
		if (!Rules || Rules->bGentle != bGentle)
		{
			AddError(FString::Printf(TEXT("%s gentle flag"), Label));
			Session.Unbind();
			return false;
		}
		// combat.vr.json gentle.telegraphMult is 2 whether or not Gentle is on; off means the flag is off and every
		// windup below is checked at its single-speed tick count.
		const double Mult = bGentle ? Rules->Gentle.TelegraphMult : 1.0;
		bool bBout = TestTrue(*FString::Printf(TEXT("%s mult"), Label), Rules->Gentle.TelegraphMult == 2.0 && Rules->bGentle == bGentle);
		bBout &= TestTrue(*FString::Printf(TEXT("%s competence field"), Label), Rules->Gentle.Competence == 1.0);
		int32 Opponents = 0;
		int32 NotGentle = 0;
		int32 AttackChecks = 0;
		int32 MeteorChecks = 0;
		int32 PendingChecks = 0;
		int32 Team0Bad = 0;
		for (const FActor& Actor : Session.GetGames().State.Actors)
		{
			if (Actor.Team == 0 || !Actor.MageAI.IsSet())
			{
				continue;
			}
			++Opponents;
			const bool bAtGentle = std::abs(Actor.MageAI->Competence - Rules->Gentle.Competence) <= 1.0e-6;
			if (bGentle && !bAtGentle)
			{
				AddError(FString::Printf(TEXT("%s competence %.3f"), Label, Actor.MageAI->Competence));
				bBout = false;
			}
			if (!bGentle && !bAtGentle)
			{
				++NotGentle;
			}
		}
		TSet<int32> Seen;
		const double Dt = 1.0 / 72.0;
		int32 GuardCount = 0;
		while (Session.GetGames().Phase == TEXT("active") && Session.GetSimSeconds() < 120.0 && GuardCount < 10000)
		{
			Session.Advance(Dt, true);
			++GuardCount;
			for (const FActor& Actor : Session.GetGames().State.Actors)
			{
				if (Actor.Team == 0)
				{
					if (!UnmultipliedPending(Actor))
					{
						++Team0Bad;
					}
					continue;
				}
				if (bGentle && Actor.MageAI.IsSet() && std::abs(Actor.MageAI->Competence - Rules->Gentle.Competence) > 1.0e-6)
				{
					AddError(FString::Printf(TEXT("%s competence drifted"), Label));
					bBout = false;
				}
				if (FirePending(Actor, Mult))
				{
					++PendingChecks;
				}
				else if (Actor.Pending.IsSet() && (Actor.Pending->Kind == TEXT("fire") || Actor.Pending->Kind == TEXT("air")))
				{
					AddError(FString::Printf(TEXT("%s school windup span %d"), Label,
						Actor.Pending->ReleaseTick - Actor.Pending->StartTick));
					bBout = false;
				}
			}
			for (const FTelegraph& Telegraph : Session.GetGames().State.Telegraphs)
			{
				if (Seen.Contains(Telegraph.Id))
				{
					continue;
				}
				Seen.Add(Telegraph.Id);
				const FActor* Owner = SimFindActor(Session.GetGames().State, Telegraph.OwnerId);
				if (!Owner)
				{
					continue;
				}
				const int32 Span = Telegraph.ResolveTick - Telegraph.StartTick;
				if (Owner->Enemy.IsSet() && !Telegraph.bSurvivesOwner)
				{
					if (!EnemyWindup(*Owner, Span, Mult))
					{
						AddError(FString::Printf(TEXT("%s enemy span %d"), Label, Span));
						bBout = false;
					}
					++AttackChecks;
				}
				else if (Owner->MageAI.IsSet() && Telegraph.bSurvivesOwner && Telegraph.Kind == TEXT("area"))
				{
					if (!SpellTelegraph(Span, Mult))
					{
						AddError(FString::Printf(TEXT("%s meteor span %d"), Label, Span));
						bBout = false;
					}
					++MeteorChecks;
				}
			}
			if (!bGentle && (AttackChecks > 0 || MeteorChecks > 0 || PendingChecks > 0) && NotGentle > 0)
			{
				break;
			}
		}
		if (bGentle && bFire)
		{
			bBout &= TestTrue(*FString::Printf(TEXT("%s opponents"), Label), Opponents > 0);
			bBout &= TestTrue(*FString::Printf(TEXT("%s fire cast"), Label), PendingChecks > 0 || MeteorChecks > 0);
		}
		if (bGentle && !bFire)
		{
			bBout &= TestTrue(*FString::Printf(TEXT("%s enemy attacks"), Label), AttackChecks > 0);
		}
		if (!bGentle)
		{
			bBout &= TestTrue(*FString::Printf(TEXT("%s not competence 1"), Label), NotGentle > 0);
			bBout &= TestTrue(*FString::Printf(TEXT("%s raw telegraph"), Label), AttackChecks > 0 || MeteorChecks > 0 || PendingChecks > 0);
		}
		UE_LOG(LogMageArena, Log, TEXT("Gentle %s phase=%s t=%.2f attacks=%d meteors=%d pending=%d opponents=%d"),
			Label, *Session.GetGames().Phase, Session.GetSimSeconds(), AttackChecks, MeteorChecks, PendingChecks, Opponents);
		Session.Unbind();
		return bBout;
	};

	bool bPass = RunBout(0, false, true, TEXT("wave1-on"));
	bPass &= RunBout(2, true, true, TEXT("fire-on"));
	// T23: the school final is Lio's air mage now, so this bout checks the air windups raw.
	bPass &= RunBout(3, true, false, TEXT("air-final-off"));
	Rig.Close();
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaSettingsOverlayLog, "MageArena.Settings.OverlayLog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaSettingsOverlayLog::RunTest(const FString& Parameters)
{
	// CacheOverlay used to drop a failed overlay load without a word, so a player on the built-in numbers had nothing in the
	// log to say why their field of view or spawn arc was not the tuned one.
	const FString EmptyDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("OverlayLog-") + FGuid::NewGuid().ToString());
	IFileManager::Get().MakeDirectory(*EmptyDir, true);
	const float NarrowBefore = FMageSettings::NarrowCameraFovDeg();
	const float OfferBefore = FMageSettings::OfferBelowDeg();
	const float ArcBefore = FMageSettings::SpawnArcDeg();

	AddExpectedMessage(TEXT("Settings overlay not loaded"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	SetVrDataDirForTest(EmptyDir);
	FMageSettings::ReloadOverlayForTest();
	SetVrDataDirForTest(FString());

	bool bPass = TestEqual(TEXT("narrow camera keeps its last good value"), FMageSettings::NarrowCameraFovDeg(), NarrowBefore);
	bPass &= TestEqual(TEXT("offer threshold keeps its last good value"), FMageSettings::OfferBelowDeg(), OfferBefore);
	bPass &= TestEqual(TEXT("spawn arc keeps its last good value"), FMageSettings::SpawnArcDeg(), ArcBefore);

	FMageSettings::ReloadOverlayForTest();
	bPass &= TestEqual(TEXT("good overlay reads back the same camera"), FMageSettings::NarrowCameraFovDeg(), NarrowBefore);
	IFileManager::Get().DeleteDirectory(*EmptyDir, false, true);
	return bPass;
}

#endif
