#include "Budget/BudgetFx.h"

namespace
{
const FString TemplateRoot = TEXT("/Niagara/DefaultAssets/Templates/Emitters/");
const FString AssetRoot = TEXT("/Game/Budget/");

FBudgetFxRow MakeRow(const TCHAR* Name, const TCHAR* Template, bool bGpu, int32 PeakLive, double SpawnPerS,
	int32 BurstCount, double BurstPeriodS, double LifeS, double SizeMinCm, double SizeMaxCm, const FLinearColor& Colour,
	const TCHAR* Attach)
{
	FBudgetFxRow Row;
	Row.Name = Name;
	Row.AssetPath = AssetRoot + Name;
	Row.TemplatePath = TemplateRoot + Template;
	Row.bGpu = bGpu;
	Row.PeakLive = PeakLive;
	Row.SpawnPerS = SpawnPerS;
	Row.BurstCount = BurstCount;
	Row.BurstPeriodS = BurstPeriodS;
	Row.LifeS = LifeS;
	Row.SizeMinCm = SizeMinCm;
	Row.SizeMaxCm = SizeMaxCm;
	Row.Colour = Colour;
	Row.Attach = Attach;
	return Row;
}

// Colours follow the greybox threat language: water element colour for the bolt, ward and sigil, white for the
// flash and motes, the red rim for the unblockable telegraph (CLAUDE.md).
const FLinearColor WaterColour(0.009721f, 0.745404f, 0.745404f);
const FLinearColor WhiteColour(1.0f, 1.0f, 1.0f);
const FLinearColor RimRedColour(0.8f, 0.02f, 0.02f);
}

const TArray<FBudgetFxRow>& BudgetStress::FxRows()
{
	static const TArray<FBudgetFxRow> Rows = {
		MakeRow(TEXT("NS_Budget_BoltTrail"), TEXT("Fountain"), false, 300, 600.0, 0, 0.0, 0.5, 4.0, 8.0, WaterColour,
			TEXT("attached to projectile 0")),
		MakeRow(TEXT("NS_Budget_SigilTrail"), TEXT("LocationBasedRibbon"), false, 90, 90.0, 0, 0.0, 1.0, 1.0, 1.0, WaterColour,
			TEXT("right index tip joint")),
		MakeRow(TEXT("NS_Budget_WardAbsorb"), TEXT("OmnidirectionalBurst"), false, 120, 0.0, 120, 0.5, 0.4, 4.0, 8.0, WaterColour,
			TEXT("in front of the dais, at palm height")),
		MakeRow(TEXT("NS_Budget_PerfectFlash"), TEXT("SimpleSpriteBurst"), false, 60, 0.0, 60, 1.0, 0.3, 4.0, 8.0, WhiteColour,
			TEXT("in front of the dais, at palm height")),
		MakeRow(TEXT("NS_Budget_Telegraph"), TEXT("Fountain"), false, 300, 300.0, 0, 0.0, 1.0, 4.0, 8.0, RimRedColour,
			TEXT("on a far-ring enemy")),
		MakeRow(TEXT("NS_Budget_Motes"), TEXT("HangingParticulates"), true, 600, 300.0, 0, 0.0, 2.0, 1.0, 2.0, WhiteColour,
			TEXT("over the centre pad")),
	};
	return Rows;
}

FBudgetParticleCount CountEmitters(TConstArrayView<FBudgetEmitterSample> Samples)
{
	FBudgetParticleCount Count;
	for (const FBudgetEmitterSample& Sample : Samples)
	{
		if (!Sample.bEnabled)
		{
			continue;
		}
		if (Sample.SimTarget == EBudgetSimTarget::Gpu)
		{
			// A GPU emitter that is not Active is draining or idle; its count is a stale upper bound.
			if (Sample.ExecutionState != EBudgetExecutionState::Active)
			{
				continue;
			}
			Count.Gpu += Sample.ParticleCount;
			++Count.ActiveGpuEmitters;
		}
		else
		{
			Count.Cpu += Sample.ParticleCount;
		}
	}
	return Count;
}

EBudgetParticleVerdict EvaluateParticles(int32 MinSystemsLive, int32 LiveParticlesMax, int32 ZeroParticleFrames,
	int32 GpuEmittersMax, FString& OutReason)
{
	using namespace BudgetStress;
	if (MinSystemsLive < FxRowCount)
	{
		OutReason = FString::Printf(TEXT("not exercised: only %d of %d systems were live on the worst frame"), MinSystemsLive, FxRowCount);
		return EBudgetParticleVerdict::NotExercised;
	}
	if (LiveParticlesMax <= 0)
	{
		OutReason = TEXT("not exercised: no live particles in the window");
		return EBudgetParticleVerdict::NotExercised;
	}
	if (ZeroParticleFrames > 0)
	{
		OutReason = FString::Printf(TEXT("not exercised: %d frames of the window had a total of 0 particles"), ZeroParticleFrames);
		return EBudgetParticleVerdict::NotExercised;
	}
	if (LiveParticlesMax > MaxLiveParticles)
	{
		OutReason = FString::Printf(TEXT("fail: %d live particles, over %d"), LiveParticlesMax, MaxLiveParticles);
		return EBudgetParticleVerdict::Fail;
	}
	if (GpuEmittersMax > MaxGpuEmitters)
	{
		OutReason = FString::Printf(TEXT("fail: %d GPU emitters, over %d"), GpuEmittersMax, MaxGpuEmitters);
		return EBudgetParticleVerdict::Fail;
	}
	OutReason = TEXT("exercised");
	return EBudgetParticleVerdict::Pass;
}
