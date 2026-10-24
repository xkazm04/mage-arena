#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Session/CaptureEvidence.h"
#include "Session/StylePickCapture.h"

#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"

namespace
{
// Counts the capture marker lines a driver writes while the probe is attached.
struct FCaptureMarkerProbe : FOutputDevice
{
	int32 Fails = 0;
	int32 Dones = 0;
	FString LastFail;

	virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type, const FName&) override
	{
		if (!Message)
		{
			return;
		}
		if (const TCHAR* At = FCString::Strstr(Message, TEXT("MAGEVR_CAPTURE_FAIL")))
		{
			++Fails;
			LastFail = At;
		}
		if (FCString::Strstr(Message, TEXT("MAGEVR_CAPTURE_DONE")))
		{
			++Dones;
		}
	}
};

struct FCaptureMarkerScope
{
	FCaptureMarkerProbe& Probe;

	explicit FCaptureMarkerScope(FCaptureMarkerProbe& InProbe)
	    : Probe(InProbe)
	{
		GLog->AddOutputDevice(&Probe);
	}

	~FCaptureMarkerScope()
	{
		// Lines reach an output device when the redirector flushes, not when UE_LOG returns.
		GLog->Flush();
		GLog->RemoveOutputDevice(&Probe);
	}
};

// A frame with structure: a gradient with a checker over it.
TArray<FColor> VariedPixels(int32 Width, int32 Height)
{
	TArray<FColor> Pixels;
	Pixels.SetNum(Width * Height);
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const uint8 Checker = ((X / 8 + Y / 8) % 2) ? 60 : 0;
			Pixels[Y * Width + X] = FColor(static_cast<uint8>(X * 255 / Width), static_cast<uint8>(Y * 255 / Height), static_cast<uint8>(120 + Checker), 255);
		}
	}
	return Pixels;
}

bool WritePng(const FString& Path, const TArray<FColor>& Pixels, int32 Width, int32 Height)
{
	IImageWrapperModule& ImageModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> Wrapper = ImageModule.CreateImageWrapper(EImageFormat::PNG);
	if (!Wrapper.IsValid() || !Wrapper->SetRaw(Pixels.GetData(), Pixels.Num() * sizeof(FColor), Width, Height, ERGBFormat::BGRA, 8))
	{
		return false;
	}
	const TArray64<uint8> Compressed = Wrapper->GetCompressed();
	return FFileHelper::SaveArrayToFile(Compressed, *Path);
}

// A 44-byte PCM header: RIFF/WAVE, a 16-byte fmt chunk and a data chunk stating Seconds of audio.
TArray<uint8> WavHeader(double Seconds, int32 Rate = 48000, int32 Channels = 2, int32 Bits = 16)
{
	const uint32 Block = static_cast<uint32>(Channels * Bits / 8);
	const uint32 Data = static_cast<uint32>(FMath::RoundToInt64(Seconds * Rate)) * Block;
	TArray<uint8> Bytes;
	auto Tag = [&Bytes](const char* Id)
	{ Bytes.Append(reinterpret_cast<const uint8*>(Id), 4); };
	auto U32 = [&Bytes](uint32 Value)
	{ for (int32 I = 0; I < 4; ++I) Bytes.Add(static_cast<uint8>(Value >> (8 * I))); };
	auto U16 = [&Bytes](uint16 Value)
	{ Bytes.Add(static_cast<uint8>(Value)); Bytes.Add(static_cast<uint8>(Value >> 8)); };
	Tag("RIFF");
	U32(36 + Data);
	Tag("WAVE");
	Tag("fmt ");
	U32(16);
	U16(1);
	U16(static_cast<uint16>(Channels));
	U32(static_cast<uint32>(Rate));
	U32(static_cast<uint32>(Rate) * Block);
	U16(static_cast<uint16>(Block));
	U16(static_cast<uint16>(Bits));
	Tag("data");
	U32(Data);
	return Bytes;
}

FString ScratchDir()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MageArenaTests"), TEXT("capture-evidence"));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaCaptureStylePickFailsWithoutSession, "MageArena.Capture.StylePick.FailsWithoutSession",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaCaptureStylePickFailsWithoutSession::RunTest(const FString& Parameters)
{
	// A driver outside any world has no session subsystem to start. It must say so, not go quiet.
	AddExpectedError(TEXT("MAGEVR_CAPTURE_FAIL"), EAutomationExpectedErrorFlags::Contains, 1);
	FCaptureMarkerProbe Probe;
	{
		FCaptureMarkerScope Scope(Probe);
		UStylePickCaptureDriver* Driver = NewObject<UStylePickCaptureDriver>(GetTransientPackage());
		Driver->Start();
		Driver->Tick(1.0f / 60.0f);
	}
	// The assertion texts must not contain the marker: AddExpectedError would swallow their failure as the expected line.
	TestEqual(TEXT("fail markers logged by a driver with no session"), Probe.Fails, 1);
	TestEqual(TEXT("done markers logged by a driver with no session"), Probe.Dones, 0);
	TestTrue(TEXT("the fail line names the missing session"), Probe.LastFail.Contains(TEXT("no session subsystem")));
	AddInfo(FString::Printf(TEXT("fail line: %s"), *Probe.LastFail));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaCaptureStylePickShortRunFails, "MageArena.Capture.StylePick.ShortRunFails",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaCaptureStylePickShortRunFails::RunTest(const FString& Parameters)
{
	// The check the driver runs when the duel leaves "active" or the sequence ends; DONE is only reachable past it.
	AddExpectedError(TEXT("MAGEVR_CAPTURE_FAIL"), EAutomationExpectedErrorFlags::Contains, 1);
	FCaptureMarkerProbe Probe;
	bool bShort = true;
	bool bFull = false;
	bool bFloor = false;
	{
		FCaptureMarkerScope Scope(Probe);
		bShort = UStylePickCaptureDriver::CheckRunLength(180, TEXT("won"));
		bFull = UStylePickCaptureDriver::CheckRunLength(UStylePickCaptureDriver::SequenceTarget, TEXT("active"));
		bFloor = UStylePickCaptureDriver::CheckRunLength(UStylePickCaptureDriver::MinSequenceFrames, TEXT("won"));
	}
	TestFalse(TEXT("a 3 s run (180 frames) does not pass"), bShort);
	TestTrue(TEXT("the full 3600-frame run passes"), bFull);
	TestTrue(TEXT("a run of exactly the minimum passes"), bFloor);
	TestEqual(TEXT("fail markers logged"), Probe.Fails, 1);
	TestEqual(TEXT("done markers logged"), Probe.Dones, 0);
	TestTrue(TEXT("the fail line names the short run"), Probe.LastFail.Contains(TEXT("short run seq=180 stage=won")));
	TestTrue(TEXT("the minimum is below the full run"), UStylePickCaptureDriver::MinSequenceFrames < UStylePickCaptureDriver::SequenceTarget);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaCaptureShotWatchdogStuckShotFails, "MageArena.Capture.ShotWatchdog.StuckShotFails",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaCaptureShotWatchdogStuckShotFails::RunTest(const FString& Parameters)
{
	const FString Dir = ScratchDir();
	IFileManager::Get().MakeDirectory(*Dir, true);
	const FString Never = FPaths::Combine(Dir, TEXT("never-written.png"));
	IFileManager::Get().Delete(*Never, false, true, true);

	// A shot that never lands: pending inside the budget, a FAIL past it. The clock is passed in, so nothing waits.
	FCaptureEvidence Stuck;
	Stuck.Begin(TEXT("Test"));
	Stuck.Requested(TEXT("stuck.png"), Never, 5, 2, TEXT("active"), 1, 100.0);
	TestTrue(TEXT("an open request is pending"), Stuck.Poll(101.0, true) == ECaptureShotPoll::Pending);
	TestTrue(TEXT("a missing file inside the budget is pending"), Stuck.Poll(100.0 + MageArenaCapture::ShotBudgetS - 0.5, false) == ECaptureShotPoll::Pending);
	TestTrue(TEXT("a missing file past the budget fails"), Stuck.Poll(100.0 + MageArenaCapture::ShotBudgetS + 0.5, false) == ECaptureShotPoll::Failed);
	TestTrue(TEXT("the failure names the shot and the wait"), Stuck.GetFailure().Contains(TEXT("shot stuck name=stuck.png waited=30.5s")));
	TestTrue(TEXT("a failed watchdog stays failed"), Stuck.Poll(100.0, false) == ECaptureShotPoll::Failed);
	FString Error;
	TestFalse(TEXT("no manifest while a shot is open"), Stuck.WriteManifest(FPaths::Combine(Dir, TEXT("stuck-manifest.json")), Error));

	// A shot that lands with structure is accepted; one that lands uniform fails the floor.
	const FString Varied = FPaths::Combine(Dir, TEXT("varied.png"));
	const FString Black = FPaths::Combine(Dir, TEXT("black.png"));
	TArray<FColor> BlackPixels;
	BlackPixels.Init(FColor::Black, 64 * 36);
	if (!TestTrue(TEXT("wrote the test pngs"), WritePng(Varied, VariedPixels(64, 36), 64, 36) && WritePng(Black, BlackPixels, 64, 36)))
	{
		return false;
	}
	FCaptureEvidence Good;
	Good.Begin(TEXT("Test"));
	Good.Requested(TEXT("varied.png"), Varied, 7, 2, TEXT("active"), 1, 100.0);
	TestTrue(TEXT("a varied png lands"), Good.Poll(100.5, false) == ECaptureShotPoll::Landed);
	TestEqual(TEXT("its byte size is recorded"), Good.GetManifest().Shots[0].Bytes, IFileManager::Get().FileSize(*Varied));
	TestTrue(TEXT("a landed run writes its manifest"), Good.WriteManifest(FPaths::Combine(Dir, TEXT("good-manifest.json")), Error));

	FCaptureEvidence Dark;
	Dark.Begin(TEXT("Test"));
	Dark.Requested(TEXT("black.png"), Black, 7, 2, TEXT("active"), 1, 100.0);
	TestTrue(TEXT("a black png fails"), Dark.Poll(100.5, false) == ECaptureShotPoll::Failed);
	TestTrue(TEXT("the failure names the pixel floor"), Dark.GetFailure().Contains(TEXT("shot failed the pixel floor name=black.png")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaCapturePixelSanityUniformFrameFails, "MageArena.Capture.PixelSanity.UniformFrameFails",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaCapturePixelSanityUniformFrameFails::RunTest(const FString& Parameters)
{
	const int32 W = 1920;
	const int32 H = 1080;
	TArray<FColor> Uniform;
	Uniform.Init(FColor(40, 90, 160, 255), W * H);
	const MageArenaCapture::FPixelSanity Flat = MageArenaCapture::CheckPixels(Uniform, W, H);
	TestFalse(TEXT("a single-colour frame fails"), Flat.bOk);
	TestEqual(TEXT("a single-colour frame has one colour"), Flat.Distinct, 1);

	TArray<FColor> Black;
	Black.Init(FColor::Black, W * H);
	TestFalse(TEXT("a black frame fails"), MageArenaCapture::CheckPixels(Black, W, H).bOk);

	// The 2026-10-04 style-pick frame 1: black with one brown strip across the lower middle.
	TArray<FColor> Strip = Black;
	for (int32 Y = H * 65 / 100; Y < H * 72 / 100; ++Y)
	{
		for (int32 X = W * 27 / 100; X < W * 73 / 100; ++X)
		{
			Strip[Y * W + X] = FColor(140, 105, 85, 255);
		}
	}
	TestFalse(TEXT("a black frame with one strip fails"), MageArenaCapture::CheckPixels(Strip, W, H).bOk);

	const MageArenaCapture::FPixelSanity Varied = MageArenaCapture::CheckPixels(VariedPixels(W, H), W, H);
	TestTrue(*FString::Printf(TEXT("a varied frame passes (distinct %d, luma sd %.2f)"), Varied.Distinct, Varied.LumaStdDev), Varied.bOk);

	TestFalse(TEXT("an empty buffer fails"), MageArenaCapture::CheckPixels(TArray<FColor>(), 0, 0).bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaCaptureAudioDurationMismatchFails, "MageArena.Capture.Audio.DurationMismatchFails",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaCaptureAudioDurationMismatchFails::RunTest(const FString& Parameters)
{
	const double Tol = MageArenaCapture::AudioToleranceS;
	const FString Short = MageArenaCapture::CheckWavDuration(WavHeader(20.8), 30.0, Tol);
	TestFalse(TEXT("a 20.8 s WAV fails a 30 s request"), Short.IsEmpty());
	TestTrue(*FString::Printf(TEXT("the failure carries the measured length (%s)"), *Short), Short.Contains(TEXT("wav duration 20.800s")));
	TestTrue(TEXT("a 30 s WAV passes a 30 s request"), MageArenaCapture::CheckWavDuration(WavHeader(30.0), 30.0, Tol).IsEmpty());
	TestTrue(TEXT("a 30 s mono 32-bit WAV passes"), MageArenaCapture::CheckWavDuration(WavHeader(30.0, 44100, 1, 32), 30.0, Tol).IsEmpty());

	MageArenaCapture::FWavInfo Info;
	FString Error;
	TestTrue(TEXT("the header parses"), MageArenaCapture::ParseWav(WavHeader(20.8), Info, Error));
	TestEqual(TEXT("sample rate"), Info.SampleRate, 48000);
	TestEqual(TEXT("channels"), Info.Channels, 2);
	TestEqual(TEXT("bits"), Info.BitsPerSample, 16);
	TestTrue(TEXT("duration"), FMath::IsNearlyEqual(Info.DurationS, 20.8, 1.0e-6));

	TArray<uint8> NotWav;
	NotWav.Init(0, 44);
	TestFalse(TEXT("a file that is not RIFF/WAVE fails"), MageArenaCapture::CheckWavDuration(NotWav, 30.0, Tol).IsEmpty());
	TArray<uint8> NoData = WavHeader(30.0);
	NoData.SetNum(36);
	TestFalse(TEXT("a header with no data chunk fails"), MageArenaCapture::CheckWavDuration(NoData, 30.0, Tol).IsEmpty());

	// The mixer clock against the game clock: the 20.8-for-30 take fails, a buffer of lag passes.
	const FString Drift = MageArenaCapture::CheckMixerDrift(30.8, 20.8, Tol);
	TestTrue(*FString::Printf(TEXT("20.8 s of mixer for 30.8 s of play fails (%s)"), *Drift), Drift.Contains(TEXT("mixer drift audio=30.800 mixer=20.800")));
	TestTrue(TEXT("85 ms of mixer lag passes"), MageArenaCapture::CheckMixerDrift(30.8, 30.715, Tol).IsEmpty());
	TestFalse(TEXT("no audio device fails"), MageArenaCapture::CheckMixerDrift(30.8, -1.0, Tol).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaCaptureManifestRoundTrips, "MageArena.Capture.Manifest.RoundTrips",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaCaptureManifestRoundTrips::RunTest(const FString& Parameters)
{
	FCaptureManifest In;
	In.Driver = TEXT("StylePick");
	In.Commit = TEXT("84b20ed0000000000000000000000000000000ff");
	In.Seed = 1;
	In.Flags = TEXT("-game -MageArenaStylePickCapture -MageArenaStyle=13 -usefixedtimestep -fps=60");
	In.Notes.Add(TEXT("style"), TEXT("13"));
	for (int32 Index = 0; Index < 2; ++Index)
	{
		FCaptureShotRecord& Shot = In.Shots.AddDefaulted_GetRef();
		Shot.Name = FString::Printf(TEXT("seq/frame_%04d.png"), Index + 1);
		Shot.Path = FString::Printf(TEXT("C:/runs/T16/shots-13/seq/frame_%04d.png"), Index + 1);
		Shot.Bytes = 1267734 + Index;
		Shot.Tick = 41 + Index;
		Shot.Bout = 2;
		Shot.Stage = Index == 0 ? TEXT("active") : TEXT("won");
		Shot.Pixel.bOk = Index == 0;
		Shot.Pixel.Width = 1920;
		Shot.Pixel.Height = 1080;
		Shot.Pixel.Distinct = 63 - Index * 60;
		Shot.Pixel.LumaStdDev = 39.87 - Index;
		Shot.Pixel.Why = Index == 0 ? FString() : TEXT("near-uniform distinct=3 luma_sd=38.87 (floor 12, 4.0)");
	}

	FCaptureManifest Out;
	FString Error;
	if (!TestTrue(*FString::Printf(TEXT("the manifest reads back (%s)"), *Error), FCaptureManifest::FromJson(In.ToJson(), Out, Error)))
	{
		return false;
	}
	TestEqual(TEXT("driver"), Out.Driver, In.Driver);
	TestEqual(TEXT("commit"), Out.Commit, In.Commit);
	TestEqual(TEXT("seed"), Out.Seed, In.Seed);
	TestEqual(TEXT("flags"), Out.Flags, In.Flags);
	TestEqual(TEXT("note style"), Out.Notes.FindRef(TEXT("style")), FString(TEXT("13")));
	if (!TestEqual(TEXT("shot count"), Out.Shots.Num(), In.Shots.Num()))
	{
		return false;
	}
	for (int32 Index = 0; Index < In.Shots.Num(); ++Index)
	{
		const FCaptureShotRecord& A = In.Shots[Index];
		const FCaptureShotRecord& B = Out.Shots[Index];
		TestEqual(TEXT("name"), B.Name, A.Name);
		TestEqual(TEXT("path"), B.Path, A.Path);
		TestEqual(TEXT("bytes"), B.Bytes, A.Bytes);
		TestEqual(TEXT("tick"), B.Tick, A.Tick);
		TestEqual(TEXT("bout"), B.Bout, A.Bout);
		TestEqual(TEXT("stage"), B.Stage, A.Stage);
		TestTrue(TEXT("pixel ok"), B.Pixel.bOk == A.Pixel.bOk);
		TestEqual(TEXT("pixel width"), B.Pixel.Width, A.Pixel.Width);
		TestEqual(TEXT("pixel height"), B.Pixel.Height, A.Pixel.Height);
		TestEqual(TEXT("pixel distinct"), B.Pixel.Distinct, A.Pixel.Distinct);
		TestTrue(TEXT("pixel luma sd"), FMath::IsNearlyEqual(B.Pixel.LumaStdDev, A.Pixel.LumaStdDev, 1.0e-9));
		TestEqual(TEXT("pixel why"), B.Pixel.Why, A.Pixel.Why);
	}

	FCaptureManifest Broken;
	TestFalse(TEXT("a manifest without its shots does not read"), FCaptureManifest::FromJson(TEXT("{\"driver\":\"x\",\"commit\":\"y\",\"seed\":1,\"flags\":\"\"}"), Broken, Error));

	// The commit is read from this tree's .git, not left blank.
	const FString Commit = MageArenaCapture::ReadGitCommit(MageArenaCapture::RepoPath(TEXT("")));
	TestTrue(*FString::Printf(TEXT("the tree's commit is a 40-character sha (%s)"), *Commit), Commit.Len() == 40);
	return true;
}

#endif
