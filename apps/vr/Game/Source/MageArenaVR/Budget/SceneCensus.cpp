#include "Budget/SceneCensus.h"

#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Greybox/GreyboxUtil.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Misc/SecureHash.h"

namespace
{
constexpr float CensusColourTolerance = 1.0e-3f;

const TCHAR* PassName(ECensusPass Pass)
{
	return Pass == ECensusPass::Translucent ? TEXT("translucent") : TEXT("opaque");
}

FString HashLine(const FCensusRow& Row)
{
	return FString::Printf(TEXT("%s|%s|%s|%s|%d"), *Row.Key.Mesh, *Row.Key.ColourText(), *Row.Key.Material, PassName(Row.Key.Pass), Row.Count);
}
}

bool FCensusKey::SameKind(const FCensusKey& Other) const
{
	return Mesh == Other.Mesh && Material == Other.Material && Pass == Other.Pass;
}

bool FCensusKey::Matches(const FCensusKey& Other) const
{
	if (!SameKind(Other) || bTinted != Other.bTinted)
	{
		return false;
	}
	return !bTinted || Colour.Equals(Other.Colour, CensusColourTolerance);
}

FString FCensusKey::ColourText() const
{
	return bTinted ? FString::Printf(TEXT("(%.3f, %.3f, %.3f, %.3f)"), Colour.R, Colour.G, Colour.B, Colour.A) : FString(TEXT("(textured)"));
}

FString FCensusKey::ToString() const
{
	return FString::Printf(TEXT("%s %s %s %s"), *Mesh, *ColourText(), *Material, PassName(Pass));
}

void FSceneCensus::Add(const FCensusKey& Key, int32 Count, const FString& Role)
{
	for (FCensusRow& Row : Rows)
	{
		if (Row.Key.Matches(Key))
		{
			Row.Count += Count;
			if (!Role.IsEmpty() && !Row.Role.Contains(Role))
			{
				Row.Role = Row.Role.IsEmpty() ? Role : Row.Role + TEXT(", ") + Role;
			}
			return;
		}
	}
	FCensusRow& Created = Rows.AddDefaulted_GetRef();
	Created.Key = Key;
	Created.Count = Count;
	Created.Role = Role;
}

int32 FSceneCensus::CountOf(const FCensusKey& Key) const
{
	int32 Count = 0;
	for (const FCensusRow& Row : Rows)
	{
		Count += Row.Key.Matches(Key) ? Row.Count : 0;
	}
	return Count;
}

int32 FSceneCensus::Parts() const
{
	int32 Count = 0;
	for (const FCensusRow& Row : Rows)
	{
		Count += Row.Count;
	}
	return Count;
}

bool FSceneCensus::HasPass(ECensusPass Pass) const
{
	return Rows.ContainsByPredicate([Pass](const FCensusRow& Row)
	{ return Row.Key.Pass == Pass && Row.Count > 0; });
}

FString FSceneCensus::Hash() const
{
	TArray<FString> Lines;
	for (const FCensusRow& Row : Rows)
	{
		Lines.Add(HashLine(Row));
	}
	Lines.Sort();
	return FMD5::HashAnsiString(*FString::Join(Lines, TEXT("\n")));
}

TSharedRef<FJsonObject> FSceneCensus::ToJson() const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("parts"), Parts());
	Root->SetNumberField(TEXT("tuples"), Rows.Num());
	Root->SetStringField(TEXT("hash"), Hash());
	TArray<TSharedPtr<FJsonValue>> Out;
	for (const FCensusRow& Row : Rows)
	{
		TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("mesh"), Row.Key.Mesh);
		Item->SetStringField(TEXT("colour"), Row.Key.ColourText());
		Item->SetStringField(TEXT("material"), Row.Key.Material);
		Item->SetStringField(TEXT("pass"), PassName(Row.Key.Pass));
		Item->SetNumberField(TEXT("count"), Row.Count);
		if (!Row.Role.IsEmpty())
		{
			Item->SetStringField(TEXT("role"), Row.Role);
		}
		Out.Add(MakeShared<FJsonValueObject>(Item));
	}
	Root->SetArrayField(TEXT("rows"), Out);
	return Root;
}

FSceneParity CompareSceneCensus(const FSceneCensus& Planned, const FSceneCensus& Drawn)
{
	FSceneParity Parity;
	for (const FCensusRow& Row : Planned.Rows)
	{
		const int32 Have = Drawn.CountOf(Row.Key);
		if (Have >= Row.Count)
		{
			continue;
		}
		// The drawn colours of this kind that no planned row holds: a part drawn in another colour shows up here.
		TArray<FString> Unplanned;
		for (const FCensusRow& Other : Drawn.Rows)
		{
			const bool bPlanned = Planned.Rows.ContainsByPredicate([&Other](const FCensusRow& Plan)
			{ return Plan.Key.Matches(Other.Key); });
			if (Other.Key.SameKind(Row.Key) && !bPlanned)
			{
				Unplanned.Add(FString::Printf(TEXT("%s x%d"), *Other.Key.ColourText(), Other.Count));
			}
		}
		Parity.Divergences.Add(FString::Printf(TEXT("%s: planned %d x %s, drawn %d; drawn %s %s %s not planned: %s"),
		    Row.Role.IsEmpty() ? TEXT("part") : *Row.Role, Row.Count, *Row.Key.ToString(), Have, *Row.Key.Mesh, *Row.Key.Material, PassName(Row.Key.Pass),
		    Unplanned.Num() ? *FString::Join(Unplanned, TEXT(", ")) : TEXT("none")));
	}
	TArray<ECensusPass> Passes = Planned.RequiredPasses;
	for (const FCensusRow& Row : Planned.Rows)
	{
		Passes.AddUnique(Row.Key.Pass);
	}
	for (const ECensusPass Pass : Passes)
	{
		if (!Drawn.HasPass(Pass))
		{
			Parity.Divergences.Add(FString::Printf(TEXT("missing %s pass: the plan requires it and no drawn part is in it"), PassName(Pass)));
		}
	}
	for (const FCensusRow& Row : Drawn.Rows)
	{
		if (!Planned.Rows.ContainsByPredicate([&Row](const FCensusRow& Plan)
		{ return Plan.Key.Matches(Row.Key); }))
		{
			Parity.Extras.Add(Row);
		}
	}
	Parity.bOk = Parity.Divergences.Num() == 0;
	return Parity;
}

FCensusKey OpaquePartKey(const TCHAR* Mesh, const FLinearColor& Colour)
{
	FCensusKey Key;
	Key.Mesh = Mesh;
	Key.Colour = Colour;
	const UMaterialInterface* Base = Greybox::UnlitOpaqueMaterial();
	const UMaterial* Root = Base ? Base->GetMaterial() : nullptr;
	Key.Material = Root ? Root->GetName() : FString(TEXT("LevelColorationUnlitMaterial"));
	Key.Pass = ECensusPass::Opaque;
	return Key;
}

bool CensusKeyOfPart(const UStaticMeshComponent* Part, FCensusKey& OutKey)
{
	const UStaticMesh* Mesh = Part ? Part->GetStaticMesh() : nullptr;
	const UMaterialInterface* Material = Part ? Part->GetMaterial(0) : nullptr;
	if (!Mesh || !Material)
	{
		return false;
	}
	OutKey = FCensusKey();
	OutKey.Mesh = Mesh->GetName();
	const UMaterial* Root = Material->GetMaterial();
	OutKey.Material = Root ? Root->GetName() : Material->GetName();
	const EBlendMode Blend = Material->GetBlendMode();
	OutKey.Pass = (Blend == BLEND_Opaque || Blend == BLEND_Masked) ? ECensusPass::Opaque : ECensusPass::Translucent;
	// Greybox::Tint writes every vector parameter with the part's colour; prefer Color, else the first one.
	TArray<FMaterialParameterInfo> Infos;
	TArray<FGuid> Ids;
	Material->GetAllVectorParameterInfo(Infos, Ids);
	const FMaterialParameterInfo* Info = Infos.FindByPredicate([](const FMaterialParameterInfo& Each)
	{ return Each.Name == TEXT("Color"); });
	if (!Info && Infos.Num() > 0)
	{
		Info = &Infos[0];
	}
	OutKey.bTinted = Info && Material->GetVectorParameterValue(FHashedMaterialParameterInfo(*Info), OutKey.Colour);
	if (!OutKey.bTinted)
	{
		OutKey.Colour = FLinearColor::Transparent;
	}
	return true;
}

void AddDrawnParts(FSceneCensus& Census, const AActor& Actor)
{
	TInlineComponentArray<UStaticMeshComponent*> Parts(&Actor);
	for (const UStaticMeshComponent* Part : Parts)
	{
		FCensusKey Key;
		if (Part && Part->IsRegistered() && Part->IsVisible() && CensusKeyOfPart(Part, Key))
		{
			Census.Add(Key);
		}
	}
}
