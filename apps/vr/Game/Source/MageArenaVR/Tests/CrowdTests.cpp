#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/InstancedStaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Greybox/ArenaGreyboxActor.h"
#include "Greybox/ArenaLayout.h"
#include "Greybox/ArenaStands.h"
#include "Greybox/BrazierFire.h"
#include "Materials/Material.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Session/ElementColour.h"
#include "StaticMeshResources.h"

// T57: the crowd cards on the greybox tier tops, drawn as one instanced component (Greybox/ArenaStands.h).
// T65: their tunic tints and the banners, held out of every threat colour class.

namespace
{
bool LoadLayout(FAutomationTestBase& Test, FArenaLayout& Layout)
{
	FString Error;
	return Test.TestTrue(FString::Printf(TEXT("the arena layout loads (%s)"), *Error), Layout.LoadFromFile(FArenaLayout::DefaultFilePath(), Error));
}

TSharedPtr<FJsonObject> LoadStyle(const TCHAR* Id)
{
	FString Path = FPaths::Combine(FPaths::ProjectDir(), FString::Printf(TEXT("../data/vr/styles/style-%s.json"), Id));
	FPaths::CollapseRelativeDirectories(Path);
	FString Json;
	TSharedPtr<FJsonObject> Style;
	if (!FFileHelper::LoadFileToString(Json, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Style))
	{
		return nullptr;
	}
	return Style;
}

// HSV saturation: 1 - min / max.
float Saturation(const FLinearColor& Colour)
{
	const float Most = FMath::Max3(Colour.R, Colour.G, Colour.B);
	return Most > 1.0e-4f ? 1.f - FMath::Min3(Colour.R, Colour.G, Colour.B) / Most : 0.f;
}

/**
 * What is wrong with Tints under Palette, one line each. A tint must be dimmer than every threat colour but the black core
 * (dark by definition: the leave cue's other half, held by chroma and by the brightest-channel floor), less saturated than
 * every chromatic threat (steel and the core are the achromatic classes, held by chroma), at least
 * BrazierFire::ThreatChromaMargin from every threat colour by chroma, and not a black core itself.
 */
TArray<FString> TintViolations(const FElementPalette& Palette, TConstArrayView<FLinearColor> Tints, const FString& What)
{
	struct FThreat
	{
		const TCHAR* Name;
		FLinearColor Colour;
		bool bAchromatic;
		bool bCore;
	};
	const FThreat Threats[] = {{TEXT("water"), Palette.Water, false, false}, {TEXT("fire"), Palette.Fire, false, false},
	    {TEXT("air"), Palette.Air, false, false}, {TEXT("earth"), Palette.Earth, false, false}, {TEXT("steel"), Palette.Steel, true, false},
	    {TEXT("unblockable rim"), Palette.UnblockableRim, false, false}, {TEXT("unblockable core"), Palette.UnblockableBody, true, true}};
	float DimmestLuminance = TNumericLimits<float>::Max();
	float LeastSaturation = TNumericLimits<float>::Max();
	for (const FThreat& Threat : Threats)
	{
		if (!Threat.bCore)
		{
			DimmestLuminance = FMath::Min(DimmestLuminance, Threat.Colour.GetLuminance());
		}
		if (!Threat.bAchromatic)
		{
			LeastSaturation = FMath::Min(LeastSaturation, Saturation(Threat.Colour));
		}
	}
	TArray<FString> Out;
	for (const FLinearColor& Tint : Tints)
	{
		const FString Name = FString::Printf(TEXT("%s tint %s"), *What, *Tint.ToString());
		if (Tint.GetLuminance() >= DimmestLuminance)
		{
			Out.Add(FString::Printf(TEXT("%s luminance %.4f is not under the dimmest threat's %.4f"), *Name, Tint.GetLuminance(), DimmestLuminance));
		}
		if (Saturation(Tint) >= LeastSaturation)
		{
			Out.Add(FString::Printf(TEXT("%s saturation %.3f is not under the least saturated chromatic threat's %.3f"), *Name, Saturation(Tint), LeastSaturation));
		}
		if (FMath::Max3(Tint.R, Tint.G, Tint.B) < ArenaCrowd::MinTintBrightest)
		{
			Out.Add(FString::Printf(TEXT("%s is in the black core class (brightest channel under %.2f)"), *Name, ArenaCrowd::MinTintBrightest));
		}
		for (const FThreat& Threat : Threats)
		{
			const float Distance = ArenaCrowd::ChromaDistance(Tint, Threat.Colour);
			if (Distance < BrazierFire::ThreatChromaMargin)
			{
				Out.Add(FString::Printf(TEXT("%s stands %.3f off %s by chroma (margin %.2f)"), *Name, Distance, Threat.Name, BrazierFire::ThreatChromaMargin));
			}
		}
	}
	return Out;
}

struct FCrowdWorld
{
	UWorld* World = nullptr;
	AArenaGreybox* Arena = nullptr;

	bool Open(FAutomationTestBase& Test)
	{
		World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CrowdTests"));
		if (!Test.TestNotNull(TEXT("a world is made"), World))
		{
			return false;
		}
		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		Context.SetCurrentWorld(World);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		// Not built: the test places only the crowd on the actor.
		Arena = World->SpawnActor<AArenaGreybox>(AArenaGreybox::StaticClass(), FTransform::Identity, Params);
		return Test.TestNotNull(TEXT("the arena actor spawns"), Arena);
	}

	void Close()
	{
		if (World)
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World = nullptr;
		}
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaCrowdVariants, "MageArena.Greybox.Crowd.Variants",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaCrowdVariants::RunTest(const FString& Parameters)
{
	// The card table in code is the crowd hand-off's manifest.json, to its printed precision.
	const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("../art/environment-kit/crowd/manifest.json"));
	FString Text;
	TSharedPtr<FJsonObject> Manifest;
	if (!TestTrue(TEXT("crowd manifest.json reads"), FFileHelper::LoadFileToString(Text, *Path) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Manifest) && Manifest.IsValid()))
	{
		return false;
	}
	const TSharedPtr<FJsonObject>* Variants = nullptr;
	if (!TestTrue(TEXT("manifest has variants"), Manifest->TryGetObjectField(TEXT("variants"), Variants)))
	{
		return false;
	}
	TestEqual(TEXT("variant count"), ArenaCrowd::Variants().Num(), (*Variants)->Values.Num());
	for (const ArenaCrowd::FVariant& Variant : ArenaCrowd::Variants())
	{
		const TSharedPtr<FJsonObject>* Row = nullptr;
		if (!TestTrue(FString::Printf(TEXT("%s in the manifest"), Variant.Name), (*Variants)->TryGetObjectField(Variant.Name, Row)))
		{
			continue;
		}
		const TSharedPtr<FJsonObject> Uv = (*Row)->GetObjectField(TEXT("uvRect"));
		const TSharedPtr<FJsonObject> Bounds = (*Row)->GetObjectField(TEXT("boundsM"));
		const double Centre = (Bounds->GetArrayField(TEXT("min"))[1]->AsNumber() + Bounds->GetArrayField(TEXT("max"))[1]->AsNumber()) * 0.5;
		TestEqual(FString::Printf(TEXT("%s height"), Variant.Name), Variant.HeightM, (*Row)->GetNumberField(TEXT("heightM")), 1.0e-4);
		TestEqual(FString::Printf(TEXT("%s width"), Variant.Name), Variant.WidthM, (*Row)->GetNumberField(TEXT("widthM")), 1.0e-4);
		TestEqual(FString::Printf(TEXT("%s centre"), Variant.Name), Variant.CentreRightM, Centre, 1.0e-4);
		TestEqual(FString::Printf(TEXT("%s u0"), Variant.Name), Variant.U0, Uv->GetNumberField(TEXT("u0")), 1.0e-6);
		TestEqual(FString::Printf(TEXT("%s v0"), Variant.Name), Variant.V0, Uv->GetNumberField(TEXT("v0")), 1.0e-6);
		TestEqual(FString::Printf(TEXT("%s u1"), Variant.Name), Variant.U1, Uv->GetNumberField(TEXT("u1")), 1.0e-6);
		TestEqual(FString::Printf(TEXT("%s v1"), Variant.Name), Variant.V1, Uv->GetNumberField(TEXT("v1")), 1.0e-6);
		TestEqual(FString::Printf(TEXT("%s seated"), Variant.Name), Variant.bSeated, (*Row)->GetStringField(TEXT("kind")) == TEXT("seated"));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaCrowdPlacement, "MageArena.Greybox.Crowd.Placement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaCrowdPlacement::RunTest(const FString& Parameters)
{
	FArenaLayout Layout;
	if (!LoadLayout(*this, Layout))
	{
		return false;
	}
	const TArray<ArenaCrowd::FCard> Cards = ArenaCrowd::Place(Layout);
	AddInfo(FString::Printf(TEXT("cards=%d"), Cards.Num()));
	// About the 470 of the hand-off: three rows at the 0.55 m pitch round the oval, four segments left as aisles.
	TestTrue(FString::Printf(TEXT("about 470 cards (%d)"), Cards.Num()), Cards.Num() >= 420 && Cards.Num() <= 560);

	const int32 Segments = FMath::Max(Layout.WallSegments, 3);
	int32 PerVariant[4] = {0, 0, 0, 0};
	int32 Seated[3] = {0, 0, 0};
	int32 PerTier[3] = {0, 0, 0};
	int32 Off = 0;
	for (const ArenaCrowd::FCard& Card : Cards)
	{
		++PerVariant[Card.Variant];
		const ArenaStands::FStandBox Box = ArenaStands::StandBox(Layout, Card.Segment, Card.Tier);
		const FVector Local = Card.FeetM - Box.MidM;
		const double Along = FVector::DotProduct(Local, Box.Along);
		const double Out = FVector::DotProduct(FVector(Local.X, Local.Y, 0.0), Box.Outward);
		// The feet on the tread top (+-1 cm), inside the drawn box in plan, never in an aisle.
		const bool bOnTop = FMath::Abs(Card.FeetM.Z - ArenaStands::TierTopM(Layout, Card.Tier)) <= 0.01;
		const bool bInside = FMath::Abs(Along) <= Box.RowLengthM * 1.04 * 0.5 && FMath::Abs(Out) <= Box.DepthM * 0.5;
		const bool bInAisle = ArenaCrowd::IsAisle(Card.Segment, Segments);
		// Facing the arena centre, within the yaw jitter.
		const FVector Facing = FRotator(0.0, Card.YawDeg, 0.0).Vector();
		const FVector ToCentre = (Layout.ArenaCentreM - Card.FeetM).GetSafeNormal2D();
		const bool bFaces = FVector::DotProduct(Facing, ToCentre) >= FMath::Cos(FMath::DegreesToRadians(ArenaCrowd::YawJitterDeg * 0.5 + 0.01));
		// The instance carries its variant's atlas rect.
		const ArenaCrowd::FVariant& Variant = ArenaCrowd::Variants()[Card.Variant];
		const float Top = static_cast<float>(1.0 - Variant.V1);
		const float Bottom = static_cast<float>(1.0 - Variant.V0);
		const bool bRect = FMath::IsNearlyEqual(Card.Rect[0], static_cast<float>(Variant.U0)) && FMath::IsNearlyEqual(Card.Rect[2], static_cast<float>(Variant.U1)) && FMath::IsNearlyEqual(FMath::Min(Card.Rect[1], Card.Rect[3]), Top) && FMath::IsNearlyEqual(FMath::Max(Card.Rect[1], Card.Rect[3]), Bottom);
		if (!bOnTop || !bInside || bInAisle || !bFaces || !bRect)
		{
			++Off;
			if (Off <= 5)
			{
				AddError(FString::Printf(TEXT("card seg %d tier %d at %s: top=%d inside=%d aisle=%d faces=%d rect=%d"), Card.Segment, Card.Tier, *Card.FeetM.ToString(),
				    bOnTop, bInside, bInAisle, bFaces, bRect));
			}
		}
		if (Card.Tier >= 0 && Card.Tier < 3)
		{
			++PerTier[Card.Tier];
			Seated[Card.Tier] += Variant.bSeated ? 1 : 0;
		}
	}
	TestEqual(TEXT("every card on a tier top, inside its box, facing the centre"), Off, 0);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		TestTrue(FString::Printf(TEXT("variant %s is used"), ArenaCrowd::Variants()[Index].Name), PerVariant[Index] > 0);
	}
	for (int32 Tier = 0; Tier < 3; ++Tier)
	{
		TestTrue(FString::Printf(TEXT("tier %d holds cards"), Tier), PerTier[Tier] > 0);
	}
	// crowd.py's mix: more sit at the front, more stand at the back.
	if (PerTier[0] > 0 && PerTier[2] > 0)
	{
		TestTrue(TEXT("the front tier sits more than the back tier"), static_cast<double>(Seated[0]) / PerTier[0] > static_cast<double>(Seated[2]) / PerTier[2]);
	}
	AddInfo(FString::Printf(TEXT("per variant %d %d %d %d; per tier %d %d %d"), PerVariant[0], PerVariant[1], PerVariant[2], PerVariant[3], PerTier[0], PerTier[1], PerTier[2]));

	// A fixed seed: the same layout places the same crowd.
	const TArray<ArenaCrowd::FCard> Again = ArenaCrowd::Place(Layout);
	bool bSame = Again.Num() == Cards.Num();
	for (int32 Index = 0; bSame && Index < Cards.Num(); ++Index)
	{
		bSame = Again[Index].Variant == Cards[Index].Variant && Again[Index].Transform.Equals(Cards[Index].Transform, 1.0e-6);
	}
	TestTrue(TEXT("placement is deterministic"), bSame);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaCrowdCard, "MageArena.Greybox.Crowd.Card",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaCrowdCard::RunTest(const FString& Parameters)
{
	// The card is the engine plane: two triangles, 100 cm square on Z = 0, and its UV axes are the ones Place assumes.
	const UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, ArenaCrowd::CardMeshPath);
	const FStaticMeshRenderData* RenderData = Plane ? Plane->GetRenderData() : nullptr;
	if (!TestTrue(TEXT("the engine plane has render data"), RenderData && RenderData->LODResources.Num() > 0))
	{
		return false;
	}
	const FStaticMeshLODResources& Lod = RenderData->LODResources[0];
	TestEqual(TEXT("the plane is two triangles"), Lod.GetNumTriangles(), 2);
	TestEqual(TEXT("the plane has one section"), Lod.Sections.Num(), 1);
	const int32 Vertices = Lod.VertexBuffers.PositionVertexBuffer.GetNumVertices();
	double UX = 0.0, UY = 0.0, VX = 0.0, VY = 0.0, XX = 0.0, YY = 0.0, MaxZ = 0.0, MaxXY = 0.0;
	for (int32 Index = 0; Index < Vertices; ++Index)
	{
		const FVector3f P = Lod.VertexBuffers.PositionVertexBuffer.VertexPosition(Index);
		const FVector2f Uv = Lod.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(Index, 0);
		AddInfo(FString::Printf(TEXT("plane vertex %d at %s uv %s"), Index, *P.ToString(), *Uv.ToString()));
		UX += (Uv.X - 0.5) * P.X;
		UY += (Uv.X - 0.5) * P.Y;
		VX += (Uv.Y - 0.5) * P.X;
		VY += (Uv.Y - 0.5) * P.Y;
		XX += P.X * P.X;
		YY += P.Y * P.Y;
		MaxZ = FMath::Max(MaxZ, FMath::Abs(static_cast<double>(P.Z)));
		MaxXY = FMath::Max3(MaxXY, FMath::Abs(static_cast<double>(P.X)), FMath::Abs(static_cast<double>(P.Y)));
	}
	TestEqual(TEXT("the plane is flat"), MaxZ, 0.0, 0.01);
	TestEqual(TEXT("the plane is 100 cm"), MaxXY, 50.0, 0.01);
	const FVector U(XX > 0.0 ? UX / XX * 100.0 : 0.0, YY > 0.0 ? UY / YY * 100.0 : 0.0, 0.0);
	const FVector V(XX > 0.0 ? VX / XX * 100.0 : 0.0, YY > 0.0 ? VY / YY * 100.0 : 0.0, 0.0);
	AddInfo(FString::Printf(TEXT("plane U per metre %s, V per metre %s"), *U.ToString(), *V.ToString()));
	TestTrue(FString::Printf(TEXT("U rises along PlaneUAxis (measured %s)"), *U.ToString()), U.Equals(ArenaCrowd::PlaneUAxis(), 0.01));
	TestTrue(FString::Printf(TEXT("V rises along PlaneVAxis (measured %s)"), *V.ToString()), V.Equals(ArenaCrowd::PlaneVAxis(), 0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaCrowdOneBatch, "MageArena.Greybox.Crowd.OneBatch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaCrowdOneBatch::RunTest(const FString& Parameters)
{
	FArenaLayout Layout;
	if (!LoadLayout(*this, Layout))
	{
		return false;
	}
	FCrowdWorld Rig;
	if (!Rig.Open(*this))
	{
		Rig.Close();
		return false;
	}
	TArray<UInstancedStaticMeshComponent*> Before;
	Rig.Arena->GetComponents(Before);
	UInstancedStaticMeshComponent* Crowd = ArenaCrowd::Build(Rig.Arena, Layout, false);
	TArray<UInstancedStaticMeshComponent*> After;
	Rig.Arena->GetComponents(After);
	if (TestNotNull(TEXT("the crowd builds from the committed assets"), Crowd))
	{
		// One component of one single-section mesh with one material: one mesh draw command per pass, whatever the count.
		TestEqual(TEXT("the crowd adds exactly one instanced component"), After.Num() - Before.Num(), 1);
		// T65: the banners are instances of this one component too.
		const int32 Banners = ArenaCrowd::PlaceBanners(Layout).Num();
		TestTrue(TEXT("the stands carry banners"), Banners > 0);
		TestEqual(TEXT("one instance per card and banner"), Crowd->GetInstanceCount(), ArenaCrowd::Place(Layout).Num() + Banners);
		TestEqual(TEXT("eight custom floats per instance (the atlas rect, the tint, the flat flag)"), Crowd->NumCustomDataFloats, 8);
		TestEqual(TEXT("the code says eight"), ArenaCrowd::CustomDataFloats, 8);
		TestEqual(TEXT("one material slot"), Crowd->GetNumMaterials(), 1);
		const UStaticMesh* Mesh = Crowd->GetStaticMesh();
		const FStaticMeshRenderData* RenderData = Mesh ? Mesh->GetRenderData() : nullptr;
		if (TestTrue(TEXT("the card has render data"), RenderData && RenderData->LODResources.Num() > 0))
		{
			TestEqual(TEXT("the card has one LOD"), RenderData->LODResources.Num(), 1);
			TestEqual(TEXT("the card has one section"), RenderData->LODResources[0].Sections.Num(), 1);
			TestEqual(TEXT("the card is two triangles"), RenderData->LODResources[0].GetNumTriangles(), 2);
		}
		TestFalse(TEXT("the crowd casts no shadow"), Crowd->CastShadow);
		TestEqual(TEXT("the crowd has no collision"), static_cast<int32>(Crowd->GetCollisionEnabled()), static_cast<int32>(ECollisionEnabled::NoCollision));
		const UMaterial* Material = Crowd->GetMaterial(0) ? Crowd->GetMaterial(0)->GetMaterial() : nullptr;
		if (TestNotNull(TEXT("the crowd material"), Material))
		{
			TestEqual(TEXT("material is M_Crowd_Atlas"), Material->GetName(), FString(ArenaCrowd::MaterialName));
			TestEqual(TEXT("masked, not translucent"), static_cast<int32>(Material->GetBlendMode()), static_cast<int32>(BLEND_Masked));
			TestEqual(TEXT("clip 0.5"), Material->GetOpacityMaskClipValue(), 0.5f);
			TestFalse(TEXT("one-sided"), Material->IsTwoSided());
			TestTrue(TEXT("lit like the greybox"), Material->GetShadingModels().HasShadingModel(MSM_DefaultLit));
			TestTrue(TEXT("usable on instanced meshes"), Material->GetUsageByFlag(MATUSAGE_InstancedStaticMeshes));
		}
	}
	Rig.Close();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaCrowdTintNotThreat, "MageArena.Greybox.Crowd.TintNotThreat",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaCrowdTintNotThreat::RunTest(const FString& Parameters)
{
	// Every tint the crowd can carry: the tunic palette, the banner, and the tints the placed instances hold.
	FArenaLayout Layout;
	if (!LoadLayout(*this, Layout))
	{
		return false;
	}
	TArray<FLinearColor> Tints(ArenaCrowd::TunicPalette());
	Tints.Add(ArenaCrowd::BannerColour());
	TestTrue(TEXT("several tunic colours, so the crowd is varied"), ArenaCrowd::TunicPalette().Num() >= 5);
	TArray<ArenaCrowd::FCard> All = ArenaCrowd::Place(Layout);
	const int32 Figures = All.Num();
	All.Append(ArenaCrowd::PlaceBanners(Layout));
	TestTrue(TEXT("the stands carry some banners"), All.Num() > Figures);
	int32 Strays = 0;
	int32 Used = 0;
	TSet<int32> UsedTints;
	for (int32 Index = 0; Index < All.Num(); ++Index)
	{
		const FLinearColor Tint(All[Index].Tint[0], All[Index].Tint[1], All[Index].Tint[2]);
		const bool bBanner = Index >= Figures;
		TestEqual(TEXT("only banners are flat"), All[Index].Flat > 0.5f, bBanner);
		const int32 Found = ArenaCrowd::TunicPalette().IndexOfByPredicate([&Tint](const FLinearColor& Each)
		{ return Each.Equals(Tint, 0.f); });
		const bool bKnown = bBanner ? Tint.Equals(ArenaCrowd::BannerColour(), 0.f) : Found != INDEX_NONE;
		Strays += bKnown ? 0 : 1;
		if (!bBanner && Found != INDEX_NONE)
		{
			UsedTints.Add(Found);
			++Used;
		}
	}
	TestEqual(TEXT("every instance carries a palette tint"), Strays, 0);
	TestEqual(TEXT("every tunic colour is used"), UsedTints.Num(), ArenaCrowd::TunicPalette().Num());
	AddInfo(FString::Printf(TEXT("tunics=%d banners=%d tint colours used=%d"), Used, All.Num() - Figures, UsedTints.Num()));

	struct FCase
	{
		FString Name;
		FElementPalette Palette;
	};
	TArray<FCase> Cases;
	Cases.Add({TEXT("greybox"), FElementPalette::Greybox()});
	for (const TCHAR* Id : {TEXT("13"), TEXT("25"), TEXT("30")})
	{
		const TSharedPtr<FJsonObject> Style = LoadStyle(Id);
		if (!TestTrue(FString::Printf(TEXT("style %s loads"), Id), Style.IsValid()))
		{
			continue;
		}
		FCase Case{FString::Printf(TEXT("style %s"), Id), FElementPalette::Greybox()};
		TestEqual(FString::Printf(TEXT("style %s sets all seven threat colours"), Id), ApplyStylePalette(Case.Palette, *Style), 7);
		Cases.Add(Case);
	}
	for (const FCase& Case : Cases)
	{
		const TArray<FString> Violations = TintViolations(Case.Palette, Tints, Case.Name);
		for (const FString& Line : Violations)
		{
			AddError(Line);
		}
		TestEqual(FString::Printf(TEXT("%s: no tunic or banner tint reaches a threat colour"), *Case.Name), Violations.Num(), 0);

		// The negative controls: the checks do fail on a tint that does. An over-bright orange, an over-saturated red,
		// and a steel grey, each under every palette.
		const FLinearColor Bad[] = {FLinearColor(0.60f, 0.20f, 0.05f), FLinearColor(0.09f, 0.0f, 0.0f), FLinearColor(0.04f, 0.04f, 0.04f)};
		for (const FLinearColor& Tint : Bad)
		{
			TestTrue(FString::Printf(TEXT("%s: the checks catch the tint %s"), *Case.Name, *Tint.ToString()), TintViolations(Case.Palette, MakeArrayView(&Tint, 1), Case.Name).Num() > 0);
		}
		bool bCaughtByLuminance = false;
		for (const FString& Line : TintViolations(Case.Palette, MakeArrayView(&Bad[0], 1), Case.Name))
		{
			bCaughtByLuminance |= Line.Contains(TEXT("luminance"));
		}
		TestTrue(FString::Printf(TEXT("%s: the checks catch an over-bright tint by luminance"), *Case.Name), bCaughtByLuminance);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaCrowdSwitchOff, "MageArena.Greybox.Crowd.SwitchOff",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaCrowdSwitchOff::RunTest(const FString& Parameters)
{
	FArenaLayout Layout;
	if (!LoadLayout(*this, Layout))
	{
		return false;
	}
	FCrowdWorld Rig;
	if (!Rig.Open(*this))
	{
		Rig.Close();
		return false;
	}
	// -MageArenaNoCrowd: nothing is added, so the stands stay the bare tiers.
	TArray<UActorComponent*> Before;
	Rig.Arena->GetComponents(Before);
	TestNull(TEXT("no crowd with the switch set"), ArenaCrowd::Build(Rig.Arena, Layout, true));
	TArray<UActorComponent*> After;
	Rig.Arena->GetComponents(After);
	TestEqual(TEXT("the switch adds no component"), After.Num(), Before.Num());
	Rig.Close();
	return true;
}

#endif
