#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// T55: no debug text in any captured or shipped view. The session sources must not post an on-screen debug message
// (UEngine keeps them private, so the source is the check), the capture driver makes no text component, and the capture
// script launches with engine screen messages disabled.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaNoDebugTextInSources, "MageArena.Presentation.NoDebugText.Sources",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaNoDebugTextInSources::RunTest(const FString& Parameters)
{
	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/MageArenaVR/Session")));
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *Root, TEXT("*.cpp"), true, false);
	if (!TestTrue(*FString::Printf(TEXT("found the session sources (%d)"), Files.Num()), Files.Num() > 5))
	{
		return false;
	}
	for (const FString& File : Files)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *File))
		{
			continue;
		}
		const FString Name = FPaths::GetCleanFilename(File);
		TestFalse(*FString::Printf(TEXT("%s adds no on-screen debug message"), *Name), Text.Contains(TEXT("AddOnScreenDebugMessage")));
		TestFalse(*FString::Printf(TEXT("%s draws no debug string"), *Name), Text.Contains(TEXT("DrawDebugString")));
		// The A08 cuff HUD is the one text component, owned by SessionPresentation. The capture driver makes none.
		if (Name == TEXT("StylePickCapture.cpp"))
		{
			TestFalse(TEXT("the capture driver makes no text component"), Text.Contains(TEXT("UTextRenderComponent")));
		}
	}

	const FString Script = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../tools/capture-style-pick.ps1")));
	FString ScriptText;
	if (TestTrue(TEXT("the style-pick capture script loads"), FFileHelper::LoadFileToString(ScriptText, *Script)))
	{
		TestTrue(TEXT("the capture launches with screen messages disabled"), ScriptText.Contains(TEXT("DisableAllScreenMessages")));
	}
	return true;
}

#endif
