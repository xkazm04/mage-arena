#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Look/ArenaLook.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaLookParse, "MageArena.Look.Parse",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaLookParse::RunTest(const FString& Parameters)
{
	EArenaLook Look = EArenaLook::Epic;
	TestTrue(TEXT("flat parses"), ArenaLook::Parse(TEXT("flat"), Look));
	TestTrue(TEXT("flat is Flat"), Look == EArenaLook::Flat);
	TestTrue(TEXT("quest parses, any case"), ArenaLook::Parse(TEXT(" Quest "), Look));
	TestTrue(TEXT("quest is Quest"), Look == EArenaLook::Quest);
	TestTrue(TEXT("epic parses"), ArenaLook::Parse(TEXT("EPIC"), Look));
	TestTrue(TEXT("epic is Epic"), Look == EArenaLook::Epic);
	TestFalse(TEXT("unknown is refused"), ArenaLook::Parse(TEXT("ultra"), Look));
	TestTrue(TEXT("unknown falls back to Flat"), Look == EArenaLook::Flat);
	TestEqual(TEXT("names round-trip"), FString(ArenaLook::Name(EArenaLook::Quest)), FString(TEXT("quest")));
	// The automation run passes no -MageArenaLook, so the suite sees the pre-push greybox.
	TestTrue(TEXT("the test process is flat"), ArenaLook::IsFlat());
	return true;
}

#endif
