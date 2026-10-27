#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/StaticMesh.h"
#include "Greybox/VisibleLod0.h"
#include "Hands/HandClip.h"
#include "Hands/HandMesh.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

namespace
{
// T59: the player's hands are a palm and fingers, not joint spheres. These two clips are the open palm and the closed fist.
constexpr int32 HandBoneCount = 25;
constexpr int32 HandFingerParts = 15;
// Both hands' share of PROJECT-PLAN section 7's 350k visible triangles.
constexpr int32 HandTriangleCap = 16000;
const EHandKeypoint Tips[] = {EHandKeypoint::ThumbTip, EHandKeypoint::IndexTip, EHandKeypoint::MiddleTip, EHandKeypoint::RingTip, EHandKeypoint::LittleTip};

bool SampleMiddle(FAutomationTestBase& Test, const TCHAR* RelativePath, EControllerHand Hand, FHandFrame& Out)
{
	FHandClip Clip;
	FString Error;
	const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Clips"), RelativePath);
	if (!FHandClip::LoadFromFile(Path, Clip, Error))
	{
		Test.AddError(FString::Printf(TEXT("load %s: %s"), RelativePath, *Error));
		return false;
	}
	return Test.TestTrue(*FString::Printf(TEXT("%s samples a hand"), RelativePath), Clip.Sample(Hand, Clip.GetDuration() * 0.5, Out));
}

const FHandPartPose* PartTo(const TArray<FHandPartPose>& Parts, EHandKeypoint To)
{
	return Parts.FindByPredicate([To](const FHandPartPose& Pose)
	{ return Pose.To == static_cast<int32>(To); });
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaHandsMeshNotSpheres, "MageArena.Hands.MeshNotSpheres",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaHandsMeshNotSpheres::RunTest(const FString& Parameters)
{
	TMap<EHandPart, int32> Triangles;
	for (const EHandPart Part : {EHandPart::Finger, EHandPart::Palm})
	{
		const TCHAR* Name = Part == EHandPart::Finger ? TEXT("finger") : TEXT("palm");
		UStaticMesh* Mesh = MageHandMesh::BuildPartMesh(Part, GetTransientPackage(), nullptr);
		if (!TestNotNull(*FString::Printf(TEXT("%s mesh is built"), Name), Mesh))
		{
			continue;
		}
		TestFalse(*FString::Printf(TEXT("%s mesh is the hand's own, not an engine basic shape (%s)"), Name, *Mesh->GetPathName()),
		    Mesh->GetPathName().StartsWith(TEXT("/Engine/BasicShapes/")));
		const FVector Extent = Mesh->GetBounds().BoxExtent;
		TestTrue(*FString::Printf(TEXT("%s mesh spans the unit box, so a part's size is its drawn size (%s)"), Name, *Extent.ToString()),
		    Extent.Equals(FVector(0.5), 0.01));
		const int32 Tris = Greybox::MeshLod0Triangles(Mesh);
		TestTrue(*FString::Printf(TEXT("%s mesh has 64..512 triangles (%d)"), Name, Tris), Tris >= 64 && Tris <= 512);
		Triangles.Add(Part, Tris);
	}

	struct FCase
	{
		const TCHAR* Path;
		EControllerHand Hand;
	};
	const FCase Cases[] = {
	    {TEXT("both-palms.normal.jsonl"), EControllerHand::Left},
	    {TEXT("both-palms.normal.jsonl"), EControllerHand::Right},
	    {TEXT("fist/fist.normal.jsonl"), EControllerHand::Left},
	};
	int32 TwoHandTriangles = 0;
	for (int32 CaseIndex = 0; CaseIndex < UE_ARRAY_COUNT(Cases); ++CaseIndex)
	{
		const FCase& Case = Cases[CaseIndex];
		FHandFrame Frame;
		if (!SampleMiddle(*this, Case.Path, Case.Hand, Frame))
		{
			continue;
		}
		TArray<FHandPartPose> Parts;
		MageHandMesh::PoseParts(Frame, Parts);
		int32 Fingers = 0;
		int32 Palms = 0;
		int32 Placeholders = 0;
		int32 HandTriangles = 0;
		for (const FHandPartPose& Pose : Parts)
		{
			Fingers += Pose.Part == EHandPart::Finger ? 1 : 0;
			Palms += Pose.Part == EHandPart::Palm ? 1 : 0;
			Placeholders += Pose.Part != EHandPart::Finger && Pose.Part != EHandPart::Palm ? 1 : 0;
			HandTriangles += Triangles.FindRef(Pose.Part);
		}
		const FString Label = FString::Printf(TEXT("%s %s"), Case.Path, Case.Hand == EControllerHand::Left ? TEXT("L") : TEXT("R"));
		TestEqual(*FString::Printf(TEXT("%s: no joint sphere or bone capsule placeholders"), *Label), Placeholders, 0);
		TestEqual(*FString::Printf(TEXT("%s: one finger segment per phalanx"), *Label), Fingers, HandFingerParts);
		TestTrue(*FString::Printf(TEXT("%s: a palm (%d palm parts)"), *Label, Palms), Palms > 0);
		// Budget/BudgetStressDriver counts a hand as drawn at MageHandJointCount instances under the hand component.
		TestTrue(*FString::Printf(TEXT("%s: at least %d parts, so the D-G4 driver still counts the hand (%d)"), *Label, MageHandJointCount, Parts.Num()),
		    Parts.Num() >= MageHandJointCount);
		// The first two cases are the two hands of one frame.
		if (CaseIndex < 2)
		{
			TwoHandTriangles += HandTriangles;
		}
	}
	TestTrue(*FString::Printf(TEXT("both hands draw at most %d triangles (%d)"), HandTriangleCap, TwoHandTriangles),
	    TwoHandTriangles > 0 && TwoHandTriangles <= HandTriangleCap);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaHandsMeshFollowsPose, "MageArena.Hands.MeshFollowsPose",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaHandsMeshFollowsPose::RunTest(const FString& Parameters)
{
	FHandFrame Open;
	FHandFrame Fist;
	if (!SampleMiddle(*this, TEXT("both-palms.normal.jsonl"), EControllerHand::Left, Open) || !SampleMiddle(*this, TEXT("fist/fist.normal.jsonl"), EControllerHand::Left, Fist))
	{
		return false;
	}
	for (const FHandFrame* Frame : {&Open, &Fist})
	{
		const TCHAR* Label = Frame == &Open ? TEXT("open palm") : TEXT("fist");
		TArray<FHandPartPose> Parts;
		MageHandMesh::PoseParts(*Frame, Parts);
		TSet<TPair<int32, int32>> Bones;
		for (const FHandPartPose& Pose : Parts)
		{
			if (Pose.From == INDEX_NONE || Pose.To == INDEX_NONE)
			{
				continue;
			}
			Bones.Add(TPair<int32, int32>(Pose.From, Pose.To));
			if (Pose.Part != EHandPart::Finger)
			{
				continue;
			}
			const FVector A = Frame->Joints[Pose.From].Location;
			const FVector B = Frame->Joints[Pose.To].Location;
			const FVector Axis = Pose.Rotation.GetAxisZ();
			const FString Name = FString::Printf(TEXT("%s finger %s-%s"), Label, FHandClip::KeypointName(Pose.From), FHandClip::KeypointName(Pose.To));
			TestTrue(*FString::Printf(TEXT("%s runs along its bone (cos %.4f)"), *Name, FVector::DotProduct(Axis, (B - A).GetSafeNormal())),
			    FVector::DotProduct(Axis, (B - A).GetSafeNormal()) > 0.999);
			TestTrue(*FString::Printf(TEXT("%s is centred on its bone (%.3f cm off)"), *Name, FVector::Distance(Pose.CentreCm, (A + B) * 0.5)),
			    FVector::Distance(Pose.CentreCm, (A + B) * 0.5) < 0.05);
			TestTrue(*FString::Printf(TEXT("%s covers its bone (%.2f >= %.2f cm)"), *Name, Pose.SizeCm.Z, FVector::Distance(A, B)),
			    Pose.SizeCm.Z >= FVector::Distance(A, B));
		}
		TestEqual(*FString::Printf(TEXT("%s: every bone has one part"), Label), Bones.Num(), HandBoneCount);
		for (const EHandKeypoint Tip : Tips)
		{
			const FHandPartPose* Pose = PartTo(Parts, Tip);
			const FString Name = FString::Printf(TEXT("%s %s"), Label, FHandClip::KeypointName(static_cast<int32>(Tip)));
			if (!TestNotNull(*FString::Printf(TEXT("%s has a finger segment"), *Name), Pose))
			{
				continue;
			}
			TestTrue(*FString::Printf(TEXT("%s segment is a finger"), *Name), Pose->Part == EHandPart::Finger);
			const FVector Axis = Pose->Rotation.GetAxisZ();
			const double Past = FVector::DotProduct(Pose->CentreCm + Axis * Pose->SizeCm.Z * 0.5 - Frame->Joints[static_cast<int32>(Tip)].Location, Axis);
			TestTrue(*FString::Printf(TEXT("%s: the fingertip ends 0..1.5 cm past the tip joint (%.2f)"), *Name, Past), Past >= 0.0 && Past <= 1.5);
		}
	}

	// The parts follow the pose: a fist curls the index tip segment at least 45 degrees away from the open palm's.
	TArray<FHandPartPose> OpenParts;
	TArray<FHandPartPose> FistParts;
	MageHandMesh::PoseParts(Open, OpenParts);
	MageHandMesh::PoseParts(Fist, FistParts);
	const FHandPartPose* OpenTip = PartTo(OpenParts, EHandKeypoint::IndexTip);
	const FHandPartPose* FistTip = PartTo(FistParts, EHandKeypoint::IndexTip);
	const FHandPartPose* OpenKnuckle = PartTo(OpenParts, EHandKeypoint::IndexProximal);
	const FHandPartPose* FistKnuckle = PartTo(FistParts, EHandKeypoint::IndexProximal);
	if (TestTrue(TEXT("index segments exist in both poses"), OpenTip && FistTip && OpenKnuckle && FistKnuckle))
	{
		// Measured against the hand's own metacarpal, so the fist's wrist turn does not count as a curl.
		const double OpenCurl = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(OpenTip->Rotation.GetAxisZ(), OpenKnuckle->Rotation.GetAxisZ()), -1.0, 1.0)));
		const double FistCurl = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(FistTip->Rotation.GetAxisZ(), FistKnuckle->Rotation.GetAxisZ()), -1.0, 1.0)));
		TestTrue(*FString::Printf(TEXT("the fist curls the index tip segment 45+ degrees more than the open palm (%.1f vs %.1f)"), FistCurl, OpenCurl),
		    FistCurl - OpenCurl >= 45.0);
	}

	// Moving the hand moves every part with it, and turns none.
	FHandFrame Moved = Open;
	const FVector Offset(10.0, -5.0, 3.0);
	for (FHandJointPose& Joint : Moved.Joints)
	{
		Joint.Location += Offset;
	}
	TArray<FHandPartPose> MovedParts;
	MageHandMesh::PoseParts(Moved, MovedParts);
	if (TestEqual(TEXT("moved hand has the same parts"), MovedParts.Num(), OpenParts.Num()))
	{
		double WorstMove = 0.0;
		double WorstTurn = 0.0;
		for (int32 Index = 0; Index < OpenParts.Num(); ++Index)
		{
			WorstMove = FMath::Max(WorstMove, FVector::Distance(MovedParts[Index].CentreCm - OpenParts[Index].CentreCm, Offset));
			WorstTurn = FMath::Max(WorstTurn, static_cast<double>(MovedParts[Index].Rotation.AngularDistance(OpenParts[Index].Rotation)));
		}
		TestTrue(*FString::Printf(TEXT("every part moves with the hand (worst %.4f cm off)"), WorstMove), WorstMove < 0.01);
		TestTrue(*FString::Printf(TEXT("no part turns when the hand only moves (worst %.5f rad)"), WorstTurn), WorstTurn < 0.001);
	}
	return true;
}

#endif
