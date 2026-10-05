#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Hands/MageArenaPlayerController.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
// Every -MageArena...Capture token in a script, without the dash.
void CollectCaptureFlags(const FString& Text, TSet<FString>& Out)
{
	const FString Marker = TEXT("-MageArena");
	int32 From = 0;
	while (true)
	{
		const int32 At = Text.Find(Marker, ESearchCase::CaseSensitive, ESearchDir::FromStart, From);
		if (At == INDEX_NONE)
		{
			return;
		}
		int32 End = At + 1;
		while (End < Text.Len() && FChar::IsAlnum(Text[End]))
		{
			++End;
		}
		const FString Flag = Text.Mid(At + 1, End - At - 1);
		if (Flag.EndsWith(TEXT("Capture")))
		{
			Out.Add(Flag);
		}
		From = End;
	}
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaCaptureFlagsStandDownMouseLook, "MageArena.Input.CaptureFlags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaCaptureFlagsStandDownMouseLook::RunTest(const FString& Parameters)
{
	// The capture scripts are the list of runs that start a scripted driver. Mouse look must stand down for every one,
	// or a mouse moved during the run drags the shots. The expected set is read from the scripts, not written here.
	const FString ToolsDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../tools")));
	TArray<FString> Scripts;
	IFileManager::Get().FindFiles(Scripts, *FPaths::Combine(ToolsDir, TEXT("capture-*.ps1")), true, false);
	TSet<FString> Flags;
	for (const FString& Script : Scripts)
	{
		FString Text;
		if (FFileHelper::LoadFileToString(Text, *FPaths::Combine(ToolsDir, Script)))
		{
			CollectCaptureFlags(Text, Flags);
		}
	}
	if (!TestTrue(*FString::Printf(TEXT("found the capture flags in %d scripts (%d flags)"), Scripts.Num(), Flags.Num()), Flags.Num() >= 5))
	{
		return false;
	}
	for (const FString& Flag : Flags)
	{
		TestTrue(*FString::Printf(TEXT("-%s stands mouse look down"), *Flag), IsCaptureRun(*(TEXT("-") + Flag)));
	}
	TestFalse(TEXT("an ordinary launch keeps mouse look"), IsCaptureRun(TEXT("-nullrhi -game")));
	TestFalse(TEXT("an empty command line keeps mouse look"), IsCaptureRun(TEXT("")));
	return true;
}

#endif
