#include "MageArenaVR.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogMageArena);

// Primary game module so a packaged APK and a Win64 simulator launch both emit the boot marker.
class FMageArenaVRModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogMageArena, Log, TEXT("MAGEVR_BOOT_OK"));
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FMageArenaVRModule, MageArenaVR, "MageArenaVR");
