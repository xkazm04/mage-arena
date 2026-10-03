#include "Kernel/Games.h"

#include "Kernel/ArenaKernel.h"
#include "Kernel/Catalog.h"
#include "Kernel/Enemies.h"
#include "Kernel/Geometry.h"
#include "Kernel/MageAI.h"

#include <cmath>

namespace
{
const FArenaTier& TiroTier()
{
	checkf(KernelData().ArenaTiers.Num() > 0, TEXT("arena tiers are empty"));
	return KernelData().ArenaTiers[0];
}

const FComposition* FindPreset(const FString& Name)
{
	for (const FComposition& Preset : KernelData().Presets)
	{
		if (Preset.Name == Name)
		{
			return &Preset;
		}
	}
	return nullptr;
}

FString JsNumber(double Value)
{
	return FString::Printf(TEXT("%g"), Value);
}

void SpawnWave(FGames& Games)
{
	const FKernelData& Data = KernelData();
	const FArenaTier& Tiro = TiroTier();
	checkf(Games.Wave >= 0 && Games.Wave < Tiro.Waves.Num(), TEXT("Invalid Tiro wave"));
	const FArenaWave& Wave = Tiro.Waves[Games.Wave];
	int32 Total = 0;
	for (const FWaveSpawn& Spawn : Wave.Spawns)
	{
		Total += Spawn.bEnemy ? Spawn.Count : 1;
	}
	int32 Index = 0;
	for (const FWaveSpawn& Spawn : Wave.Spawns)
	{
		const int32 Count = Spawn.bEnemy ? Spawn.Count : 1;
		for (int32 N = 0; N < Count; ++N)
		{
			const double JitterX = (ArenaRandom(Games.State, FString::Printf(TEXT("wave %d spawn x"), Games.Wave)) * 2.0 - 1.0) * Data.SpawnJitterM;
			const double JitterY = (ArenaRandom(Games.State, FString::Printf(TEXT("wave %d spawn y"), Games.Wave)) * 2.0 - 1.0) * Data.SpawnJitterM;
			const FSimVec Pos = OpeningPosition(Index, Total, FSimVec{JitterX, JitterY});
			int32 Id = 0;
			FString Kind;
			if (Spawn.bEnemy)
			{
				Id = AddEnemy(Games.State, Spawn.EnemyId, Pos).Id;
				Kind = Spawn.EnemyId;
			}
			else
			{
				// games.ts labels the duelists as water proxies. Fire duels opt in and keep that preset attached.
				const TCHAR* Label = Games.bFireMages ? TEXT("Tiro entrant \u00B7 Ember mage") : TEXT("Tiro entrant \u00B7 Water proxy");
				Id = AddMage(Games.State, 1, Pos, Label).Id;
				const int32 PresetIndex = Games.Wave - 2;
				checkf(Data.OpponentPresets.IsValidIndex(PresetIndex), TEXT("no opponent preset for Tiro wave %d"), Games.Wave);
				const FComposition* Preset = FindPreset(Data.OpponentPresets[PresetIndex]);
				checkf(Preset, TEXT("opponent preset %s is missing"), *Data.OpponentPresets[PresetIndex]);
				FActor* Actor = SimFindActor(Games.State, Id);
				check(Actor);
				Actor->Water = NewWaterState(Preset);
				if (Games.bFireMages)
				{
					Actor->Fire.bSchool = true;
				}
				AttachMageAI(*Actor, Spawn.Competence, Games.State.Tick);
				Kind = Games.bFireMages
					? FString::Printf(TEXT("Ember mage %s"), *JsNumber(Spawn.Competence))
					: FString::Printf(TEXT("Water proxy %s"), *JsNumber(Spawn.Competence));
			}
			FSpawnRecord Record;
			Record.Wave = Games.Wave + 1;
			Record.Id = Id;
			Record.Kind = Kind;
			Record.X = Pos.X;
			Record.Y = Pos.Y;
			Games.SpawnLog.Add(Record);
			++Index;
		}
	}
}

bool FiniteActor(const FActor& Actor)
{
	const double Fields[] = {Actor.Hp, Actor.Mana, Actor.Stamina, Actor.Pos.X, Actor.Pos.Y};
	for (const double Field : Fields)
	{
		if (!std::isfinite(Field))
		{
			return false;
		}
	}
	return Actor.Hp >= 0.0 && Actor.Hp <= Actor.MaxHp
		&& Actor.Mana >= 0.0 && Actor.Mana <= Actor.MaxMana
		&& Actor.Stamina >= 0.0 && Actor.Stamina <= Actor.MaxStamina;
}
}

bool TryCreateGames(FGames& Out, uint32 Seed, const FComposition* Composition, int32 StartWave, bool bReferencePlayer, bool bFireMages)
{
	const FKernelData& Data = KernelData();
	if (Data.ArenaTiers.Num() == 0 || Data.Presets.Num() == 0)
	{
		return false;
	}
	const FArenaTier& Tiro = Data.ArenaTiers[0];
	if (StartWave < 0 || StartWave >= Tiro.Waves.Num())
	{
		return false;
	}
	const FComposition* Used = Composition ? Composition : &Data.Presets[0];
	Out = FGames();
	Out.bFireMages = bFireMages;
	Out.State = CreateArena(Seed);
	Out.PlayerId = AddMage(Out.State, 0, Data.PlayerSpawn, TEXT("Cassia")).Id;
	FActor* Player = SimFindActor(Out.State, Out.PlayerId);
	check(Player);
	Player->Water = NewWaterState(Used);
	if (bReferencePlayer)
	{
		AttachMageAI(*Player, Data.ReferenceCompetence, Out.State.Tick);
	}
	Out.Wave = StartWave;
	Out.Phase = TEXT("active");
	Out.WavesCleared = 0;
	Out.WaveStartTick = Out.State.Tick;
	SpawnWave(Out);
	return true;
}

const FGamesResult* GamesResult(FGames& Games)
{
	if (Games.Result.IsSet())
	{
		return &Games.Result.GetValue();
	}
	if (Games.Phase != TEXT("lost") && Games.Phase != TEXT("complete"))
	{
		return nullptr;
	}
	const FArenaTier& Tiro = TiroTier();
	const int32 Index = Games.WavesCleared - 1;
	FGamesResult Result;
	Result.Kind = Games.Phase == TEXT("complete") ? TEXT("champion") : TEXT("missio");
	Result.WavesCleared = Games.WavesCleared;
	Result.Gold = Index >= 0 && Tiro.PayoutGold.IsValidIndex(Index) ? Tiro.PayoutGold[Index] : 0;
	Result.Renown = Index >= 0 && Tiro.Renown.IsValidIndex(Index) ? Tiro.Renown[Index] : 0;
	Result.bFinalReached = Games.Wave == Tiro.Waves.Num() - 1;
	Result.bFinalWon = Games.Phase == TEXT("complete");
	Games.Result = Result;
	return &Games.Result.GetValue();
}

void StepGames(FGames& Games, const FInputFrame* PlayerInput)
{
	if (Games.Phase != TEXT("active"))
	{
		return;
	}
	TMap<int32, FInputFrame> Inputs = EnemyInputs(Games.State);
	const int32 ActorCount = Games.State.Actors.Num();
	for (int32 Index = 0; Index < ActorCount; ++Index)
	{
		FActor& Actor = Games.State.Actors[Index];
		if (Actor.MageAI.IsSet() && !Actor.bDown)
		{
			Inputs.FindOrAdd(Actor.Id) = MageInput(Games.State, Actor);
		}
	}
	if (PlayerInput)
	{
		Inputs.FindOrAdd(Games.PlayerId) = *PlayerInput;
	}
	StepArena(Games.State, Inputs);
	QueueDeathEffects(Games.State);
	const FActor* Player = SimFindActor(Games.State, Games.PlayerId);
	checkf(Player, TEXT("games player %d is missing"), Games.PlayerId);
	const int32 PlayerTeam = Player->Team;
	if (Player->bDown)
	{
		Games.Phase = TEXT("lost");
		GamesResult(Games);
		return;
	}
	bool bEnemyAlive = false;
	for (const FActor& Actor : Games.State.Actors)
	{
		if (Actor.Team != PlayerTeam && !Actor.bDown)
		{
			bEnemyAlive = true;
			break;
		}
	}
	bool bEnemyHazard = false;
	for (const FTelegraph& Telegraph : Games.State.Telegraphs)
	{
		if (Telegraph.OwnerId != Games.PlayerId)
		{
			bEnemyHazard = true;
			break;
		}
	}
	if (!bEnemyHazard)
	{
		for (const FProjectile& Projectile : Games.State.Projectiles)
		{
			if (Projectile.OwnerId != Games.PlayerId)
			{
				bEnemyHazard = true;
				break;
			}
		}
	}
	if (!bEnemyAlive && !bEnemyHazard)
	{
		const FArenaTier& Tiro = TiroTier();
		Games.WavesCleared = Games.Wave + 1;
		Games.Phase = Games.Wave == Tiro.Waves.Num() - 1 ? TEXT("complete") : TEXT("intermission");
		if (Games.Phase == TEXT("complete"))
		{
			GamesResult(Games);
		}
	}
}

bool TryAdvanceGames(FGames& Games)
{
	if (Games.Phase != TEXT("intermission"))
	{
		return false;
	}
	const FKernelData& Data = KernelData();
	const FArenaTier& Tiro = TiroTier();
	if (Games.Wave + 1 >= Tiro.Waves.Num())
	{
		return false;
	}
	Games.Wave++;
	FActor* Player = SimFindActor(Games.State, Games.PlayerId);
	checkf(Player, TEXT("games player %d is missing"), Games.PlayerId);
	ResetWave(Games.State, *Player);
	Player = SimFindActor(Games.State, Games.PlayerId);
	check(Player);
	Player->Pos = Data.PlayerSpawn;
	Player->PreviousPos = Player->Pos;
	if (Player->MageAI.IsSet())
	{
		const double Level = Player->MageAI->Competence;
		AttachMageAI(*Player, Level, Games.State.Tick);
	}
	FActor Kept = *Player;
	Games.State.Actors.Reset();
	Games.State.Actors.Add(MoveTemp(Kept));
	Games.Phase = TEXT("active");
	Games.WaveStartTick = Games.State.Tick;
	SpawnWave(Games);
	return true;
}

FFightReport RunFight(uint32 Seed, int32 Wave, double MaxSeconds)
{
	const FKernelData& Data = KernelData();
	const FComposition* Preset = FindPreset(Data.ReferencePreset);
	checkf(Preset, TEXT("reference preset %s is missing"), *Data.ReferencePreset);
	FGames Games;
	checkf(TryCreateGames(Games, Seed, Preset, Wave, true), TEXT("Invalid Tiro wave %d"), Wave);
	TArray<bool> DeadAir;
	bool bBucketActive = false;
	const int32 Limit = SimTicks(MaxSeconds);
	const int32 Bucket = SimTicks(Data.DeadAirBucketS);
	while (Games.Phase == TEXT("active") && Games.State.Tick < Limit)
	{
		const int32 EventsBefore = Games.State.Events.Num();
		StepGames(Games);
		bool bNoise = Games.State.Projectiles.Num() > 0 || Games.State.Telegraphs.Num() > 0;
		if (!bNoise)
		{
			for (const FActor& Actor : Games.State.Actors)
			{
				if (Actor.Pending.IsSet())
				{
					bNoise = true;
					break;
				}
			}
		}
		if (!bNoise)
		{
			for (int32 Index = EventsBefore; Index < Games.State.Events.Num(); ++Index)
			{
				const FString& Kind = Games.State.Events[Index].Kind;
				if (Kind == TEXT("cast") || Kind == TEXT("hit"))
				{
					bNoise = true;
					break;
				}
			}
		}
		if (bNoise)
		{
			bBucketActive = true;
		}
		if (Bucket > 0 && Games.State.Tick % Bucket == 0)
		{
			DeadAir.Add(!bBucketActive);
			bBucketActive = false;
		}
	}
	const FActor* Player = SimFindActor(Games.State, Games.PlayerId);
	check(Player);
	FFightReport Report;
	Report.Seed = Seed;
	Report.Wave = Wave + 1;
	Report.Outcome = Games.Phase == TEXT("active") ? TEXT("timeout") : Games.Phase == TEXT("lost") ? TEXT("loss") : TEXT("win");
	Report.DurationS = SimSeconds(Games.State.Tick);
	Report.Hp = Player->Hp;
	Report.Mana = Player->Mana;
	Report.Stamina = Player->Stamina;
	Report.Perfects = Player->Metrics.Perfects;
	Report.IncomingHits = Player->Metrics.Hits;
	Report.DamageDealt = Player->Metrics.DamageDealt;
	Report.DamageTaken = Player->Metrics.DamageTaken;
	Report.Hash = StateHash(Games.State);
	Report.bFinite = true;
	for (const FActor& Actor : Games.State.Actors)
	{
		if (!FiniteActor(Actor))
		{
			Report.bFinite = false;
			break;
		}
	}
	Report.Buckets = DeadAir.Num();
	for (const bool bQuiet : DeadAir)
	{
		if (bQuiet)
		{
			Report.DeadAirBuckets++;
		}
	}
	return Report;
}
