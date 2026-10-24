#include "Session/CaptureEvidence.h"

#include "Session/ArenaSession.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace MageArenaCapture
{
FPixelSanity CheckPixels(const TArray<FColor>& Pixels, int32 Width, int32 Height)
{
	FPixelSanity Out;
	Out.Width = Width;
	Out.Height = Height;
	if (Width <= 0 || Height <= 0 || Pixels.Num() < Width * Height)
	{
		Out.Why = FString::Printf(TEXT("empty image %dx%d"), Width, Height);
		return Out;
	}
	TSet<int32> Colours;
	double Sum = 0.0;
	double SumSq = 0.0;
	for (int32 Gy = 0; Gy < GridY; ++Gy)
	{
		const int32 Y = FMath::Min(Height - 1, ((2 * Gy + 1) * Height) / (2 * GridY));
		for (int32 Gx = 0; Gx < GridX; ++Gx)
		{
			const int32 X = FMath::Min(Width - 1, ((2 * Gx + 1) * Width) / (2 * GridX));
			const FColor& C = Pixels[Y * Width + X];
			Colours.Add(((C.R >> 3) << 10) | ((C.G >> 3) << 5) | (C.B >> 3));
			const double Luma = 0.2126 * C.R + 0.7152 * C.G + 0.0722 * C.B;
			Sum += Luma;
			SumSq += Luma * Luma;
		}
	}
	const double Count = static_cast<double>(GridX * GridY);
	const double Mean = Sum / Count;
	Out.Distinct = Colours.Num();
	Out.LumaStdDev = FMath::Sqrt(FMath::Max(0.0, SumSq / Count - Mean * Mean));
	Out.bOk = Out.Distinct >= MinDistinctColours && Out.LumaStdDev >= MinLumaStdDev;
	if (!Out.bOk)
	{
		Out.Why = FString::Printf(TEXT("near-uniform distinct=%d luma_sd=%.2f (floor %d, %.1f)"), Out.Distinct, Out.LumaStdDev,
		    MinDistinctColours, MinLumaStdDev);
	}
	return Out;
}

FPixelSanity CheckPng(const FString& Path, bool& bDecoded)
{
	bDecoded = false;
	FPixelSanity Out;
	TArray<uint8> Compressed;
	if (IFileManager::Get().FileSize(*Path) <= 0 || !FFileHelper::LoadFileToArray(Compressed, *Path))
	{
		Out.Why = TEXT("not on disk");
		return Out;
	}
	IImageWrapperModule& ImageModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> Wrapper = ImageModule.CreateImageWrapper(EImageFormat::PNG);
	TArray<uint8> Raw;
	if (!Wrapper.IsValid() || !Wrapper->SetCompressed(Compressed.GetData(), Compressed.Num()) || !Wrapper->GetRaw(ERGBFormat::BGRA, 8, Raw))
	{
		Out.Why = TEXT("not a decodable png");
		return Out;
	}
	const int32 Width = static_cast<int32>(Wrapper->GetWidth());
	const int32 Height = static_cast<int32>(Wrapper->GetHeight());
	if (Raw.Num() < Width * Height * 4)
	{
		Out.Why = TEXT("short png");
		return Out;
	}
	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(Width * Height);
	FMemory::Memcpy(Pixels.GetData(), Raw.GetData(), static_cast<SIZE_T>(Width) * Height * 4);
	bDecoded = true;
	return CheckPixels(Pixels, Width, Height);
}

namespace
{
uint32 ReadU32(const TArray<uint8>& Bytes, int32 At)
{
	return static_cast<uint32>(Bytes[At]) | (static_cast<uint32>(Bytes[At + 1]) << 8) | (static_cast<uint32>(Bytes[At + 2]) << 16) | (static_cast<uint32>(Bytes[At + 3]) << 24);
}

uint16 ReadU16(const TArray<uint8>& Bytes, int32 At)
{
	return static_cast<uint16>(Bytes[At] | (Bytes[At + 1] << 8));
}

bool ChunkIs(const TArray<uint8>& Bytes, int32 At, const char* Id)
{
	return Bytes[At] == Id[0] && Bytes[At + 1] == Id[1] && Bytes[At + 2] == Id[2] && Bytes[At + 3] == Id[3];
}
}

bool ParseWav(const TArray<uint8>& Bytes, FWavInfo& Out, FString& Error)
{
	Out = FWavInfo();
	if (Bytes.Num() < 12 || !ChunkIs(Bytes, 0, "RIFF") || !ChunkIs(Bytes, 8, "WAVE"))
	{
		Error = TEXT("not a RIFF/WAVE file");
		return false;
	}
	bool bFmt = false;
	int32 At = 12;
	while (At + 8 <= Bytes.Num())
	{
		const uint32 Size = ReadU32(Bytes, At + 4);
		if (ChunkIs(Bytes, At, "fmt "))
		{
			if (Size < 16 || At + 8 + 16 > Bytes.Num())
			{
				Error = TEXT("short fmt chunk");
				return false;
			}
			Out.Channels = ReadU16(Bytes, At + 10);
			Out.SampleRate = static_cast<int32>(ReadU32(Bytes, At + 12));
			Out.BitsPerSample = ReadU16(Bytes, At + 22);
			bFmt = true;
		}
		else if (ChunkIs(Bytes, At, "data"))
		{
			if (!bFmt)
			{
				Error = TEXT("data chunk before the fmt chunk");
				return false;
			}
			if (Out.SampleRate <= 0 || Out.Channels <= 0 || Out.BitsPerSample <= 0 || Out.BitsPerSample % 8 != 0)
			{
				Error = FString::Printf(TEXT("bad format rate=%d channels=%d bits=%d"), Out.SampleRate, Out.Channels, Out.BitsPerSample);
				return false;
			}
			Out.DataBytes = Size;
			Out.DurationS = static_cast<double>(Size) / (static_cast<double>(Out.SampleRate) * Out.Channels * (Out.BitsPerSample / 8));
			return true;
		}
		// Chunks are word-aligned: an odd size carries one pad byte.
		At += 8 + static_cast<int32>(Size) + static_cast<int32>(Size & 1);
	}
	Error = bFmt ? TEXT("no data chunk") : TEXT("no fmt chunk");
	return false;
}

FString CheckWavDuration(const TArray<uint8>& Bytes, double ExpectS, double ToleranceS)
{
	FWavInfo Info;
	FString Error;
	if (!ParseWav(Bytes, Info, Error))
	{
		return FString::Printf(TEXT("wav unreadable: %s"), *Error);
	}
	if (FMath::Abs(Info.DurationS - ExpectS) > ToleranceS)
	{
		return FString::Printf(TEXT("wav duration %.3fs expected %.3fs within %.1fs rate=%d channels=%d bits=%d bytes=%lld"), Info.DurationS, ExpectS,
		    ToleranceS, Info.SampleRate, Info.Channels, Info.BitsPerSample, Info.DataBytes);
	}
	return FString();
}

FString CheckMixerDrift(double AudioS, double MixerS, double ToleranceS)
{
	if (MixerS < 0.0 || FMath::Abs(AudioS - MixerS) > ToleranceS)
	{
		return FString::Printf(TEXT("mixer drift audio=%.3f mixer=%.3f"), AudioS, MixerS);
	}
	return FString();
}

bool CanCapture()
{
	return !FParse::Param(FCommandLine::Get(), TEXT("nullrhi")) && FApp::CanEverRender();
}

FString ReadGitCommit(const FString& RepoDir)
{
	const FString DotGit = FPaths::Combine(RepoDir, TEXT(".git"));
	FString GitDir = DotGit;
	FString Text;
	// A worktree's .git is a file naming its git directory.
	if (!IFileManager::Get().DirectoryExists(*DotGit) && FFileHelper::LoadFileToString(Text, *DotGit) && Text.StartsWith(TEXT("gitdir:")))
	{
		GitDir = Text.Mid(7).TrimStartAndEnd();
		if (FPaths::IsRelative(GitDir))
		{
			GitDir = FPaths::Combine(RepoDir, GitDir);
		}
	}
	FString Head;
	if (!FFileHelper::LoadFileToString(Head, *FPaths::Combine(GitDir, TEXT("HEAD"))))
	{
		return TEXT("unknown");
	}
	Head.TrimStartAndEndInline();
	if (!Head.StartsWith(TEXT("ref:")))
	{
		return Head;
	}
	const FString Ref = Head.Mid(4).TrimStartAndEnd();
	FString CommonDir = GitDir;
	if (FFileHelper::LoadFileToString(Text, *FPaths::Combine(GitDir, TEXT("commondir"))))
	{
		CommonDir = Text.TrimStartAndEnd();
		if (FPaths::IsRelative(CommonDir))
		{
			CommonDir = FPaths::Combine(GitDir, CommonDir);
		}
	}
	for (const FString& Dir : {GitDir, CommonDir})
	{
		if (FFileHelper::LoadFileToString(Text, *FPaths::Combine(Dir, Ref)))
		{
			return Text.TrimStartAndEnd();
		}
	}
	TArray<FString> Packed;
	if (FFileHelper::LoadFileToStringArray(Packed, *FPaths::Combine(CommonDir, TEXT("packed-refs"))))
	{
		for (const FString& Line : Packed)
		{
			FString Sha;
			FString Name;
			if (!Line.StartsWith(TEXT("#")) && Line.Split(TEXT(" "), &Sha, &Name) && Name.TrimStartAndEnd() == Ref)
			{
				return Sha;
			}
		}
	}
	return TEXT("unknown");
}

FString RepoPath(const FString& Relative)
{
	FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("../../.."), Relative);
	FPaths::CollapseRelativeDirectories(Path);
	return FPaths::ConvertRelativePathToFull(Path);
}
}

FString FCaptureManifest::ToJson() const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("driver"), Driver);
	Root->SetStringField(TEXT("commit"), Commit);
	Root->SetNumberField(TEXT("seed"), static_cast<double>(Seed));
	Root->SetStringField(TEXT("flags"), Flags);
	TSharedRef<FJsonObject> NoteObject = MakeShared<FJsonObject>();
	for (const TPair<FString, FString>& Pair : Notes)
	{
		NoteObject->SetStringField(Pair.Key, Pair.Value);
	}
	Root->SetObjectField(TEXT("notes"), NoteObject);
	TArray<TSharedPtr<FJsonValue>> ShotValues;
	for (const FCaptureShotRecord& Shot : Shots)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("name"), Shot.Name);
		Object->SetStringField(TEXT("path"), Shot.Path);
		Object->SetNumberField(TEXT("bytes"), static_cast<double>(Shot.Bytes));
		Object->SetNumberField(TEXT("tick"), Shot.Tick);
		Object->SetNumberField(TEXT("bout"), Shot.Bout);
		Object->SetStringField(TEXT("stage"), Shot.Stage);
		TSharedRef<FJsonObject> Pixel = MakeShared<FJsonObject>();
		Pixel->SetBoolField(TEXT("ok"), Shot.Pixel.bOk);
		Pixel->SetNumberField(TEXT("width"), Shot.Pixel.Width);
		Pixel->SetNumberField(TEXT("height"), Shot.Pixel.Height);
		Pixel->SetNumberField(TEXT("distinct"), Shot.Pixel.Distinct);
		Pixel->SetNumberField(TEXT("lumaStdDev"), Shot.Pixel.LumaStdDev);
		Pixel->SetStringField(TEXT("why"), Shot.Pixel.Why);
		Object->SetObjectField(TEXT("pixel"), Pixel);
		ShotValues.Add(MakeShared<FJsonValueObject>(Object));
	}
	Root->SetArrayField(TEXT("shots"), ShotValues);
	FString Text;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	FJsonSerializer::Serialize(Root, Writer);
	return Text;
}

bool FCaptureManifest::FromJson(const FString& Text, FCaptureManifest& Out, FString& Error)
{
	Out = FCaptureManifest();
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		Error = TEXT("not JSON");
		return false;
	}
	double Seed = -1.0;
	if (!Root->TryGetStringField(TEXT("driver"), Out.Driver) || !Root->TryGetStringField(TEXT("commit"), Out.Commit) || !Root->TryGetNumberField(TEXT("seed"), Seed) || !Root->TryGetStringField(TEXT("flags"), Out.Flags))
	{
		Error = TEXT("missing driver, commit, seed or flags");
		return false;
	}
	Out.Seed = static_cast<int64>(Seed);
	const TSharedPtr<FJsonObject>* NoteObject = nullptr;
	if (Root->TryGetObjectField(TEXT("notes"), NoteObject) && NoteObject)
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*NoteObject)->Values)
		{
			Out.Notes.Add(Pair.Key, Pair.Value.IsValid() ? Pair.Value->AsString() : FString());
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* ShotValues = nullptr;
	if (!Root->TryGetArrayField(TEXT("shots"), ShotValues) || !ShotValues)
	{
		Error = TEXT("missing shots");
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : *ShotValues)
	{
		const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
		const TSharedPtr<FJsonObject>* Pixel = nullptr;
		FCaptureShotRecord Shot;
		double Bytes = 0.0;
		if (!Object.IsValid() || !Object->TryGetStringField(TEXT("name"), Shot.Name) || !Object->TryGetStringField(TEXT("path"), Shot.Path) || !Object->TryGetNumberField(TEXT("bytes"), Bytes) || !Object->TryGetNumberField(TEXT("tick"), Shot.Tick) || !Object->TryGetNumberField(TEXT("bout"), Shot.Bout) || !Object->TryGetStringField(TEXT("stage"), Shot.Stage) || !Object->TryGetObjectField(TEXT("pixel"), Pixel) || !Pixel || !(*Pixel)->TryGetBoolField(TEXT("ok"), Shot.Pixel.bOk) || !(*Pixel)->TryGetNumberField(TEXT("width"), Shot.Pixel.Width) || !(*Pixel)->TryGetNumberField(TEXT("height"), Shot.Pixel.Height) || !(*Pixel)->TryGetNumberField(TEXT("distinct"), Shot.Pixel.Distinct) || !(*Pixel)->TryGetNumberField(TEXT("lumaStdDev"), Shot.Pixel.LumaStdDev) || !(*Pixel)->TryGetStringField(TEXT("why"), Shot.Pixel.Why))
		{
			Error = FString::Printf(TEXT("shot %d is incomplete"), Out.Shots.Num());
			return false;
		}
		Shot.Bytes = static_cast<int64>(Bytes);
		Shot.bLanded = true;
		Out.Shots.Add(Shot);
	}
	return true;
}

void FCaptureEvidence::Begin(const TCHAR* Driver)
{
	Manifest = FCaptureManifest();
	Manifest.Driver = Driver;
	Manifest.Commit = MageArenaCapture::ReadGitCommit(MageArenaCapture::RepoPath(TEXT("")));
	Manifest.Flags = FCommandLine::Get();
	Failure.Reset();
}

void FCaptureEvidence::Requested(const FString& Name, const FString& Path, const UArenaSessionSubsystem* Session)
{
	const FArenaSession* Bout = Session ? &Session->GetSession() : nullptr;
	Requested(Name, Path, Bout ? Bout->GetGames().State.Tick : -1, Bout ? Bout->GetBoutIndex() : -1, Bout ? Bout->GetStage() : FString(),
	    Bout ? static_cast<int64>(Bout->GetGames().State.Seed) : -1, FPlatformTime::Seconds());
}

void FCaptureEvidence::Requested(const FString& Name, const FString& Path, int32 Tick, int32 Bout, const FString& Stage, int64 Seed, double NowWall)
{
	FCaptureShotRecord& Shot = Manifest.Shots.AddDefaulted_GetRef();
	Shot.Name = Name;
	Shot.Path = Path;
	Shot.Tick = Tick;
	Shot.Bout = Bout;
	Shot.Stage = Stage;
	Shot.RequestedWall = NowWall;
	if (Manifest.Seed < 0)
	{
		Manifest.Seed = Seed;
	}
}

ECaptureShotPoll FCaptureEvidence::Poll(double NowWall, bool bRequestOpen)
{
	if (!Failure.IsEmpty())
	{
		return ECaptureShotPoll::Failed;
	}
	bool bPending = false;
	for (int32 Index = 0; Index < Manifest.Shots.Num(); ++Index)
	{
		FCaptureShotRecord& Shot = Manifest.Shots[Index];
		if (Shot.bLanded)
		{
			continue;
		}
		const double Waited = NowWall - Shot.RequestedWall;
		bool bDecoded = false;
		if (!(bRequestOpen && Index == Manifest.Shots.Num() - 1))
		{
			Shot.Pixel = MageArenaCapture::CheckPng(Shot.Path, bDecoded);
		}
		if (bDecoded)
		{
			if (!Shot.Pixel.bOk)
			{
				Failure = FString::Printf(TEXT("shot failed the pixel floor name=%s %s"), *Shot.Name, *Shot.Pixel.Why);
				return ECaptureShotPoll::Failed;
			}
			Shot.Bytes = IFileManager::Get().FileSize(*Shot.Path);
			Shot.bLanded = true;
			continue;
		}
		if (Waited > MageArenaCapture::ShotBudgetS)
		{
			Failure = FString::Printf(TEXT("shot stuck name=%s waited=%.1fs"), *Shot.Name, Waited);
			return ECaptureShotPoll::Failed;
		}
		bPending = true;
	}
	return bPending ? ECaptureShotPoll::Pending : ECaptureShotPoll::Landed;
}

ECaptureShotPoll FCaptureEvidence::Poll()
{
	return Poll(FPlatformTime::Seconds(), FScreenshotRequest::IsScreenshotRequested());
}

bool FCaptureEvidence::WriteManifest(const FString& Path, FString& Error) const
{
	for (const FCaptureShotRecord& Shot : Manifest.Shots)
	{
		if (!Shot.bLanded)
		{
			Error = FString::Printf(TEXT("manifest written with %s still open"), *Shot.Name);
			return false;
		}
	}
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	if (!FFileHelper::SaveStringToFile(Manifest.ToJson(), *Path))
	{
		Error = FString::Printf(TEXT("manifest not written %s"), *Path);
		return false;
	}
	return true;
}
