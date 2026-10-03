#include "Combat/AbsorbResolver.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
bool Fail(FString& OutError, const FString& Message)
{
	OutError = Message;
	return false;
}

bool ParseDrain(const FString& Formula, double& OutBase, double& OutPerRank, FString& OutError)
{
	FString Left;
	FString Right;
	if (!Formula.Split(TEXT("-"), &Left, &Right))
	{
		return Fail(OutError, FString::Printf(TEXT("drainPerSecond has no '-': %s"), *Formula));
	}
	Left = Left.TrimStartAndEnd();
	Right = Right.TrimStartAndEnd();
	FString Coeff;
	FString Tail;
	if (!Right.Split(TEXT("*"), &Coeff, &Tail))
	{
		return Fail(OutError, FString::Printf(TEXT("drainPerSecond has no '*': %s"), *Formula));
	}
	Coeff = Coeff.TrimStartAndEnd();
	Tail = Tail.TrimStartAndEnd();
	if (Tail != TEXT("nerveRank"))
	{
		return Fail(OutError, FString::Printf(TEXT("drainPerSecond does not end in nerveRank: %s"), *Formula));
	}
	const FString Rebuilt = FString::Printf(TEXT("%s - %s * nerveRank"), *Left, *Coeff);
	if (Rebuilt != Formula)
	{
		return Fail(OutError, FString::Printf(TEXT("drainPerSecond is not 'base - coeff * nerveRank': %s"), *Formula));
	}
	OutBase = FCString::Atod(*Left);
	OutPerRank = FCString::Atod(*Coeff);
	if (!(OutBase > 0.0) || !(OutPerRank > 0.0) || !FMath::IsFinite(OutBase) || !FMath::IsFinite(OutPerRank))
	{
		return Fail(OutError, FString::Printf(TEXT("drainPerSecond coefficients are not positive: %s"), *Formula));
	}
	return true;
}

bool ParseMana(const FString& Raw, double& OutPerTier, double& OutPerRank, double& OutTierZero, FString& OutError)
{
	FString Formula;
	FString Rest;
	if (!Raw.Split(TEXT(";"), &Formula, &Rest))
	{
		return Fail(OutError, FString::Printf(TEXT("manaReturned has no ';': %s"), *Raw));
	}
	Formula = Formula.TrimStartAndEnd();
	Rest = Rest.TrimStartAndEnd();
	const FString Prefix = TEXT("tier 0 returns ");
	if (!Rest.StartsWith(Prefix))
	{
		return Fail(OutError, FString::Printf(TEXT("manaReturned has no tier-0 clause: %s"), *Raw));
	}
	const FString ZeroText = Rest.Mid(Prefix.Len()).TrimStartAndEnd();
	if (ZeroText.IsEmpty())
	{
		return Fail(OutError, FString::Printf(TEXT("manaReturned tier-0 clause is empty: %s"), *Raw));
	}
	for (int32 Index = 0; Index < ZeroText.Len(); ++Index)
	{
		if (!FChar::IsDigit(ZeroText[Index]))
		{
			return Fail(OutError, FString::Printf(TEXT("manaReturned tier-0 clause is not an integer: %s"), *Raw));
		}
	}

	const FString Mid = TEXT(" * incomingTier * (1 + ");
	const FString Suffix = TEXT(" * nerveRank)");
	if (!Formula.EndsWith(Suffix))
	{
		return Fail(OutError, FString::Printf(TEXT("manaReturned formula does not end in nerveRank: %s"), *Raw));
	}
	const int32 MidIndex = Formula.Find(Mid, ESearchCase::CaseSensitive, ESearchDir::FromStart, 0);
	if (MidIndex <= 0)
	{
		return Fail(OutError, FString::Printf(TEXT("manaReturned formula is missing the incomingTier term: %s"), *Raw));
	}
	const int32 RankStart = MidIndex + Mid.Len();
	const int32 RankLen = Formula.Len() - Suffix.Len() - RankStart;
	if (RankLen <= 0)
	{
		return Fail(OutError, FString::Printf(TEXT("manaReturned per-rank term is empty: %s"), *Raw));
	}
	const FString PerTierText = Formula.Left(MidIndex);
	const FString PerRankText = Formula.Mid(RankStart, RankLen);
	const FString Rebuilt = FString::Printf(TEXT("%s * incomingTier * (1 + %s * nerveRank)"), *PerTierText, *PerRankText);
	if (Rebuilt != Formula)
	{
		return Fail(OutError, FString::Printf(TEXT("manaReturned formula does not match the pinned shape: %s"), *Raw));
	}
	OutPerTier = FCString::Atod(*PerTierText);
	OutPerRank = FCString::Atod(*PerRankText);
	OutTierZero = FCString::Atod(*ZeroText);
	if (!(OutPerTier > 0.0) || !(OutPerRank > 0.0) || !(OutTierZero > 0.0)
		|| !FMath::IsFinite(OutPerTier) || !FMath::IsFinite(OutPerRank) || !FMath::IsFinite(OutTierZero))
	{
		return Fail(OutError, FString::Printf(TEXT("manaReturned coefficients are not positive: %s"), *Raw));
	}
	return true;
}

bool RequireNumber(const FJsonObject& Object, const TCHAR* Field, double& OutValue, const FString& Path, FString& OutError)
{
	if (!Object.TryGetNumberField(Field, OutValue) || !FMath::IsFinite(OutValue))
	{
		return Fail(OutError, FString::Printf(TEXT("absorb rules: %s is missing finite %s"), *Path, Field));
	}
	return true;
}

bool InArc(const FVector& Facing, const FVector& Toward, double ArcDeg)
{
	const FVector2D To(Toward.X, Toward.Y);
	// The arc is measured on the ground plane. Straight up, straight down, and a zero
	// vector have no angle there, so they are outside rather than a free perfect.
	if (To.SizeSquared() <= 1.0e-12)
	{
		return false;
	}
	if (ArcDeg >= 360.0)
	{
		return true;
	}
	const FVector2D Face(Facing.X, Facing.Y);
	if (Face.SizeSquared() <= 1.0e-12)
	{
		return false;
	}
	const double Dot = FVector2D::DotProduct(Face.GetSafeNormal(), To.GetSafeNormal());
	const double Limit = FMath::Cos(FMath::DegreesToRadians(ArcDeg) * 0.5) - 1.0e-12;
	return Dot >= Limit;
}
}

FString FAbsorbRules::PinnedFilePath()
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("../data/pinned/docs/design/baseline-fourteen-nights/design/data/combat.json")));
}

const TCHAR* FAbsorbRules::KindName(EAbsorbHitKind Kind)
{
	switch (Kind)
	{
	case EAbsorbHitKind::Magic:
		return TEXT("magic");
	case EAbsorbHitKind::Physical:
		return TEXT("physical");
	case EAbsorbHitKind::Unblockable:
		return TEXT("unblockable");
	default:
		return TEXT("");
	}
}

bool FAbsorbRules::LoadFromPinnedFile(FString& OutError)
{
	const FString Path = PinnedFilePath();
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		return Fail(OutError, FString::Printf(TEXT("absorb rules: cannot read %s"), *Path));
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return Fail(OutError, FString::Printf(TEXT("absorb rules: %s is not JSON"), *Path));
	}
	const TSharedPtr<FJsonObject>* AbsorbPtr = nullptr;
	if (!Root->TryGetObjectField(TEXT("absorb"), AbsorbPtr) || AbsorbPtr == nullptr || !AbsorbPtr->IsValid())
	{
		return Fail(OutError, FString::Printf(TEXT("absorb rules: %s has no absorb object"), *Path));
	}
	const FJsonObject& Absorb = **AbsorbPtr;

	FAbsorbRules Parsed;
	Parsed.SourcePath = Path;
	if (!RequireNumber(Absorb, TEXT("arcDeg"), Parsed.ArcDeg, Path, OutError)
		|| !RequireNumber(Absorb, TEXT("raiseCostMana"), Parsed.RaiseCostMana, Path, OutError)
		|| !RequireNumber(Absorb, TEXT("minReleaseBeforeReRaiseS"), Parsed.MinReleaseS, Path, OutError))
	{
		return false;
	}
	if (!(Parsed.ArcDeg > 0.0) || Parsed.MinReleaseS < 0.0)
	{
		return Fail(OutError, FString::Printf(TEXT("absorb rules: %s has a non-positive arc or a negative release gap"), *Path));
	}
	if (!Absorb.TryGetStringField(TEXT("drainPerSecond"), Parsed.DrainFormula))
	{
		return Fail(OutError, FString::Printf(TEXT("absorb rules: %s has no drainPerSecond string"), *Path));
	}
	if (!ParseDrain(Parsed.DrainFormula, Parsed.DrainBase, Parsed.DrainSubtractPerRank, OutError))
	{
		return Fail(OutError, FString::Printf(TEXT("absorb rules: %s: %s"), *Path, *OutError));
	}

	const TSharedPtr<FJsonObject>* ReductionPtr = nullptr;
	if (!Absorb.TryGetObjectField(TEXT("reduction"), ReductionPtr) || ReductionPtr == nullptr || !ReductionPtr->IsValid())
	{
		return Fail(OutError, FString::Printf(TEXT("absorb rules: %s has no reduction object"), *Path));
	}
	const FJsonObject& Reduction = **ReductionPtr;
	if (!RequireNumber(Reduction, TEXT("magic"), Parsed.ReductionMagic, Path, OutError)
		|| !RequireNumber(Reduction, TEXT("physical"), Parsed.ReductionPhysical, Path, OutError)
		|| !RequireNumber(Reduction, TEXT("unblockable"), Parsed.ReductionUnblockable, Path, OutError)
		|| !RequireNumber(Reduction, TEXT("outsideArc"), Parsed.ReductionOutsideArc, Path, OutError))
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* PerfectPtr = nullptr;
	if (!Absorb.TryGetObjectField(TEXT("perfect"), PerfectPtr) || PerfectPtr == nullptr || !PerfectPtr->IsValid())
	{
		return Fail(OutError, FString::Printf(TEXT("absorb rules: %s has no perfect object"), *Path));
	}
	const FJsonObject& Perfect = **PerfectPtr;
	if (!RequireNumber(Perfect, TEXT("windowS"), Parsed.WindowS, Path, OutError)
		|| !RequireNumber(Perfect, TEXT("reduction"), Parsed.PerfectReduction, Path, OutError)
		|| !RequireNumber(Perfect, TEXT("tierClockAdvanceS"), Parsed.TierClockAdvanceS, Path, OutError))
	{
		return false;
	}
	if (!(Parsed.WindowS > 0.0))
	{
		return Fail(OutError, FString::Printf(TEXT("absorb rules: %s has a non-positive perfect window"), *Path));
	}
	if (!Perfect.TryGetStringField(TEXT("manaReturned"), Parsed.ManaFormula))
	{
		return Fail(OutError, FString::Printf(TEXT("absorb rules: %s has no manaReturned string"), *Path));
	}
	if (!ParseMana(Parsed.ManaFormula, Parsed.ManaPerTier, Parsed.ManaPerRank, Parsed.TierZeroReturn, OutError))
	{
		return Fail(OutError, FString::Printf(TEXT("absorb rules: %s: %s"), *Path, *OutError));
	}
	const TArray<TSharedPtr<FJsonValue>>* NotAgainstValues = nullptr;
	if (!Perfect.TryGetArrayField(TEXT("notAgainst"), NotAgainstValues) || NotAgainstValues == nullptr)
	{
		return Fail(OutError, FString::Printf(TEXT("absorb rules: %s has no perfect.notAgainst array"), *Path));
	}
	for (const TSharedPtr<FJsonValue>& Value : *NotAgainstValues)
	{
		FString Name;
		if (!Value.IsValid() || !Value->TryGetString(Name) || Name.IsEmpty())
		{
			return Fail(OutError, FString::Printf(TEXT("absorb rules: %s has a non-string perfect.notAgainst entry"), *Path));
		}
		Parsed.NotAgainst.Add(Name);
	}

	*this = MoveTemp(Parsed);
	return true;
}

double FAbsorbRules::ReductionFor(EAbsorbHitKind Kind) const
{
	switch (Kind)
	{
	case EAbsorbHitKind::Magic:
		return ReductionMagic;
	case EAbsorbHitKind::Physical:
		return ReductionPhysical;
	case EAbsorbHitKind::Unblockable:
		return ReductionUnblockable;
	default:
		return 0.0;
	}
}

bool FAbsorbRules::BlocksPerfect(EAbsorbHitKind Kind) const
{
	const FString Name = KindName(Kind);
	for (const FString& Entry : NotAgainst)
	{
		if (Entry == Name)
		{
			return true;
		}
	}
	return false;
}

double FAbsorbRules::ManaForPerfect(int32 IncomingTier, double InNerveRank) const
{
	if (IncomingTier <= 0)
	{
		return TierZeroReturn;
	}
	return ManaPerTier * static_cast<double>(IncomingTier) * (1.0 + ManaPerRank * InNerveRank);
}

double FAbsorbRules::DrainPerSecond(double InNerveRank) const
{
	return FMath::Max(0.0, DrainBase - DrainSubtractPerRank * InNerveRank);
}

bool FAbsorbResolver::Init(FString& OutError)
{
	FAbsorbRules Loaded;
	if (!Loaded.LoadFromPinnedFile(OutError))
	{
		LoadedRules = FAbsorbRules();
		bReady = false;
		return false;
	}
	LoadedRules = MoveTemp(Loaded);
	bReady = true;
	return true;
}

FAbsorbResult FAbsorbResolver::Resolve(double HitTime, EAbsorbHitKind Kind, int32 IncomingTier, const FVector& HitDirection, const FWardState& WardState) const
{
	FAbsorbResult Result;
	if (!bReady)
	{
		return Result;
	}
	const bool bGuarded = WardState.bRaised && InArc(WardState.Facing, HitDirection, LoadedRules.ArcDeg);
	const double SinceOnset = HitTime - WardState.OnsetTime;
	// 1e-6 s covers double rounding between k/72 and the clip's stored sample time.
	// One frame is about 1.4e-2 s, so this is not a wider design window.
	constexpr double WindowEpsilonS = 1.0e-6;
	const bool bInWindow = SinceOnset >= -WindowEpsilonS && SinceOnset <= LoadedRules.WindowS + WindowEpsilonS;
	const bool bPerfect = bGuarded && WardState.bFresh && bInWindow && !LoadedRules.BlocksPerfect(Kind);
	if (!bGuarded)
	{
		Result.Reduction = LoadedRules.ReductionOutsideArc;
		return Result;
	}
	if (!bPerfect)
	{
		Result.Reduction = LoadedRules.ReductionFor(Kind);
		return Result;
	}
	Result.Reduction = LoadedRules.PerfectReduction;
	Result.bPerfect = true;
	Result.ManaReturned = LoadedRules.ManaForPerfect(IncomingTier, NerveRank);
	Result.TierClockAdvanceS = LoadedRules.TierClockAdvanceS;
	return Result;
}
