#include "Hands/MageSettings.h"

#include "Greybox/ArenaLayout.h"
#include "Kernel/VrRules.h"
#include "MageArenaVR.h"

#include "Dom/JsonObject.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
struct FStore
{
	FMageSettingsState State;
	bool bLoaded = false;
	bool bCommandLock = false;
	FString FileOverride;
	float NarrowCamera = 70.f;
	float OfferBelow = 90.f;
	float SpawnArc = 40.f;
	float LayoutFov = 90.f;
	float DeviceOverride = 0.f;
};

FStore& Store()
{
	static FStore Value;
	return Value;
}

bool ParseHand(const FString& Text, EMageHand& Out)
{
	if (Text.Equals(TEXT("left"), ESearchCase::IgnoreCase))
	{
		Out = EMageHand::Left;
		return true;
	}
	if (Text.Equals(TEXT("right"), ESearchCase::IgnoreCase))
	{
		Out = EMageHand::Right;
		return true;
	}
	return false;
}

bool ParseFov(const FString& Text, EMageFovMode& Out)
{
	if (Text.Equals(TEXT("narrow"), ESearchCase::IgnoreCase))
	{
		Out = EMageFovMode::Narrow;
		return true;
	}
	if (Text.Equals(TEXT("wide"), ESearchCase::IgnoreCase))
	{
		Out = EMageFovMode::Wide;
		return true;
	}
	return false;
}

bool ParseGentle(const FString& Text, bool& Out)
{
	if (Text.Equals(TEXT("on"), ESearchCase::IgnoreCase) || Text.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Text == TEXT("1"))
	{
		Out = true;
		return true;
	}
	if (Text.Equals(TEXT("off"), ESearchCase::IgnoreCase) || Text.Equals(TEXT("false"), ESearchCase::IgnoreCase) || Text == TEXT("0"))
	{
		Out = false;
		return true;
	}
	return false;
}

void LogState(const TCHAR* Why)
{
	const FMageSettingsState& State = Store().State;
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_SETTINGS %s hand=%s fov=%s gentle=%d"),
		Why,
		State.Hand == EMageHand::Left ? TEXT("left") : TEXT("right"),
		State.Fov == EMageFovMode::Narrow ? TEXT("narrow") : TEXT("wide"),
		State.bGentle ? 1 : 0);
}
}

void FMageSettings::EnsureLoaded()
{
	if (!Store().bLoaded)
	{
		Reload();
	}
}

void FMageSettings::Reload()
{
	FStore& Slot = Store();
	Slot.State = FMageSettingsState();
	Slot.bCommandLock = false;
	const FString Path = FilePath();
	FString Text;
	if (FPaths::FileExists(Path))
	{
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			UE_LOG(LogMageArena, Warning, TEXT("Settings file unreadable, using defaults: %s"), *Path);
		}
		else
		{
			TSharedPtr<FJsonObject> Root;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
			if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
			{
				UE_LOG(LogMageArena, Warning, TEXT("Settings file corrupt, using defaults: %s"), *Path);
			}
			else
			{
				FString Hand;
				FString Fov;
				bool bGentle = false;
				EMageHand ParsedHand = EMageHand::Right;
				EMageFovMode ParsedFov = EMageFovMode::Wide;
				if (!Root->TryGetStringField(TEXT("hand"), Hand) || !ParseHand(Hand, ParsedHand)
					|| !Root->TryGetStringField(TEXT("fovMode"), Fov) || !ParseFov(Fov, ParsedFov)
					|| !Root->TryGetBoolField(TEXT("gentle"), bGentle))
				{
					UE_LOG(LogMageArena, Warning, TEXT("Settings file missing a field, using defaults: %s"), *Path);
				}
				else
				{
					Slot.State.Hand = ParsedHand;
					Slot.State.Fov = ParsedFov;
					Slot.State.bGentle = bGentle;
				}
			}
		}
	}
	CacheOverlay();
	ApplyCommandLine();
	Slot.bLoaded = true;
	LogState(TEXT("load"));
}

FMageSettingsState FMageSettings::Get()
{
	EnsureLoaded();
	return Store().State;
}

void FMageSettings::Restore(const FMageSettingsState& State)
{
	Store().State = State;
	Store().bLoaded = true;
	LogState(TEXT("restore"));
}

void FMageSettings::SetHand(EMageHand Hand, bool bPersist)
{
	EnsureLoaded();
	Store().State.Hand = Hand;
	if (bPersist)
	{
		Save();
	}
	LogState(TEXT("hand"));
}

void FMageSettings::SetFov(EMageFovMode Fov, bool bPersist)
{
	EnsureLoaded();
	Store().State.Fov = Fov;
	if (bPersist)
	{
		Save();
	}
	LogState(TEXT("fov"));
}

void FMageSettings::SetGentle(bool bOn, bool bPersist)
{
	EnsureLoaded();
	Store().State.bGentle = bOn;
	if (bPersist)
	{
		Save();
	}
	LogState(TEXT("gentle"));
}

void FMageSettings::ToggleHand()
{
	SetHand(IsLeftHanded() ? EMageHand::Right : EMageHand::Left, true);
}

void FMageSettings::ToggleFov()
{
	SetFov(IsNarrow() ? EMageFovMode::Wide : EMageFovMode::Narrow, true);
}

void FMageSettings::ToggleGentle()
{
	SetGentle(!IsGentle(), true);
}

bool FMageSettings::IsLeftHanded()
{
	EnsureLoaded();
	return Store().State.Hand == EMageHand::Left;
}

bool FMageSettings::IsNarrow()
{
	EnsureLoaded();
	return Store().State.Fov == EMageFovMode::Narrow;
}

bool FMageSettings::IsGentle()
{
	EnsureLoaded();
	return Store().State.bGentle;
}

EControllerHand FMageSettings::CastingHand()
{
	return IsLeftHanded() ? EControllerHand::Left : EControllerHand::Right;
}

EControllerHand FMageSettings::WardHand()
{
	return IsLeftHanded() ? EControllerHand::Right : EControllerHand::Left;
}

float FMageSettings::NarrowCameraFovDeg()
{
	EnsureLoaded();
	return Store().NarrowCamera;
}

float FMageSettings::OfferBelowDeg()
{
	EnsureLoaded();
	return Store().OfferBelow;
}

float FMageSettings::SpawnArcDeg()
{
	EnsureLoaded();
	return Store().SpawnArc;
}

float FMageSettings::DeviceFovDeg()
{
	EnsureLoaded();
	if (Store().DeviceOverride > 0.f)
	{
		return Store().DeviceOverride;
	}
	return Store().LayoutFov;
}

void FMageSettings::SetDeviceFovForTest(float FovDeg)
{
	Store().DeviceOverride = FovDeg;
}

void FMageSettings::ReloadOverlayForTest()
{
	CacheOverlay();
}

bool FMageSettings::ShouldOfferNarrow()
{
	return DeviceFovDeg() < OfferBelowDeg() && !IsNarrow();
}

void FMageSettings::SetFileOverride(const FString& Path)
{
	Store().FileOverride = Path;
}

FString FMageSettings::FileOverride()
{
	return Store().FileOverride;
}

FString FMageSettings::FilePath()
{
	if (!Store().FileOverride.IsEmpty())
	{
		return Store().FileOverride;
	}
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MageArena"), TEXT("settings.json"));
}

void FMageSettings::Save()
{
	if (Store().bCommandLock)
	{
		UE_LOG(LogMageArena, Log, TEXT("MAGEVR_SETTINGS save skipped (command line)"));
		return;
	}
	const FMageSettingsState& State = Store().State;
	const FString Body = FString::Printf(TEXT("{\"hand\":\"%s\",\"fovMode\":\"%s\",\"gentle\":%s}\n"),
		State.Hand == EMageHand::Left ? TEXT("left") : TEXT("right"),
		State.Fov == EMageFovMode::Narrow ? TEXT("narrow") : TEXT("wide"),
		State.bGentle ? TEXT("true") : TEXT("false"));
	const FString Path = FilePath();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	if (!FFileHelper::SaveStringToFile(Body, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogMageArena, Warning, TEXT("Settings file was not written: %s"), *Path);
	}
}

void FMageSettings::ApplyCommandLine()
{
	FString Hand;
	FString Fov;
	FString Gentle;
	bool bLocked = false;
	if (FParse::Value(FCommandLine::Get(), TEXT("MageArenaHand="), Hand))
	{
		EMageHand Parsed = EMageHand::Right;
		if (ParseHand(Hand, Parsed))
		{
			Store().State.Hand = Parsed;
			bLocked = true;
		}
	}
	if (FParse::Value(FCommandLine::Get(), TEXT("MageArenaFov="), Fov))
	{
		EMageFovMode Parsed = EMageFovMode::Wide;
		if (ParseFov(Fov, Parsed))
		{
			Store().State.Fov = Parsed;
			bLocked = true;
		}
	}
	if (FParse::Value(FCommandLine::Get(), TEXT("MageArenaGentle="), Gentle))
	{
		bool bOn = false;
		if (ParseGentle(Gentle, bOn))
		{
			Store().State.bGentle = bOn;
			bLocked = true;
		}
	}
	float Device = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("MageArenaDeviceFov="), Device) && Device > 0.f)
	{
		Store().DeviceOverride = Device;
	}
	Store().bCommandLock = bLocked;
}

void FMageSettings::CacheOverlay()
{
	FVrRuleset Rules;
	FString Error;
	if (LoadVrRuleset(Rules, Error, false))
	{
		Store().NarrowCamera = static_cast<float>(Rules.Narrow.CameraFovDeg);
		Store().OfferBelow = static_cast<float>(Rules.Narrow.OfferBelowDeg);
		Store().SpawnArc = static_cast<float>(Rules.Narrow.SpawnArcDeg);
	}
	else
	{
		UE_LOG(LogMageArena, Warning, TEXT("Settings overlay not loaded, keeping the built-in narrow-mode numbers: %s"), *Error);
	}
	FArenaLayout Layout;
	FString LayoutError;
	if (!Layout.LoadFromFile(FArenaLayout::DefaultFilePath(), LayoutError))
	{
		UE_LOG(LogMageArena, Warning, TEXT("Settings overlay not loaded, keeping the built-in camera fov: %s"), *LayoutError);
	}
	else if (Layout.CameraFovDeg > 0.0)
	{
		Store().LayoutFov = static_cast<float>(Layout.CameraFovDeg);
	}
}

namespace
{
void SettingsHandCommand(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogMageArena, Error, TEXT("MageArena.Settings.Hand left|right"));
		return;
	}
	EMageHand Hand = EMageHand::Right;
	if (!ParseHand(Args[0], Hand))
	{
		UE_LOG(LogMageArena, Error, TEXT("MageArena.Settings.Hand expected left or right"));
		return;
	}
	FMageSettings::SetHand(Hand, true);
}

void SettingsFovCommand(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogMageArena, Error, TEXT("MageArena.Settings.Fov wide|narrow"));
		return;
	}
	EMageFovMode Fov = EMageFovMode::Wide;
	if (!ParseFov(Args[0], Fov))
	{
		UE_LOG(LogMageArena, Error, TEXT("MageArena.Settings.Fov expected wide or narrow"));
		return;
	}
	FMageSettings::SetFov(Fov, true);
}

void SettingsGentleCommand(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogMageArena, Error, TEXT("MageArena.Settings.Gentle on|off"));
		return;
	}
	bool bOn = false;
	if (!ParseGentle(Args[0], bOn))
	{
		UE_LOG(LogMageArena, Error, TEXT("MageArena.Settings.Gentle expected on, off, true, or false"));
		return;
	}
	FMageSettings::SetGentle(bOn, true);
}

FAutoConsoleCommand GSettingsHand(
	TEXT("MageArena.Settings.Hand"),
	TEXT("Casting hand. left or right. Writes Saved/MageArena/settings.json unless a command-line override is set."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&SettingsHandCommand));

FAutoConsoleCommand GSettingsFov(
	TEXT("MageArena.Settings.Fov"),
	TEXT("View mode. wide or narrow. Writes the settings file unless a command-line override is set."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&SettingsFovCommand));

FAutoConsoleCommand GSettingsGentle(
	TEXT("MageArena.Settings.Gentle"),
	TEXT("Gentle opponents. on or off. Writes the settings file unless a command-line override is set."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&SettingsGentleCommand));
}
