#pragma once

#include "CoreMinimal.h"

/**
 * D-G4 particle proxy table and counter (docs/research/NIAGARA-PROXY-2026-10.md 1.4, 2.2, 4.5).
 * Pure data and pure functions: no Niagara header and no engine object, so a test runs under -nullrhi
 * and the Niagara-facing driver code only has to fill FBudgetEmitterSample.
 */
namespace BudgetStress
{
/** The row of the D-G4 table the particle count is judged against. */
inline constexpr int32 MaxLiveParticles = 2000;
inline constexpr int32 MaxGpuEmitters = 1;
/** The proxy has six systems; the capture must see all of them live on every frame. */
inline constexpr int32 FxRowCount = 6;
/** The per-system ceiling of the P9 budget report (docs/PROJECT-PLAN.md:396). */
inline constexpr int32 MaxCpuPeakLive = 300;
}

/** One proxy system: an effect the game will have, built from an engine emitter template. */
struct FBudgetFxRow
{
	/** Asset name, under /Game/Budget/. */
	FString Name;
	/** Asset path of the system, /Game/Budget/<Name>. */
	FString AssetPath;
	/** Engine emitter template, under /Niagara/DefaultAssets/Templates/Emitters/. */
	FString TemplatePath;
	bool bGpu = false;
	/** Particles alive at once at the steady state. */
	int32 PeakLive = 0;
	/** Continuous spawn rate; 0 for a burst row. */
	double SpawnPerS = 0.0;
	/** Burst row: particles per burst and seconds between bursts; both 0 for a continuous row. */
	int32 BurstCount = 0;
	double BurstPeriodS = 0.0;
	double LifeS = 0.0;
	/** Sprite size (or ribbon width) range in centimetres. */
	double SizeMinCm = 0.0;
	double SizeMaxCm = 0.0;
	FLinearColor Colour = FLinearColor::White;
	/** Where the system is placed, in words (the driver maps it to a component or a point). */
	FString Attach;
};

namespace BudgetStress
{
/** The 6 rows of the proxy table, in table order. */
const TArray<FBudgetFxRow>& FxRows();
}

/** What the driver reads from one emitter instance; its own enums keep Niagara out of this file. */
enum class EBudgetSimTarget : uint8
{
	Cpu,
	Gpu,
};

enum class EBudgetExecutionState : uint8
{
	Active,
	Inactive,
	InactiveClear,
	Complete,
	Disabled,
};

struct FBudgetEmitterSample
{
	bool bEnabled = true;
	EBudgetSimTarget SimTarget = EBudgetSimTarget::Cpu;
	EBudgetExecutionState ExecutionState = EBudgetExecutionState::Active;
	int32 ParticleCount = 0;
};

struct FBudgetParticleCount
{
	int32 Cpu = 0;
	int32 Gpu = 0;
	/** GPU emitters that are enabled and Active. */
	int32 ActiveGpuEmitters = 0;

	int32 Total() const { return Cpu + Gpu; }
};

/** Sums one frame. Disabled emitters are skipped; a GPU emitter counts once, and only while Active. */
FBudgetParticleCount CountEmitters(TConstArrayView<FBudgetEmitterSample> Samples);

enum class EBudgetParticleVerdict : uint8
{
	NotExercised,
	Fail,
	Pass,
};

/**
 * The verdict of the particle row. Never a pass on zeros: "not exercised" when fewer systems than rows were live on
 * some frame, when the window's live particles are 0, or when any frame's total was 0. Otherwise "fail" over
 * MaxLiveParticles or MaxGpuEmitters, else "pass". OutReason is fit for the summary's particlesNote.
 */
EBudgetParticleVerdict EvaluateParticles(int32 MinSystemsLive, int32 LiveParticlesMax, int32 ZeroParticleFrames,
	int32 GpuEmittersMax, FString& OutReason);
