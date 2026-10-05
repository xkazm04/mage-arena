#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

// Player comfort. Defaults are the live bout: right hand, wide view, gentle off.
// The file is Saved/MageArena/settings.json. A missing or corrupt file keeps the defaults.
// Command-line overrides (-MageArenaHand=, -MageArenaFov=, -MageArenaGentle=) do not write the file.

enum class EMageHand : uint8
{
	Right,
	Left
};

enum class EMageFovMode : uint8
{
	Wide,
	Narrow
};

struct FMageSettingsState
{
	EMageHand Hand = EMageHand::Right;
	EMageFovMode Fov = EMageFovMode::Wide;
	bool bGentle = false;
};

class MAGEARENAVR_API FMageSettings
{
public:
	static void EnsureLoaded();
	static void Reload();
	static FMageSettingsState Get();
	// Memory only. Tests use this to put the process back.
	static void Restore(const FMageSettingsState& State);
	static void SetHand(EMageHand Hand, bool bPersist);
	static void SetFov(EMageFovMode Fov, bool bPersist);
	static void SetGentle(bool bOn, bool bPersist);
	static void ToggleHand();
	static void ToggleFov();
	static void ToggleGentle();

	static bool IsLeftHanded();
	static bool IsNarrow();
	static bool IsGentle();
	static EControllerHand CastingHand();
	static EControllerHand WardHand();

	// Overlay numbers. Camera fov is applied only while narrow is on.
	static float NarrowCameraFovDeg();
	static float OfferBelowDeg();
	static float SpawnArcDeg();
	static float DeviceFovDeg();
	// 0 clears the test override and uses the layout (or -MageArenaDeviceFov=).
	static void SetDeviceFovForTest(float FovDeg);
	static bool ShouldOfferNarrow();
	// Re-reads the overlay numbers (and logs why it could not).
	static void ReloadOverlayForTest();

	static void SetFileOverride(const FString& Path);
	static FString FileOverride();
	static FString FilePath();

private:
	static void Save();
	static void ApplyCommandLine();
	static void CacheOverlay();
};
