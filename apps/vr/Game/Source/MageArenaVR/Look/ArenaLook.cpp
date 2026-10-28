#include "Look/ArenaLook.h"

#include "MageArenaVR.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

bool ArenaLook::Parse(const FString& Value, EArenaLook& OutLook)
{
	const FString Key = Value.TrimStartAndEnd().ToLower();
	if (Key == TEXT("flat"))
	{
		OutLook = EArenaLook::Flat;
		return true;
	}
	if (Key == TEXT("quest"))
	{
		OutLook = EArenaLook::Quest;
		return true;
	}
	if (Key == TEXT("epic"))
	{
		OutLook = EArenaLook::Epic;
		return true;
	}
	OutLook = EArenaLook::Flat;
	return false;
}

EArenaLook ArenaLook::Current()
{
	static const EArenaLook Cached = []()
	{
		FString Value;
		EArenaLook Look = EArenaLook::Flat;
		if (FParse::Value(FCommandLine::Get(), TEXT("MageArenaLook="), Value) && !Parse(Value, Look))
		{
			UE_LOG(LogMageArena, Warning, TEXT("MAGEVR_LOOK unknown -MageArenaLook=%s, using flat"), *Value);
		}
		UE_LOG(LogMageArena, Log, TEXT("MAGEVR_LOOK %s"), Name(Look));
		return Look;
	}();
	return Cached;
}

const TCHAR* ArenaLook::Name(EArenaLook Look)
{
	switch (Look)
	{
	case EArenaLook::Quest:
		return TEXT("quest");
	case EArenaLook::Epic:
		return TEXT("epic");
	default:
		return TEXT("flat");
	}
}
