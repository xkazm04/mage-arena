#include "Greybox/HandPresentationComponent.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Gestures/SigilRecognizerSubsystem.h"
#include "Gestures/WardDetector.h"
#include "Greybox/GreyboxUtil.h"
#include "Hands/HandFrame.h"
#include "Hands/HandInputSubsystem.h"
#include "Hands/MageArenaPawn.h"
#include "HeadMountedDisplayTypes.h"
#include "MageArenaVR.h"
#include "Misc/Parse.h"

namespace
{
const int32 BoneLinks[][2] = {
	{ static_cast<int32>(EHandKeypoint::Wrist), static_cast<int32>(EHandKeypoint::Palm) },
	{ static_cast<int32>(EHandKeypoint::Wrist), static_cast<int32>(EHandKeypoint::ThumbMetacarpal) },
	{ static_cast<int32>(EHandKeypoint::ThumbMetacarpal), static_cast<int32>(EHandKeypoint::ThumbProximal) },
	{ static_cast<int32>(EHandKeypoint::ThumbProximal), static_cast<int32>(EHandKeypoint::ThumbDistal) },
	{ static_cast<int32>(EHandKeypoint::ThumbDistal), static_cast<int32>(EHandKeypoint::ThumbTip) },
	{ static_cast<int32>(EHandKeypoint::Wrist), static_cast<int32>(EHandKeypoint::IndexMetacarpal) },
	{ static_cast<int32>(EHandKeypoint::IndexMetacarpal), static_cast<int32>(EHandKeypoint::IndexProximal) },
	{ static_cast<int32>(EHandKeypoint::IndexProximal), static_cast<int32>(EHandKeypoint::IndexIntermediate) },
	{ static_cast<int32>(EHandKeypoint::IndexIntermediate), static_cast<int32>(EHandKeypoint::IndexDistal) },
	{ static_cast<int32>(EHandKeypoint::IndexDistal), static_cast<int32>(EHandKeypoint::IndexTip) },
	{ static_cast<int32>(EHandKeypoint::Wrist), static_cast<int32>(EHandKeypoint::MiddleMetacarpal) },
	{ static_cast<int32>(EHandKeypoint::MiddleMetacarpal), static_cast<int32>(EHandKeypoint::MiddleProximal) },
	{ static_cast<int32>(EHandKeypoint::MiddleProximal), static_cast<int32>(EHandKeypoint::MiddleIntermediate) },
	{ static_cast<int32>(EHandKeypoint::MiddleIntermediate), static_cast<int32>(EHandKeypoint::MiddleDistal) },
	{ static_cast<int32>(EHandKeypoint::MiddleDistal), static_cast<int32>(EHandKeypoint::MiddleTip) },
	{ static_cast<int32>(EHandKeypoint::Wrist), static_cast<int32>(EHandKeypoint::RingMetacarpal) },
	{ static_cast<int32>(EHandKeypoint::RingMetacarpal), static_cast<int32>(EHandKeypoint::RingProximal) },
	{ static_cast<int32>(EHandKeypoint::RingProximal), static_cast<int32>(EHandKeypoint::RingIntermediate) },
	{ static_cast<int32>(EHandKeypoint::RingIntermediate), static_cast<int32>(EHandKeypoint::RingDistal) },
	{ static_cast<int32>(EHandKeypoint::RingDistal), static_cast<int32>(EHandKeypoint::RingTip) },
	{ static_cast<int32>(EHandKeypoint::Wrist), static_cast<int32>(EHandKeypoint::LittleMetacarpal) },
	{ static_cast<int32>(EHandKeypoint::LittleMetacarpal), static_cast<int32>(EHandKeypoint::LittleProximal) },
	{ static_cast<int32>(EHandKeypoint::LittleProximal), static_cast<int32>(EHandKeypoint::LittleIntermediate) },
	{ static_cast<int32>(EHandKeypoint::LittleIntermediate), static_cast<int32>(EHandKeypoint::LittleDistal) },
	{ static_cast<int32>(EHandKeypoint::LittleDistal), static_cast<int32>(EHandKeypoint::LittleTip) },
};

}

UHandPresentationComponent::UHandPresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UHandPresentationComponent::BeginPlay()
{
	Super::BeginPlay();
	if (const AMageArenaPawn* Pawn = Cast<AMageArenaPawn>(GetOwner()))
	{
		if (Pawn->HasLayout())
		{
			Layout = Pawn->GetLayout();
			bReady = true;
		}
	}
	if (!bReady)
	{
		FString Error;
		bReady = Layout.LoadFromFile(FArenaLayout::DefaultFilePath(), Error);
		if (!bReady)
		{
			UE_LOG(LogMageArena, Error, TEXT("Hand presentation layout failed: %s"), *Error);
		}
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
	{
		SetComponentTickEnabled(false);
		return;
	}

	UStaticMesh* Sphere = Greybox::LoadShape(TEXT("Sphere"));
	UStaticMesh* Cylinder = Greybox::LoadShape(TEXT("Cylinder"));
	UStaticMesh* Cube = Greybox::LoadShape(TEXT("Cube"));
	// BasicShapeMaterial is the one engine material that already supports instancing.
	// The unlit translucent debug material does not, and -game substitutes the default grid.
	UMaterialInterface* Lit = Greybox::BasicMaterial();
	UMaterialInterface* ArcMaterial = Greybox::TranslucentMaterial();
	if (!ArcMaterial)
	{
		UE_LOG(LogMageArena, Warning, TEXT("Ward translucent material missing; arc falls back to the lit basic material"));
		ArcMaterial = Lit;
	}
	AActor* Owner = GetOwner();
	FLinearColor Hand = Layout.Hand;
	Hand.A = 1.f;
	FLinearColor Cast = Layout.CastFlash;
	Cast.A = 1.f;
	FLinearColor Reject = Layout.RejectFlash;
	Reject.A = 1.f;
	Joints = Greybox::MakeInstances(Owner, Sphere, Greybox::Tint(Lit, Owner, Hand));
	Bones = Greybox::MakeInstances(Owner, Cylinder, Greybox::Tint(Lit, Owner, Hand));
	CastMaterial = Greybox::Tint(Lit, Owner, Cast);
	RejectMaterial = Greybox::Tint(Lit, Owner, Reject);
	Flash = Greybox::MakeInstances(Owner, Cube, CastMaterial);
	const int32 ArcCount = FMath::Clamp(Layout.WardArcSegments, 3, 24);
	UMaterialInstanceDynamic* ArcInstance = Greybox::Tint(ArcMaterial, Owner, Layout.WardArc);
	for (int32 Index = 0; Index < ArcCount; ++Index)
	{
		UStaticMeshComponent* Segment = Greybox::MakeMesh(Owner, Cube, ArcInstance);
		if (!Segment)
		{
			continue;
		}
		Segment->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
		Segment->SetVisibility(false);
		WardSegments.Add(Segment);
	}
	if (Joints)
	{
		Joints->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
	}
	if (Bones)
	{
		Bones->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
	}
	if (Flash)
	{
		Flash->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
		Flash->SetVisibility(false);
	}
	BindSignals();
}

void UHandPresentationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindSignals();
	Super::EndPlay(EndPlayReason);
}

void UHandPresentationComponent::BindSignals()
{
	UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (!Instance)
	{
		return;
	}
	if (USigilRecognizerSubsystem* Recognizer = Instance->GetSubsystem<USigilRecognizerSubsystem>())
	{
		Sigils = Recognizer;
		CastHandle = Recognizer->OnSigilCast.AddUObject(this, &UHandPresentationComponent::HandleCast);
		RejectHandle = Recognizer->OnSigilRejected.AddUObject(this, &UHandPresentationComponent::HandleReject);
	}
}

void UHandPresentationComponent::UnbindSignals()
{
	if (USigilRecognizerSubsystem* Recognizer = Sigils.Get())
	{
		Recognizer->OnSigilCast.Remove(CastHandle);
		Recognizer->OnSigilRejected.Remove(RejectHandle);
	}
	Sigils.Reset();
}

void UHandPresentationComponent::HandleCast(FName Line, float Score, double LatencyMs)
{
	bFlashOn = true;
	bFlashCast = true;
	FlashLeft = static_cast<float>(Layout.FlashDurationS);
	if (Flash && CastMaterial)
	{
		Flash->SetMaterial(0, CastMaterial);
	}
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_SIGIL_FLASH cast line=%s score=%.3f latency=%.1f"), *Line.ToString(), Score, LatencyMs);
}

void UHandPresentationComponent::HandleReject(double Distance)
{
	bFlashOn = true;
	bFlashCast = false;
	FlashLeft = static_cast<float>(Layout.FlashDurationS);
	if (Flash && RejectMaterial)
	{
		Flash->SetMaterial(0, RejectMaterial);
	}
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_SIGIL_FLASH reject distance=%.3f"), Distance);
}

void UHandPresentationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bReady || FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
	{
		return;
	}
	if (bFlashOn)
	{
		FlashLeft -= DeltaTime;
		if (FlashLeft <= 0.f)
		{
			bFlashOn = false;
		}
	}
	UpdateHands();
	UpdateFlash();
	UpdateWard();
}

void UHandPresentationComponent::UpdateHands()
{
	bHasRightTip = false;
	if (Joints)
	{
		Joints->ClearInstances();
	}
	if (Bones)
	{
		Bones->ClearInstances();
	}
	UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UHandInputSubsystem* Hands = Instance ? Instance->GetSubsystem<UHandInputSubsystem>() : nullptr;
	if (!Hands)
	{
		return;
	}
	const double JointCm = Layout.JointRadiusM * 2.0 * 100.0;
	const double BoneCm = Layout.BoneRadiusM * 2.0 * 100.0;
	const FVector JointSize(JointCm, JointCm, JointCm);
	const EControllerHand Sides[] = { EControllerHand::Left, EControllerHand::Right };
	for (EControllerHand Side : Sides)
	{
		FHandFrame Frame;
		if (!Hands->GetLatest(Side, Frame) || Frame.Joints.Num() < MageHandJointCount)
		{
			continue;
		}
		for (int32 Joint = 0; Joint < MageHandJointCount; ++Joint)
		{
			Greybox::AddSized(Joints, Frame.Joints[Joint].Location, FQuat::Identity, JointSize);
		}
		if (Side == EControllerHand::Right && Frame.Joints.IsValidIndex(static_cast<int32>(EHandKeypoint::IndexTip)))
		{
			RightIndexTip = Frame.Joints[static_cast<int32>(EHandKeypoint::IndexTip)].Location;
			bHasRightTip = true;
		}
		for (int32 LinkIndex = 0; LinkIndex < UE_ARRAY_COUNT(BoneLinks); ++LinkIndex)
		{
			const FVector A = Frame.Joints[BoneLinks[LinkIndex][0]].Location;
			const FVector B = Frame.Joints[BoneLinks[LinkIndex][1]].Location;
			const double Length = FVector::Distance(A, B);
			if (Length < 0.4)
			{
				continue;
			}
			const FVector Direction = (B - A).GetSafeNormal();
			const FQuat Rotation = FQuat::FindBetweenNormals(FVector::UpVector, Direction);
			Greybox::AddSized(Bones, (A + B) * 0.5, Rotation, FVector(BoneCm, BoneCm, Length));
		}
	}
	if (Joints)
	{
		Joints->SetVisibility(Joints->GetInstanceCount() > 0);
	}
	if (Bones)
	{
		Bones->SetVisibility(Bones->GetInstanceCount() > 0);
	}
}

void UHandPresentationComponent::UpdateFlash()
{
	if (!Flash)
	{
		return;
	}
	Flash->ClearInstances();
	if (!bFlashOn || !bHasRightTip)
	{
		Flash->SetVisibility(false);
		return;
	}
	const FVector Centre = RightIndexTip + FVector(12.0, 0.0, 0.0);
	const double Radius = Layout.FlashRadiusM * 100.0;
	FillRing(Flash, Centre, FVector::RightVector, FVector::UpVector, Radius, 1.8);
	Flash->SetVisibility(Flash->GetInstanceCount() > 0);
}

void UHandPresentationComponent::UpdateWard()
{
	bWardVisible = false;
	UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UHandInputSubsystem* Hands = Instance ? Instance->GetSubsystem<UHandInputSubsystem>() : nullptr;
	UWardDetectorSubsystem* Detector = Instance ? Instance->GetSubsystem<UWardDetectorSubsystem>() : nullptr;
	FHandFrame Frame;
	const bool bShow = Hands && Detector && Detector->GetState().bRaised
		&& Hands->GetLatest(EControllerHand::Left, Frame)
		&& Frame.Joints.IsValidIndex(static_cast<int32>(EHandKeypoint::Palm))
		&& WardSegments.Num() > 0;
	if (!bShow)
	{
		for (UStaticMeshComponent* Segment : WardSegments)
		{
			if (Segment)
			{
				Segment->SetVisibility(false);
			}
		}
		return;
	}

	FVector Normal = FWardDetector::PalmNormal(Frame);
	if (Normal.IsNearlyZero())
	{
		Normal = FVector::ForwardVector;
	}
	// Clip space +X is the seated forward. Flip a palm that faces the body so the arc opens toward the arena.
	if (FVector::DotProduct(Normal, FVector::ForwardVector) < 0.0)
	{
		Normal *= -1.0;
	}
	Normal = Normal.GetSafeNormal();
	FVector Up = FVector::UpVector - Normal * FVector::DotProduct(FVector::UpVector, Normal);
	if (Up.SizeSquared() < 0.01)
	{
		Up = FVector::RightVector;
	}
	Up = Up.GetSafeNormal();
	const FVector Right = FVector::CrossProduct(Up, Normal).GetSafeNormal();
	const FVector Palm = Frame.Joints[static_cast<int32>(EHandKeypoint::Palm)].Location;
	const double Inner = Layout.WardArcInnerM * 100.0;
	const double Outer = Layout.WardArcOuterM * 100.0;
	const double Mid = (Inner + Outer) * 0.5;
	const double Thick = FMath::Max(Outer - Inner, 1.0);
	const double Height = Layout.WardArcHeightM * 100.0;
	const int32 Count = WardSegments.Num();
	const double Half = FMath::DegreesToRadians(Layout.WardArcDeg * 0.5);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		UStaticMeshComponent* Segment = WardSegments[Index];
		if (!Segment)
		{
			continue;
		}
		const double Angle = FMath::Lerp(-Half, Half, (Index + 0.5) / static_cast<double>(Count));
		const FVector Dir = Normal * FMath::Cos(Angle) + Right * FMath::Sin(Angle);
		const FVector Tangent = (-Normal * FMath::Sin(Angle) + Right * FMath::Cos(Angle)).GetSafeNormal();
		const double Step = FMath::DegreesToRadians(Layout.WardArcDeg / Count) * Mid * 1.15;
		const FQuat Rotation = FRotationMatrix::MakeFromXZ(Tangent, Up).ToQuat();
		Segment->SetRelativeLocation(Palm + Dir * Mid);
		Segment->SetRelativeRotation(Rotation);
		Greybox::SetSized(Segment, FVector(Step, Thick, Height));
		Segment->SetVisibility(true);
	}
	bWardVisible = true;
}

void UHandPresentationComponent::FillRing(UInstancedStaticMeshComponent* Instances, const FVector& CentreCm, const FVector& AxisA, const FVector& AxisB, double RadiusCm, double TubeCm)
{
	if (!Instances)
	{
		return;
	}
	const int32 Count = FMath::Max(Layout.FlashSegments, 3);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const double Angle = 2.0 * PI * Index / Count;
		const FVector Offset = AxisA * (FMath::Cos(Angle) * RadiusCm) + AxisB * (FMath::Sin(Angle) * RadiusCm);
		const FVector Tangent = (-AxisA * FMath::Sin(Angle) + AxisB * FMath::Cos(Angle)).GetSafeNormal();
		const double Step = (2.0 * PI * RadiusCm / Count) * 1.2;
		const FQuat Rotation = FRotationMatrix::MakeFromXZ(Tangent, AxisB).ToQuat();
		Greybox::AddSized(Instances, CentreCm + Offset, Rotation, FVector(Step, TubeCm, TubeCm));
	}
}
