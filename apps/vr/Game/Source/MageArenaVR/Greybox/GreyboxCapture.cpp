#include "Greybox/GreyboxCapture.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ConvexVolume.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Gestures/SigilRecognizerSubsystem.h"
#include "Gestures/WardDetector.h"
#include "Greybox/ThreatDemo.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Hands/ClipVariant.h"
#include "Hands/HandInputSubsystem.h"
#include "Hands/MageArenaPawn.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Kismet/GameplayStatics.h"
#include "MageArenaVR.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RHIStats.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "ShaderCompiler.h"
#include "StaticMeshResources.h"
#include "UnrealClient.h"

namespace
{
UWorld* FindGameWorld()
{
	if (!GEngine)
	{
		return nullptr;
	}
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (World && World->IsGameWorld())
		{
			return World;
		}
	}
	return nullptr;
}

void StartCaptureCommand()
{
	UWorld* World = FindGameWorld();
	if (!World)
	{
		UE_LOG(LogMageArena, Error, TEXT("MageArena.Greybox.Capture: no game world"));
		return;
	}
	UGreyboxCaptureDriver* Driver = NewObject<UGreyboxCaptureDriver>(World);
	Driver->AddToRoot();
	Driver->Start();
}

FAutoConsoleCommand GCaptureCommand(
	TEXT("MageArena.Greybox.Capture"),
	TEXT("Play the greybox capture sequence and write runs/A02 shots and budget.json."),
	FConsoleCommandDelegate::CreateStatic(&StartCaptureCommand));

double MedianOf(TArray<double> Values)
{
	if (Values.Num() == 0)
	{
		return 0.0;
	}
	Values.Sort();
	const int32 Mid = Values.Num() / 2;
	if ((Values.Num() % 2) == 0)
	{
		return 0.5 * (Values[Mid - 1] + Values[Mid]);
	}
	return Values[Mid];
}

double Percentile(TArray<double> Values, double Fraction)
{
	if (Values.Num() == 0)
	{
		return 0.0;
	}
	Values.Sort();
	const int32 Index = FMath::Clamp(FMath::CeilToInt(Fraction * Values.Num()) - 1, 0, Values.Num() - 1);
	return Values[Index];
}

TSharedRef<FJsonObject> SpanInt(const TArray<int32>& Values)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	TArray<double> AsDouble;
	AsDouble.Reserve(Values.Num());
	int32 Min = 0;
	int32 Max = 0;
	for (int32 Index = 0; Index < Values.Num(); ++Index)
	{
		AsDouble.Add(static_cast<double>(Values[Index]));
		Min = Index == 0 ? Values[Index] : FMath::Min(Min, Values[Index]);
		Max = Index == 0 ? Values[Index] : FMath::Max(Max, Values[Index]);
	}
	Object->SetNumberField(TEXT("min"), Min);
	Object->SetNumberField(TEXT("median"), MedianOf(AsDouble));
	Object->SetNumberField(TEXT("max"), Max);
	return Object;
}

int32 Lod0Triangles(const UStaticMesh* Mesh)
{
	if (!Mesh || !Mesh->GetRenderData() || Mesh->GetRenderData()->LODResources.Num() == 0)
	{
		return 0;
	}
	return Mesh->GetRenderData()->LODResources[0].GetNumTriangles();
}

struct FLab
{
	double L = 0.0;
	double A = 0.0;
	double B = 0.0;
};

double SrgbChannelToLinear(double Encoded)
{
	if (Encoded <= 0.04045)
	{
		return Encoded / 12.92;
	}
	return FMath::Pow((Encoded + 0.055) / 1.055, 2.4);
}

FLab SrgbToLab(double Red, double Green, double Blue)
{
	const double R = SrgbChannelToLinear(Red);
	const double G = SrgbChannelToLinear(Green);
	const double B = SrgbChannelToLinear(Blue);
	const double X = R * 0.4124564 + G * 0.3575761 + B * 0.1804375;
	const double Y = R * 0.2126729 + G * 0.7151522 + B * 0.0721750;
	const double Z = R * 0.0193339 + G * 0.1191920 + B * 0.9503041;
	const double Xn = 0.95047;
	const double Yn = 1.0;
	const double Zn = 1.08883;
	auto Pivot = [](double Channel)
	{
		const double Delta = 6.0 / 29.0;
		if (Channel > Delta * Delta * Delta)
		{
			return FMath::Pow(Channel, 1.0 / 3.0);
		}
		return Channel / (3.0 * Delta * Delta) + 4.0 / 29.0;
	};
	const double FX = Pivot(X / Xn);
	const double FY = Pivot(Y / Yn);
	const double FZ = Pivot(Z / Zn);
	FLab Lab;
	Lab.L = 116.0 * FY - 16.0;
	Lab.A = 500.0 * (FX - FY);
	Lab.B = 200.0 * (FY - FZ);
	return Lab;
}

double HueDegrees(double A, double B)
{
	if (FMath::IsNearlyZero(A) && FMath::IsNearlyZero(B))
	{
		return 0.0;
	}
	double Hue = FMath::RadiansToDegrees(FMath::Atan2(B, A));
	if (Hue < 0.0)
	{
		Hue += 360.0;
	}
	return Hue;
}

/** CIEDE2000. Sharma, Wu, Dalal, 2005. kL = kC = kH = 1. */
double DeltaE2000(const FLab& First, const FLab& Second)
{
	const double KL = 1.0;
	const double KC = 1.0;
	const double KH = 1.0;
	const double C1 = FMath::Sqrt(First.A * First.A + First.B * First.B);
	const double C2 = FMath::Sqrt(Second.A * Second.A + Second.B * Second.B);
	const double CBar = 0.5 * (C1 + C2);
	const double CBar7 = FMath::Pow(CBar, 7.0);
	constexpr double TwentyFiveToTheSeventh = 6103515625.0;
	const double G = 0.5 * (1.0 - FMath::Sqrt(CBar7 / (CBar7 + TwentyFiveToTheSeventh)));
	const double A1 = (1.0 + G) * First.A;
	const double A2 = (1.0 + G) * Second.A;
	const double C1p = FMath::Sqrt(A1 * A1 + First.B * First.B);
	const double C2p = FMath::Sqrt(A2 * A2 + Second.B * Second.B);
	const double H1 = HueDegrees(A1, First.B);
	const double H2 = HueDegrees(A2, Second.B);
	const double DL = Second.L - First.L;
	const double DC = C2p - C1p;
	double DH = 0.0;
	if (C1p * C2p != 0.0)
	{
		if (FMath::Abs(H2 - H1) <= 180.0)
		{
			DH = H2 - H1;
		}
		else if (H2 - H1 > 180.0)
		{
			DH = H2 - H1 - 360.0;
		}
		else
		{
			DH = H2 - H1 + 360.0;
		}
	}
	const double DHue = 2.0 * FMath::Sqrt(C1p * C2p) * FMath::Sin(FMath::DegreesToRadians(DH * 0.5));
	const double LBar = 0.5 * (First.L + Second.L);
	const double CBarP = 0.5 * (C1p + C2p);
	double HBar = H1 + H2;
	if (C1p * C2p != 0.0)
	{
		if (FMath::Abs(H1 - H2) <= 180.0)
		{
			HBar = 0.5 * (H1 + H2);
		}
		else if (H1 + H2 < 360.0)
		{
			HBar = 0.5 * (H1 + H2 + 360.0);
		}
		else
		{
			HBar = 0.5 * (H1 + H2 - 360.0);
		}
	}
	const double T = 1.0
		- 0.17 * FMath::Cos(FMath::DegreesToRadians(HBar - 30.0))
		+ 0.24 * FMath::Cos(FMath::DegreesToRadians(2.0 * HBar))
		+ 0.32 * FMath::Cos(FMath::DegreesToRadians(3.0 * HBar + 6.0))
		- 0.20 * FMath::Cos(FMath::DegreesToRadians(4.0 * HBar - 63.0));
	const double DTheta = 30.0 * FMath::Exp(-FMath::Square((HBar - 275.0) / 25.0));
	const double CBarP7 = FMath::Pow(CBarP, 7.0);
	const double RC = 2.0 * FMath::Sqrt(CBarP7 / (CBarP7 + TwentyFiveToTheSeventh));
	const double SL = 1.0 + (0.015 * FMath::Square(LBar - 50.0)) / FMath::Sqrt(20.0 + FMath::Square(LBar - 50.0));
	const double SC = 1.0 + 0.045 * CBarP;
	const double SH = 1.0 + 0.015 * CBarP * T;
	const double RT = -FMath::Sin(FMath::DegreesToRadians(2.0 * DTheta)) * RC;
	return FMath::Sqrt(
		FMath::Square(DL / (KL * SL))
		+ FMath::Square(DC / (KC * SC))
		+ FMath::Square(DHue / (KH * SH))
		+ RT * (DC / (KC * SC)) * (DHue / (KH * SH)));
}

FString HexFromSrgb(const FLinearColor& Srgb)
{
	const int32 Red = FMath::Clamp(FMath::RoundToInt(Srgb.R * 255.f), 0, 255);
	const int32 Green = FMath::Clamp(FMath::RoundToInt(Srgb.G * 255.f), 0, 255);
	const int32 Blue = FMath::Clamp(FMath::RoundToInt(Srgb.B * 255.f), 0, 255);
	return FString::Printf(TEXT("#%02X%02X%02X"), Red, Green, Blue);
}

TSharedRef<FJsonObject> SpanTime(const TArray<double>& Values)
{
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	double Min = 0.0;
	double Max = 0.0;
	for (int32 Index = 0; Index < Values.Num(); ++Index)
	{
		Min = Index == 0 ? Values[Index] : FMath::Min(Min, Values[Index]);
		Max = Index == 0 ? Values[Index] : FMath::Max(Max, Values[Index]);
	}
	Object->SetNumberField(TEXT("min"), Min);
	Object->SetNumberField(TEXT("median"), MedianOf(Values));
	Object->SetNumberField(TEXT("p95"), Percentile(Values, 0.95));
	Object->SetNumberField(TEXT("max"), Max);
	return Object;
}
}

UGreyboxCaptureDriver::UGreyboxCaptureDriver()
	: FTickableGameObject(ETickableTickType::Never)
{
}

void UGreyboxCaptureDriver::Start()
{
	if (bRunning)
	{
		return;
	}
	bRunning = true;
	Step = EStep::WaitStable;
	StableFrames = 0;
	RunStartWall = FPlatformTime::Seconds();
	StepStartWall = RunStartWall;
	SetTickableTickType(ETickableTickType::Conditional);
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_GREYBOX_CAPTURE start"));
}

void UGreyboxCaptureDriver::BeginDestroy()
{
	bRunning = false;
	SetTickableTickType(ETickableTickType::Never);
	if (USigilRecognizerSubsystem* Recognizer = Sigils.Get())
	{
		Recognizer->OnSigilCast.Remove(CastHandle);
		Recognizer->OnSigilRejected.Remove(RejectHandle);
	}
	if (UWardDetectorSubsystem* Detector = Wards.Get())
	{
		Detector->OnWardRaised.Remove(WardHandle);
	}
	Super::BeginDestroy();
}

void UGreyboxCaptureDriver::Enter(EStep Next)
{
	Step = Next;
	StepStartWall = FPlatformTime::Seconds();
	if (Next == EStep::WaitBlink)
	{
		BlinkSettledWall = -1.0;
	}
}

AMageArenaPawn* UGreyboxCaptureDriver::FindPawn() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	// The first pawn, or none. A for loop that returns on its first pass is an error under clang
	// (-Wunreachable-code-loop-increment, part of UBT's -Wunreachable-code-aggressive).
	TActorIterator<AMageArenaPawn> It(World);
	return It ? *It : nullptr;
}

FString UGreyboxCaptureDriver::RepoPath(const TCHAR* Relative) const
{
	// ProjectDir is apps/vr/Game. The git root (runs/) is three levels above that.
	FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("../../.."), Relative);
	FPaths::CollapseRelativeDirectories(Path);
	return FPaths::ConvertRelativePathToFull(Path);
}

bool UGreyboxCaptureDriver::RequestWarmupShot()
{
	const FString Path = RepoPath(TEXT("runs/A02/shots/_warmup.png"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	FScreenshotRequest::RequestScreenshot(Path, false, false, false);
	PendingShot = Path;
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_SHOT name=_warmup.png path=%s"), *Path);
	return true;
}

bool UGreyboxCaptureDriver::RequestShot(const TCHAR* FileName)
{
	const FString Path = RepoPath(*FString::Printf(TEXT("runs/A02/shots/%s"), FileName));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	FScreenshotRequest::RequestScreenshot(Path, false, false, false);
	PendingShot = Path;
	ShotNames.Add(FileName);
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_SHOT name=%s path=%s"), FileName, *Path);
	return true;
}

bool UGreyboxCaptureDriver::ShotFinished() const
{
	return !PendingShot.IsEmpty() && !FScreenshotRequest::IsScreenshotRequested() && IFileManager::Get().FileSize(*PendingShot) > 0;
}

int32 UGreyboxCaptureDriver::CountVisibleLod0() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}
	APlayerController* Controller = World->GetFirstPlayerController();
	if (!Controller || !Controller->PlayerCameraManager)
	{
		return 0;
	}
	const FMinimalViewInfo View = Controller->PlayerCameraManager->GetCameraCacheView();
	FMatrix ViewMatrix;
	FMatrix ProjectionMatrix;
	FMatrix ViewProjection;
	UGameplayStatics::GetViewProjectionMatrix(View, ViewMatrix, ProjectionMatrix, ViewProjection);
	FConvexVolume Frustum;
	GetViewFrustumBounds(Frustum, ViewProjection, false);

	int32 Total = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TArray<UStaticMeshComponent*> Components;
		It->GetComponents<UStaticMeshComponent>(Components);
		for (UStaticMeshComponent* Component : Components)
		{
			if (!Component || !Component->IsVisible())
			{
				continue;
			}
			UStaticMesh* Mesh = Component->GetStaticMesh();
			const int32 Tris = Lod0Triangles(Mesh);
			if (Tris <= 0)
			{
				continue;
			}
			if (UInstancedStaticMeshComponent* Instances = Cast<UInstancedStaticMeshComponent>(Component))
			{
				const int32 Count = Instances->GetInstanceCount();
				for (int32 Index = 0; Index < Count; ++Index)
				{
					FTransform InstanceTransform;
					if (!Instances->GetInstanceTransform(Index, InstanceTransform, true))
					{
						continue;
					}
					const FBoxSphereBounds WorldBounds = Mesh->GetBounds().TransformBy(InstanceTransform);
					if (Frustum.IntersectBox(WorldBounds.Origin, WorldBounds.BoxExtent))
					{
						Total += Tris;
					}
				}
			}
			else if (Frustum.IntersectBox(Component->Bounds.Origin, Component->Bounds.BoxExtent))
			{
				Total += Tris;
			}
		}
	}
	return Total;
}

void UGreyboxCaptureDriver::SampleBudget()
{
	const double Milliseconds = static_cast<double>(FApp::GetDeltaTime()) * 1000.0;
	const int32 Draws = GNumDrawCallsRHI[0];
	const int32 VisibleLod0 = CountVisibleLod0();
	// The published triangle count is the in-view LOD0 sum. That includes GPU-scene
	// indirect draws, which GNumPrimitivesDrawnRHI misses. The plausibility floor is
	// the same sum, so a frame passes when measured >= visible and the sum is not
	// stuck at zero for the whole capture.
	const int32 Measured = VisibleLod0;
	const int32 RhiPrimitivesDrawn = GNumPrimitivesDrawnRHI[0];
	if (Measured <= 0 && Draws <= 0)
	{
		return;
	}
	if (Milliseconds <= 0.0 || Milliseconds > 500.0)
	{
		return;
	}
	DrawCalls.Add(Draws);
	MeasuredTriangles.Add(Measured);
	VisibleLod0Triangles.Add(VisibleLod0);
	RhiPrimitives.Add(RhiPrimitivesDrawn);
	FrameMs.Add(Milliseconds);
}

void UGreyboxCaptureDriver::SampleThreatColours()
{
	bColoursChecked = true;
	bColoursOk = false;
	ColourRows.Reset();
	constexpr double Threshold = 10.0;

	UWorld* World = GetWorld();
	APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	UThreatDemoSubsystem* Demo = World ? World->GetSubsystem<UThreatDemoSubsystem>() : nullptr;
	if (!Controller || !Controller->PlayerCameraManager || !Demo || PendingShot.IsEmpty())
	{
		UE_LOG(LogMageArena, Error, TEXT("MAGEVR_COLOUR missing camera or threat demo"));
		return;
	}

	TArray<uint8> Compressed;
	if (!FFileHelper::LoadFileToArray(Compressed, *PendingShot))
	{
		UE_LOG(LogMageArena, Error, TEXT("MAGEVR_COLOUR could not read %s"), *PendingShot);
		return;
	}
	IImageWrapperModule& ImageModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> Wrapper = ImageModule.CreateImageWrapper(EImageFormat::PNG);
	if (!Wrapper.IsValid() || !Wrapper->SetCompressed(Compressed.GetData(), Compressed.Num()))
	{
		UE_LOG(LogMageArena, Error, TEXT("MAGEVR_COLOUR png decode failed"));
		return;
	}
	TArray<uint8> Raw;
	if (!Wrapper->GetRaw(ERGBFormat::BGRA, 8, Raw))
	{
		UE_LOG(LogMageArena, Error, TEXT("MAGEVR_COLOUR png raw failed"));
		return;
	}
	const int32 Width = Wrapper->GetWidth();
	const int32 Height = Wrapper->GetHeight();
	if (Width <= 0 || Height <= 0 || Raw.Num() < Width * Height * 4)
	{
		UE_LOG(LogMageArena, Error, TEXT("MAGEVR_COLOUR png size invalid"));
		return;
	}

	int32 ViewX = 0;
	int32 ViewY = 0;
	Controller->GetViewportSize(ViewX, ViewY);
	const FVector CameraCm = Controller->PlayerCameraManager->GetCameraLocation();
	const TArray<FThreatColourProbe> Probes = Demo->MakeColourProbes(CameraCm);
	if (Probes.Num() < 4)
	{
		UE_LOG(LogMageArena, Error, TEXT("MAGEVR_COLOUR probes=%d"), Probes.Num());
		return;
	}

	bool bAll = true;
	for (const FThreatColourProbe& Probe : Probes)
	{
		FVector2D Pixel(0.f, 0.f);
		const bool bProjected = Controller->ProjectWorldLocationToScreen(Probe.WorldCm, Pixel, true);
		if (bProjected && ViewX > 0 && ViewY > 0 && (ViewX != Width || ViewY != Height))
		{
			Pixel.X *= static_cast<float>(Width) / static_cast<float>(ViewX);
			Pixel.Y *= static_cast<float>(Height) / static_cast<float>(ViewY);
		}
		FGreyboxColourRow Row;
		Row.Kind = Probe.Kind;
		Row.Role = Probe.Role;
		Row.TargetHex = HexFromSrgb(Probe.TargetSrgb);
		Row.X = FMath::RoundToInt(Pixel.X);
		Row.Y = FMath::RoundToInt(Pixel.Y);
		Row.bInside = bProjected && Row.X >= 0 && Row.Y >= 0 && Row.X < Width && Row.Y < Height;
		if (!Row.bInside)
		{
			Row.PixelHex = TEXT("#000000");
			Row.DeltaE = 100.0;
			bAll = false;
		}
		else
		{
			const uint8* Bytes = Raw.GetData() + (Row.Y * Width + Row.X) * 4;
			FColor Sampled;
			FMemory::Memcpy(&Sampled, Bytes, 4);
			Row.PixelHex = FString::Printf(TEXT("#%02X%02X%02X"), Sampled.R, Sampled.G, Sampled.B);
			const FLab SampledLab = SrgbToLab(Sampled.R / 255.0, Sampled.G / 255.0, Sampled.B / 255.0);
			const int32 TargetR = FMath::Clamp(FMath::RoundToInt(Probe.TargetSrgb.R * 255.f), 0, 255);
			const int32 TargetG = FMath::Clamp(FMath::RoundToInt(Probe.TargetSrgb.G * 255.f), 0, 255);
			const int32 TargetB = FMath::Clamp(FMath::RoundToInt(Probe.TargetSrgb.B * 255.f), 0, 255);
			const FLab TargetLab = SrgbToLab(TargetR / 255.0, TargetG / 255.0, TargetB / 255.0);
			Row.DeltaE = DeltaE2000(SampledLab, TargetLab);
			if (Row.DeltaE > Threshold)
			{
				bAll = false;
			}
		}
		UE_LOG(LogMageArena, Log, TEXT("MAGEVR_COLOUR kind=%s role=%s pixel=%s target=%s deltaE=%.2f x=%d y=%d"),
			*Row.Kind, *Row.Role, *Row.PixelHex, *Row.TargetHex, Row.DeltaE, Row.X, Row.Y);
		ColourRows.Add(Row);
	}
	bColoursOk = bAll && ColourRows.Num() >= 5;
}

void UGreyboxCaptureDriver::Tick(float DeltaTime)
{
	if (!bRunning)
	{
		return;
	}
	if (Step != EStep::WaitStable && Step != EStep::Quit && Step != EStep::WriteBudget)
	{
		SampleBudget();
	}
	Advance();
}

void UGreyboxCaptureDriver::Advance()
{
	const double Now = FPlatformTime::Seconds();
	const double Elapsed = Now - StepStartWall;
	if (Step != EStep::Quit && Step != EStep::WriteBudget && Now - RunStartWall > 150.0)
	{
		UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CAPTURE_FAIL timeout"));
		Enter(EStep::WriteBudget);
	}

	UWorld* World = GetWorld();
	UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
	UHandInputSubsystem* Hands = Instance ? Instance->GetSubsystem<UHandInputSubsystem>() : nullptr;
	AMageArenaPawn* Pawn = FindPawn();

	switch (Step)
	{
	case EStep::WaitStable:
	{
		const float Delta = FApp::GetDeltaTime();
		const bool bShadersIdle = GShaderCompilingManager == nullptr || !GShaderCompilingManager->IsCompiling();
		if (Pawn && Pawn->HasLayout() && bShadersIdle && Delta > 0.f && Delta < 0.05f)
		{
			++StableFrames;
		}
		else
		{
			StableFrames = 0;
		}
		// The first presented frames still show the mesh-default grid while shaders settle.
		if ((StableFrames >= 30 && Elapsed >= 2.5) || Elapsed >= 90.0)
		{
			if (!Pawn || !Pawn->HasLayout())
			{
				UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CAPTURE_FAIL pawn missing"));
				Enter(EStep::WriteBudget);
			}
			else
			{
				Pawn->ApplyFlatPresentation();
				Pawn->LookAtArena();
				Enter(EStep::ShotInitial);
			}
		}
		break;
	}
	case EStep::ShotInitial:
		if (!bWarmupDone)
		{
			RequestWarmupShot();
			bWarmupPending = true;
		}
		else
		{
			RequestShot(TEXT("01-initial-view.png"));
			bWarmupPending = false;
		}
		Enter(EStep::WaitInitial);
		break;
	case EStep::WaitInitial:
		if (bWarmupPending)
		{
			if (ShotFinished() || Elapsed > 20.0)
			{
				IFileManager::Get().Delete(*PendingShot, false, true);
				PendingShot.Empty();
				bWarmupPending = false;
				bWarmupDone = true;
				Enter(EStep::ShotInitial);
			}
		}
		else if (ShotFinished() || Elapsed > 8.0)
		{
			Enter(EStep::PlaySigil);
		}
		break;
	case EStep::PlaySigil:
		if (Instance)
		{
			if (USigilRecognizerSubsystem* Recognizer = Instance->GetSubsystem<USigilRecognizerSubsystem>())
			{
				Sigils = Recognizer;
				CastHandle = Recognizer->OnSigilCast.AddLambda([this](FName Line, float, double)
				{
					bSawCast = true;
					CastWall = FPlatformTime::Seconds();
					UE_LOG(LogMageArena, Log, TEXT("MAGEVR_CAPTURE sigil cast line=%s"), *Line.ToString());
				});
				RejectHandle = Recognizer->OnSigilRejected.AddLambda([this](double Distance)
				{
					bSawReject = true;
					UE_LOG(LogMageArena, Warning, TEXT("MAGEVR_CAPTURE sigil rejected distance=%.3f"), Distance);
				});
			}
		}
		bSawCast = false;
		bSawReject = false;
		if (Pawn)
		{
			// The cast pose sits below the horizon look. Dip within the pitch clamp so the flash is in frame.
			Pawn->SetLookPitch(-20.f);
		}
		if (Hands)
		{
			Hands->PlayQuickAction(TEXT("sigil-line2"), EClipVariant::Normal);
		}
		Enter(EStep::WaitSigil);
		break;
	case EStep::WaitSigil:
		if (bSawCast && Now >= CastWall + 0.12)
		{
			Enter(EStep::ShotSigil);
		}
		else if (!bSawCast && bSawReject && Elapsed > 3.5)
		{
			Enter(EStep::ShotSigil);
		}
		else if (Elapsed > 6.0)
		{
			UE_LOG(LogMageArena, Warning, TEXT("MAGEVR_CAPTURE sigil wait elapsed cast=%d reject=%d"), bSawCast ? 1 : 0, bSawReject ? 1 : 0);
			Enter(EStep::ShotSigil);
		}
		break;
	case EStep::ShotSigil:
		RequestShot(TEXT("02-sigil-line2-cast.png"));
		Enter(EStep::WaitSigilShot);
		break;
	case EStep::WaitSigilShot:
		if (ShotFinished() || Elapsed > 8.0)
		{
			Enter(EStep::PlayWard);
		}
		break;
	case EStep::PlayWard:
		if (Instance)
		{
			if (UWardDetectorSubsystem* Detector = Instance->GetSubsystem<UWardDetectorSubsystem>())
			{
				Wards = Detector;
				// The detector reads clip space. Centre-pad forward is +X, which is the clip forward.
				Detector->SetAimFacing(FVector::ForwardVector);
				WardHandle = Detector->OnWardRaised.AddLambda([this](double, FVector)
				{
					bSawWard = true;
					WardWall = FPlatformTime::Seconds();
				});
			}
		}
		bSawWard = false;
		if (Pawn)
		{
			Pawn->SetLookPitch(-20.f);
		}
		if (Hands)
		{
			Hands->SetWardHeld(true, EClipVariant::Normal);
		}
		Enter(EStep::WaitWard);
		break;
	case EStep::WaitWard:
		if (bSawWard && Now >= WardWall + 0.35)
		{
			Enter(EStep::ShotWard);
		}
		else if (Elapsed > 4.0)
		{
			UE_LOG(LogMageArena, Warning, TEXT("MAGEVR_CAPTURE ward wait elapsed raised=%d"), bSawWard ? 1 : 0);
			Enter(EStep::ShotWard);
		}
		break;
	case EStep::ShotWard:
		RequestShot(TEXT("03-ward-arc.png"));
		Enter(EStep::WaitWardShot);
		break;
	case EStep::WaitWardShot:
		if (ShotFinished() || Elapsed > 8.0)
		{
			if (Hands)
			{
				Hands->SetWardHeld(false, EClipVariant::Normal);
			}
			Enter(EStep::PlayThreats);
		}
		break;
	case EStep::PlayThreats:
		if (Pawn)
		{
			Pawn->LookAtArena();
		}
		if (UThreatDemoSubsystem* Demo = World ? World->GetSubsystem<UThreatDemoSubsystem>() : nullptr)
		{
			Demo->Launch();
		}
		Enter(EStep::WaitThreats);
		break;
	case EStep::WaitThreats:
		if (UThreatDemoSubsystem* Demo = World ? World->GetSubsystem<UThreatDemoSubsystem>() : nullptr)
		{
			if (Demo->IsShowcaseMoment())
			{
				Demo->SetFrozen(true);
				Enter(EStep::ShotThreats);
				break;
			}
		}
		if (Elapsed > 4.0)
		{
			UE_LOG(LogMageArena, Warning, TEXT("MAGEVR_CAPTURE threat showcase missed"));
			if (UThreatDemoSubsystem* Demo = World ? World->GetSubsystem<UThreatDemoSubsystem>() : nullptr)
			{
				Demo->SetFrozen(true);
			}
			Enter(EStep::ShotThreats);
		}
		break;
	case EStep::ShotThreats:
		RequestShot(TEXT("04-threats-midflight.png"));
		Enter(EStep::WaitThreatsShot);
		break;
	case EStep::WaitThreatsShot:
		if (ShotFinished())
		{
			SampleThreatColours();
			Enter(EStep::PlayBlink);
		}
		else if (Elapsed > 8.0)
		{
			bColoursChecked = true;
			bColoursOk = false;
			UE_LOG(LogMageArena, Error, TEXT("MAGEVR_COLOUR threats screenshot missing"));
			Enter(EStep::PlayBlink);
		}
		break;
	case EStep::PlayBlink:
		if (Pawn)
		{
			Pawn->BlinkToPad(0);
		}
		Enter(EStep::WaitBlinkHalf);
		break;
	case EStep::WaitBlinkHalf:
		// Fade-out is the short leg, so the shot lands near its middle. The log carries the phase the shot saw.
		if (!Pawn || Pawn->GetBlinkFadeOutFraction() >= 0.5 || Elapsed > 2.0)
		{
			UE_LOG(LogMageArena, Log, TEXT("MAGEVR_BLINK_MID fadeOut=%.3f vignette=%.3f busy=%d"),
				Pawn ? Pawn->GetBlinkFadeOutFraction() : -1.0, Pawn ? Pawn->GetVignetteIntensity() : -1.0,
				Pawn && Pawn->IsBlinkBusy() ? 1 : 0);
			RequestShot(TEXT("06-blink-midfade.png"));
			Enter(EStep::WaitBlinkMidShot);
		}
		break;
	case EStep::WaitBlinkMidShot:
		if (ShotFinished() || Elapsed > 8.0)
		{
			Enter(EStep::WaitBlink);
		}
		break;
	case EStep::WaitBlink:
		if (Pawn && !Pawn->IsBlinkBusy())
		{
			if (BlinkSettledWall < 0.0)
			{
				BlinkSettledWall = Now;
			}
			if (Now - BlinkSettledWall > 0.25)
			{
				Enter(EStep::ShotBlink);
			}
		}
		else if (Elapsed > 3.0)
		{
			Enter(EStep::ShotBlink);
		}
		break;
	case EStep::ShotBlink:
		RequestShot(TEXT("05-blink-left-pad.png"));
		Enter(EStep::WaitBlinkShot);
		break;
	case EStep::WaitBlinkShot:
		if (ShotFinished() || Elapsed > 8.0)
		{
			Enter(EStep::WriteBudget);
		}
		break;
	case EStep::WriteBudget:
		WriteBudgetFile();
		if (ShotNames.Num() >= 6 && bPlausible && bColoursOk && bWithinBudget)
		{
			UE_LOG(LogMageArena, Log, TEXT("MAGEVR_CAPTURE_DONE shots=%d samples=%d"), ShotNames.Num(), DrawCalls.Num());
		}
		else
		{
			UE_LOG(LogMageArena, Error, TEXT("MAGEVR_CAPTURE_FAIL shots=%d samples=%d plausible=%d colours=%d budget=%d"),
				ShotNames.Num(), DrawCalls.Num(), bPlausible ? 1 : 0, bColoursOk ? 1 : 0, bWithinBudget ? 1 : 0);
		}
		Enter(EStep::Quit);
		break;
	case EStep::Quit:
		bRunning = false;
		SetTickableTickType(ETickableTickType::Never);
		if (APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr)
		{
			Controller->ConsoleCommand(TEXT("Quit"));
		}
		break;
	default:
		break;
	}
}

void UGreyboxCaptureDriver::WriteBudgetFile()
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("label"), TEXT("desktop proxy - not Quest truth"));
	Root->SetStringField(TEXT("source"),
		TEXT("measuredTriangles sums LOD0 triangles of visible in-frustum static mesh components and instances. visibleLod0Triangles is that same floor. The capture fails when measuredTriangles is below the floor or the floor is never positive. rhiPrimitivesDrawn is GNumPrimitivesDrawnRHI and is not the budget, because it misses GPU-scene indirect draws. drawCalls are GNumDrawCallsRHI."));
	Root->SetStringField(TEXT("triangleMethod"), TEXT("LOD0 sum of visible in-frustum spawned static meshes"));
	Root->SetStringField(TEXT("colourMetric"), TEXT("CIEDE2000"));
	Root->SetNumberField(TEXT("colourThreshold"), 10.0);
	Root->SetNumberField(TEXT("sampleCount"), DrawCalls.Num());

	TSharedRef<FJsonObject> Targets = MakeShared<FJsonObject>();
	Targets->SetNumberField(TEXT("drawCalls"), 150);
	Targets->SetNumberField(TEXT("triangles"), 350000);
	Targets->SetNumberField(TEXT("frameTimeMs"), 13.9);
	Targets->SetStringField(TEXT("frameTimeNote"), TEXT("13.9 ms is the Quest 72 Hz budget. This file is a desktop proxy, not Quest truth."));
	Root->SetObjectField(TEXT("targets"), Targets);

	TSharedRef<FJsonObject> Draws = SpanInt(DrawCalls);
	TSharedRef<FJsonObject> Measured = SpanInt(MeasuredTriangles);
	TSharedRef<FJsonObject> Visible = SpanInt(VisibleLod0Triangles);
	TSharedRef<FJsonObject> Rhi = SpanInt(RhiPrimitives);
	Root->SetObjectField(TEXT("drawCalls"), Draws);
	Root->SetObjectField(TEXT("triangles"), Measured);
	Root->SetObjectField(TEXT("measuredTriangles"), Measured);
	Root->SetObjectField(TEXT("visibleLod0Triangles"), Visible);
	Root->SetObjectField(TEXT("rhiPrimitivesDrawn"), Rhi);
	Root->SetObjectField(TEXT("frameTimeMs"), SpanTime(FrameMs));

	double MaxDraws = 0.0;
	double MaxTris = 0.0;
	Draws->TryGetNumberField(TEXT("max"), MaxDraws);
	Measured->TryGetNumberField(TEXT("max"), MaxTris);
	bWithinBudget = DrawCalls.Num() > 0 && MaxDraws <= 150.0 && MaxTris <= 350000.0;
	Root->SetBoolField(TEXT("withinCountBudget"), bWithinBudget);

	bPlausible = MeasuredTriangles.Num() > 0 && MeasuredTriangles.Num() == VisibleLod0Triangles.Num();
	int32 MaxVisible = 0;
	for (int32 Index = 0; Index < MeasuredTriangles.Num() && bPlausible; ++Index)
	{
		if (MeasuredTriangles[Index] < VisibleLod0Triangles[Index])
		{
			bPlausible = false;
		}
		MaxVisible = FMath::Max(MaxVisible, VisibleLod0Triangles[Index]);
	}
	if (MaxVisible <= 0)
	{
		bPlausible = false;
	}
	Root->SetBoolField(TEXT("plausibilityOk"), bPlausible);
	Root->SetBoolField(TEXT("coloursOk"), bColoursOk);

	TArray<TSharedPtr<FJsonValue>> Colours;
	for (const FGreyboxColourRow& Row : ColourRows)
	{
		TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("kind"), Row.Kind);
		Item->SetStringField(TEXT("role"), Row.Role);
		Item->SetStringField(TEXT("target"), Row.TargetHex);
		Item->SetStringField(TEXT("pixel"), Row.PixelHex);
		Item->SetNumberField(TEXT("deltaE"), Row.DeltaE);
		Item->SetNumberField(TEXT("x"), Row.X);
		Item->SetNumberField(TEXT("y"), Row.Y);
		Item->SetBoolField(TEXT("inside"), Row.bInside);
		Colours.Add(MakeShared<FJsonValueObject>(Item));
	}
	Root->SetArrayField(TEXT("colours"), Colours);

	TArray<TSharedPtr<FJsonValue>> Shots;
	for (const FString& Name : ShotNames)
	{
		Shots.Add(MakeShared<FJsonValueString>(Name));
	}
	Root->SetArrayField(TEXT("shots"), Shots);

	FString Text;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	FJsonSerializer::Serialize(Root, Writer);
	const FString Path = RepoPath(TEXT("runs/A02/budget.json"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	if (!FFileHelper::SaveStringToFile(Text, *Path))
	{
		UE_LOG(LogMageArena, Error, TEXT("MAGEVR_BUDGET write failed %s"), *Path);
		return;
	}
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_BUDGET path=%s samples=%d drawMax=%.0f triMax=%.0f within=%d plausible=%d colours=%d"),
		*Path, DrawCalls.Num(), MaxDraws, MaxTris, bWithinBudget ? 1 : 0, bPlausible ? 1 : 0, bColoursOk ? 1 : 0);
}

TStatId UGreyboxCaptureDriver::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGreyboxCaptureDriver, STATGROUP_Tickables);
}

bool UGreyboxCaptureDriver::IsTickable() const
{
	return bRunning && GetWorld() != nullptr;
}

UWorld* UGreyboxCaptureDriver::GetTickableGameObjectWorld() const
{
	return GetWorld();
}
