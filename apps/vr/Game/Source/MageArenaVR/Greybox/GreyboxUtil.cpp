#include "Greybox/GreyboxUtil.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MageArenaVR.h"

UStaticMesh* Greybox::LoadShape(const TCHAR* ShapeName)
{
	const FString Path = FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), ShapeName, ShapeName);
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path);
	if (!Mesh)
	{
		UE_LOG(LogMageArena, Error, TEXT("Basic shape missing: %s"), *Path);
	}
	return Mesh;
}

UMaterialInterface* Greybox::BasicMaterial()
{
	return LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
}

UMaterialInterface* Greybox::TranslucentMaterial()
{
	return LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Engine/EngineDebugMaterials/M_SimpleUnlitTranslucent.M_SimpleUnlitTranslucent"));
}

UMaterialInterface* Greybox::UnlitOpaqueMaterial()
{
	// LevelColorationUnlitMaterial is opaque, unlit, and exposes a Color vector parameter.
	// It is not marked for instanced static meshes, so callers use one component per shape.
	return LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Engine/EngineDebugMaterials/LevelColorationUnlitMaterial.LevelColorationUnlitMaterial"));
}

UMaterialInstanceDynamic* Greybox::Tint(UMaterialInterface* Base, UObject* Outer, const FLinearColor& Colour)
{
	if (!Base || !Outer)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Base, Outer);
	if (!Instance)
	{
		return nullptr;
	}
	TArray<FMaterialParameterInfo> Vectors;
	TArray<FGuid> VectorIds;
	Instance->GetAllVectorParameterInfo(Vectors, VectorIds);
	for (const FMaterialParameterInfo& Info : Vectors)
	{
		Instance->SetVectorParameterValueByInfo(Info, Colour);
	}
	Instance->SetVectorParameterValue(TEXT("Color"), Colour);

	TArray<FMaterialParameterInfo> Scalars;
	TArray<FGuid> ScalarIds;
	Instance->GetAllScalarParameterInfo(Scalars, ScalarIds);
	for (const FMaterialParameterInfo& Info : Scalars)
	{
		const FString Name = Info.Name.ToString();
		if (Name.Contains(TEXT("Opacity")))
		{
			Instance->SetScalarParameterValueByInfo(Info, Colour.A);
		}
		else if (Name.Contains(TEXT("Roughness")))
		{
			// Matte, so the sky cubemap cannot print a grid across the greybox.
			Instance->SetScalarParameterValueByInfo(Info, 1.f);
		}
		else if (Name.Contains(TEXT("Specular")))
		{
			Instance->SetScalarParameterValueByInfo(Info, 0.f);
		}
	}
	return Instance;
}

UInstancedStaticMeshComponent* Greybox::MakeInstances(AActor* Owner, UStaticMesh* Mesh, UMaterialInterface* Material)
{
	if (!Owner || !Mesh)
	{
		return nullptr;
	}
	UInstancedStaticMeshComponent* Instances = NewObject<UInstancedStaticMeshComponent>(Owner);
	Instances->SetStaticMesh(Mesh);
	if (Material)
	{
		Instances->SetMaterial(0, Material);
	}
	Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Instances->SetCastShadow(false);
	Instances->SetMobility(EComponentMobility::Movable);
	Instances->SetupAttachment(Owner->GetRootComponent());
	Instances->RegisterComponent();
	Owner->AddInstanceComponent(Instances);
	return Instances;
}

UStaticMeshComponent* Greybox::MakeMesh(AActor* Owner, UStaticMesh* Mesh, UMaterialInterface* Material)
{
	if (!Owner || !Mesh)
	{
		return nullptr;
	}
	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Owner);
	Component->SetStaticMesh(Mesh);
	if (Material)
	{
		Component->SetMaterial(0, Material);
	}
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCastShadow(false);
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetupAttachment(Owner->GetRootComponent());
	Component->RegisterComponent();
	Owner->AddInstanceComponent(Component);
	return Component;
}

void Greybox::AddSized(UInstancedStaticMeshComponent* Instances, const FVector& CentreCm, const FQuat& Rotation, const FVector& FullSizeCm)
{
	if (!Instances || !Instances->GetStaticMesh())
	{
		return;
	}
	const FVector Extent = Instances->GetStaticMesh()->GetBounds().BoxExtent;
	const FVector Scale(
		FullSizeCm.X / FMath::Max(Extent.X * 2.0, 0.01),
		FullSizeCm.Y / FMath::Max(Extent.Y * 2.0, 0.01),
		FullSizeCm.Z / FMath::Max(Extent.Z * 2.0, 0.01));
	Instances->AddInstance(FTransform(Rotation, CentreCm, Scale));
}

void Greybox::SetSized(UStaticMeshComponent* MeshComponent, const FVector& FullSizeCm)
{
	if (!MeshComponent || !MeshComponent->GetStaticMesh())
	{
		return;
	}
	const FVector Extent = MeshComponent->GetStaticMesh()->GetBounds().BoxExtent;
	MeshComponent->SetRelativeScale3D(FVector(
		FullSizeCm.X / FMath::Max(Extent.X * 2.0, 0.01),
		FullSizeCm.Y / FMath::Max(Extent.Y * 2.0, 0.01),
		FullSizeCm.Z / FMath::Max(Extent.Z * 2.0, 0.01)));
}
