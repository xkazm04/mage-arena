#pragma once

#include "CoreMinimal.h"

class UArenaSessionSubsystem;

/**
 * The evidence contract every capture driver shares (T45). A requested screenshot either lands on disk within
 * ShotBudgetS, decodes and passes the pixel floor, or the driver logs MAGEVR_CAPTURE_FAIL; no driver starts without a
 * renderer; and a run that logs MAGEVR_CAPTURE_DONE first writes a manifest of what it captured.
 */
namespace MageArenaCapture
{
// Wall-clock seconds a requested screenshot has to land and decode. A request is served within a frame (in the T45
// style-pick run none was still open a tick later); the longest any capture allows a shot is the greybox capture's 20 s
// warm-up shot (GreyboxCapture.cpp, WaitInitial), so 30 s is a stall, not a slow frame.
constexpr double ShotBudgetS = 30.0;
// The pixel floor point-samples a GridX x GridY grid. A shot passes with at least MinDistinctColours colours (5 bits a
// channel) and a Rec.709 luma standard deviation of at least MinLumaStdDev (0-255). Measured on Wolf: the 33 earlier
// stills and style-pick frames (runs/A02, T08, T10-T16) read 47 colours and 24.83 at the lowest; the 1870 frames of the
// T45 style-pick run read 20 and 28.63 at the lowest (a blink vignette: black surround, the arena in a centre window);
// the 2026-10-04 style-pick frame 1, a black screen with one brown strip, reads 6 and 17.29; a uniform frame reads 1 and
// 0. 12 colours sits between 6 and 20 with room on both sides; luma alone cannot tell the strip from a scene, so its
// floor only rejects flat frames.
constexpr int32 GridX = 32;
constexpr int32 GridY = 18;
constexpr int32 MinDistinctColours = 12;
constexpr double MinLumaStdDev = 4.0;
// Seconds the recorded WAV may differ from the requested length, and the game clock from the mixer clock. The
// 2026-10 take that "came out 20.8 s long for 30 s of play" is 10 s out; see runs/T45/REPORT.md for the live numbers.
constexpr double AudioToleranceS = 1.0;

struct FPixelSanity
{
	bool bOk = false;
	int32 Width = 0;
	int32 Height = 0;
	int32 Distinct = 0;
	double LumaStdDev = 0.0;
	FString Why;
};

/** The floor over a pixel buffer of Width x Height. Pure: no file and no RHI, so it runs under -nullrhi. */
FPixelSanity CheckPixels(const TArray<FColor>& Pixels, int32 Width, int32 Height);
/** Reads and decodes a PNG, then applies the floor. bDecoded is false while the file is missing, short or undecodable. */
FPixelSanity CheckPng(const FString& Path, bool& bDecoded);

struct FWavInfo
{
	int32 SampleRate = 0;
	int32 Channels = 0;
	int32 BitsPerSample = 0;
	int64 DataBytes = 0;
	double DurationS = 0.0;
};

/** Reads a RIFF/WAVE header: the fmt chunk and the data chunk's stated length. Pure over the bytes. */
bool ParseWav(const TArray<uint8>& Bytes, FWavInfo& Out, FString& Error);
/** Empty when the WAV lasts ExpectS within ToleranceS; else the FAIL reason with the measured numbers. */
FString CheckWavDuration(const TArray<uint8>& Bytes, double ExpectS, double ToleranceS);
/** Empty when the game-time recording length and the mixer clock agree within ToleranceS; else the FAIL reason. */
FString CheckMixerDrift(double AudioS, double MixerS, double ToleranceS);

/** False under -nullrhi or in a process that can never render: no screenshot can land there. */
bool CanCapture();
/** The commit HEAD names, read from the .git files (a worktree's gitdir included); "unknown" when it cannot be read. */
FString ReadGitCommit(const FString& RepoDir);
/** <repo>/<Relative>, from the project directory (apps/vr/Game). */
FString RepoPath(const FString& Relative);
}

struct FCaptureShotRecord
{
	FString Name;
	FString Path;
	int64 Bytes = 0;
	int32 Tick = -1;
	int32 Bout = -1;
	FString Stage;
	MageArenaCapture::FPixelSanity Pixel;
	double RequestedWall = 0.0;
	bool bLanded = false;
};

/** What a capture run took, written as manifest.json next to its shots when the run logs DONE. */
struct FCaptureManifest
{
	FString Driver;
	FString Commit;
	int64 Seed = -1;
	FString Flags;
	TMap<FString, FString> Notes;
	TArray<FCaptureShotRecord> Shots;

	FString ToJson() const;
	static bool FromJson(const FString& Text, FCaptureManifest& Out, FString& Error);
};

enum class ECaptureShotPoll : uint8
{
	Pending,
	Landed,
	Failed
};

/** Held by each driver: records the shots it requests, watches them land, and writes the manifest at DONE. */
class FCaptureEvidence
{
public:
	void Begin(const TCHAR* Driver);
	/** Records a requested shot, with the tick, bout and stage of the session when there is one. */
	void Requested(const FString& Name, const FString& Path, const UArenaSessionSubsystem* Session);
	void Requested(const FString& Name, const FString& Path, int32 Tick, int32 Bout, const FString& Stage, int64 Seed, double NowWall);
	/**
	 * Checks every open shot. Failed when one has not landed within ShotBudgetS or landed under the pixel floor;
	 * Pending while one is still on its way; Landed when none is open. bRequestOpen: the engine still holds a request
	 * (FScreenshotRequest), which can only be the newest shot's.
	 */
	ECaptureShotPoll Poll(double NowWall, bool bRequestOpen);
	ECaptureShotPoll Poll();
	const FString& GetFailure() const { return Failure; }
	void Note(const FString& Key, const FString& Value) { Manifest.Notes.Add(Key, Value); }
	const FCaptureManifest& GetManifest() const { return Manifest; }
	/** Writes the manifest. False (with Error) when a shot is still open or the file cannot be written. */
	bool WriteManifest(const FString& Path, FString& Error) const;

private:
	FCaptureManifest Manifest;
	FString Failure;
};
