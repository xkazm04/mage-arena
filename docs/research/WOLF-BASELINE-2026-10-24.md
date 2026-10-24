# Wolf baseline, 2026-10-24

The first UE run on Wolf since 2026-10-16 (WOLF-PLAN P0). Every later card compares per test against this file.

- **Host:** Wolf, Windows hostname `kaziho` (x64). The hostname is not literally `Wolf` (WOLF-PLAN section b asks for both).
- **Engine:** UE 5.8.3, `Build.version` PatchVersion 3, Changelist 58210709, CompatibleChangelist 55116800, branch
  `++UE5+Release-5.8`, at `C:\Program Files\Epic Games\UE_5.8`.
- **Toolchain:** Visual Studio Community 2022 17.14.23 (vswhere, NativeGame + NativeDesktop workloads; the Clang
  component is present), clang-format 19.1.5, Node v24.14.0. Meta XR and Meta Interaction plugins 1.207.0, BuildId
  55116800, copied into the worktree from the main checkout.
- **Commit:** `e321455` (master tip; the run was on a clean worktree of it, branch
  `autopilot/accepted-idea-delivery-2ab3fe9b`), sigil corpus regenerated with `generate.mjs --corpus 30 --seed 1000` and
  `mousepaths.mjs` first.
- **Build:** `apps/vr/tools/build.ps1`, cold, with the plugins: `Result: Succeeded`, `Total execution time: 419.88 seconds`.
- **Commands:** one `UnrealEditor-Cmd ... -ExecCmds="Automation RunTests <filter>;Quit" -unattended -nullrhi -nosplash -log
  -ReportExportPath=...` per filter (WOLF-PLAN section d). Verdicts below are read from the `-ReportExportPath` JSON. Raw
  logs and JSON: `runs/wolf/baseline-2026-10-24/` (gitignored).
- **Boot log:** `kernel data: pinned dir .../apps/vr/data/pinned/, 9 files, SHA1 11F9B16CBBC77C2469704829C518E4106236C2FF`.

## Verdict

- `MageArena.`: **244 / 244 pass** (6 pass with a warning: four are the corrupt-file tests logging the corruption they
  provoke, and `Hands.HeldSample.CorpusModels` and `Sigils.Accuracy` log an offline HTTP probe timeout). 45 / 45
  `MageArena.Kernel.Conformance.*`, `MageArena.Fire.Golden` green. The last count on record was 234 / 234 (2026-10-08).
- `MageArenaDesign.`: **red in exactly the five known tests** (`Duel.FireSeated`, `Duel.AirSeated`, `Session.FullSeated`,
  `Session.Wave1Seated`, `Session.CreaturesSeated`), `Census.Rivals` and `Census.Sweep` green. No sixth red.
- Node gates (section d): `pin OK: 9 files @ baeac66`, `conformance OK: 45 vectors`, check-pin self-test detects both
  corruptions, agy-guard pass 40 fail 0, `unused-members OK: 555 private/protected members in 39 classes`, its self-test OK,
  `format-check OK: 0 changed C++ file(s)`, `actions pinned: 6 refs in 3 workflows`, `check-commits OK`, `staged clips OK:
  111 files, 25776936 bytes`, `overlay OK: 14 files` / `overlay sha256 d0ee5333db8992db01580107aa45ddd2c7ce0d016a63427129e17896947daecd`
  and its self-test, leonardo-redaction pass 24 fail 0, `repo-law OK: 4/4 sub-checks`.

P0's "Not done when" does not hold, so P1 and P2 may start.

## MageArenaDesign. (7 tests, 334.9 s in the suite, editor process 350 s)

| Test | Result | Duration s |
|---|---|---|
| MageArenaDesign.Census.Rivals | pass | 75.226 |
| MageArenaDesign.Census.Sweep | pass (warning) | 254.493 |
| MageArenaDesign.Duel.AirSeated | **FAIL** | 1.081 |
| MageArenaDesign.Duel.FireSeated | **FAIL** | 1.385 |
| MageArenaDesign.Session.CreaturesSeated | **FAIL** | 1.068 |
| MageArenaDesign.Session.FullSeated | **FAIL** | 0.988 |
| MageArenaDesign.Session.Wave1Seated | **FAIL** | 0.656 |

First error line of each red test (verbatim from the -ReportExportPath JSON):

- Duel.AirSeated: `Expected 'length 26.42s in 45-80' to be true.`
- Duel.FireSeated: `Expected 'brennic-c1 length 32.88s in 45-80' to be true.`; `Expected 'brennic-c1 player wins (phase lost)' to be true.`; `Expected 'brennic-c15 length 22.92s in 45-80' to be true.`
- Session.CreaturesSeated: `Expected 'victory inside 30-45s window (measured 45.02s, phase intermission)' to be true.`
- Session.FullSeated: `Expected 'the script did not lose bout 1' to be false.`; `Expected 'the day closes' to be "closed", but it was "lost".`; `Expected 'all four bouts won' to be 4, but it was 0.`
- Session.Wave1Seated: `Expected 'phase' to be "intermission", but it was "lost".`; `Expected 'waves cleared' to be true.`; `Expected 'victory within 120s (measured 23.25s, phase lost)' to be true.`

## MageArena. (244 tests, 114.1 s in the suite, editor process 170 s)

| Test | Result | Duration s |
|---|---|---|
| MageArena.Air.Deflect | pass | 0.042 |
| MageArena.Air.FormEvasion | pass | 0.008 |
| MageArena.Air.Literals | pass | 0.008 |
| MageArena.Air.Mind | pass | 0.008 |
| MageArena.Air.Momentum | pass | 0.008 |
| MageArena.Air.NothingElseMoves | pass | 0.008 |
| MageArena.Air.Rows | pass | 0.008 |
| MageArena.Air.SquallTicks | pass | 0.008 |
| MageArena.Air.StrictLoad | pass | 0.033 |
| MageArena.Air.TempestSignature | pass | 0.008 |
| MageArena.Air.Thresholds | pass | 0.008 |
| MageArena.Air.VeerArc | pass | 0.008 |
| MageArena.Audio.Assets | pass | 0.022 |
| MageArena.Audio.EventMap | pass | 0.021 |
| MageArena.Audio.SquallCap | pass | 0.009 |
| MageArena.Blink.Clips | pass | 0.555 |
| MageArena.Blink.Stream | pass | 0.037 |
| MageArena.Blink.Timing72 | pass | 0.035 |
| MageArena.Budget.FxAssets | pass | 0.914 |
| MageArena.Budget.FxPlan | pass | 0.008 |
| MageArena.Budget.ParticleCount | pass | 0.008 |
| MageArena.Budget.SharedMaterials | pass | 0.008 |
| MageArena.Budget.StressPopulation | pass | 0.008 |
| MageArena.Budget.ZeroIsNotAPass | pass | 0.008 |
| MageArena.Clips.DeterministicReplay | pass | 0.422 |
| MageArena.Clips.LoadAll | pass | 0.331 |
| MageArena.Clips.NoEmitAfterEnd | pass | 0.008 |
| MageArena.Clips.QuickActionPath | pass | 0.658 |
| MageArena.Collar.EachSourceOnce | pass | 0.008 |
| MageArena.Collar.LifetimeSurvives | pass (warning) | 0.820 |
| MageArena.Collar.MissedCasts | pass | 0.008 |
| MageArena.Collar.NullPathSilent | pass | 0.008 |
| MageArena.Collar.ReflectedHit | pass | 0.008 |
| MageArena.Collar.TabletColumns | pass | 0.505 |
| MageArena.Collar.UnblockableDodge | pass | 0.008 |
| MageArena.Collar.Weights | pass | 0.008 |
| MageArena.Colour.CreatureSchools | pass | 0.008 |
| MageArena.Colour.HoundEmber | pass | 0.009 |
| MageArena.Colour.Schools | pass | 0.008 |
| MageArena.Colour.StyleOverride | pass | 0.008 |
| MageArena.Colour.Unblockable | pass | 0.008 |
| MageArena.Creatures.DeathEmber | pass | 0.008 |
| MageArena.Creatures.NoRulesBite | pass | 0.008 |
| MageArena.Creatures.SpitEmberPerfect | pass | 0.008 |
| MageArena.Creatures.SpitFromHoldLine | pass | 0.008 |
| MageArena.Creatures.SpitLoads | pass | 0.009 |
| MageArena.Defences.Dome | pass | 0.008 |
| MageArena.Defences.FireWallBolt | pass | 0.008 |
| MageArena.Defences.FireWallBurn | pass | 0.008 |
| MageArena.Defences.FireWallDuration | pass | 0.008 |
| MageArena.Defences.FireWallNull | pass | 0.008 |
| MageArena.Defences.FireWallOwnBolt | pass | 0.008 |
| MageArena.Defences.FireWallPhysical | pass | 0.008 |
| MageArena.Defences.FireWallReaction | pass | 0.008 |
| MageArena.Defences.FireWallUnblockable | pass | 0.008 |
| MageArena.Defences.FireWallVolley | pass | 0.008 |
| MageArena.Defences.PinnedPath | pass | 0.008 |
| MageArena.Defences.PlantedHands | pass | 0.008 |
| MageArena.Defences.PlantedMana | pass | 0.008 |
| MageArena.Defences.Proposals | pass | 0.008 |
| MageArena.Defences.SplitBolt | pass | 0.008 |
| MageArena.Defences.SplitBoltAtFullFlow | pass | 0.008 |
| MageArena.Defences.SplitKeysStrict | pass | 0.076 |
| MageArena.Defences.SplitLatchClearsOnIdle | pass | 0.008 |
| MageArena.Defences.SplitLatchStillOnWhileCasting | pass | 0.008 |
| MageArena.Defences.SplitLine | pass | 0.008 |
| MageArena.Defences.SplitLineAtFullFlowRefused | pass | 0.008 |
| MageArena.Defences.StaffClips | pass | 0.084 |
| MageArena.Defences.TierAndCrest | pass | 0.008 |
| MageArena.Defences.WardWhileSigil | pass | 0.457 |
| MageArena.Fire.Gating | pass | 0.008 |
| MageArena.Fire.Golden | pass | 0.008 |
| MageArena.Fire.Heat | pass | 0.008 |
| MageArena.Fire.Shapes | pass | 0.008 |
| MageArena.Fire.StepCost | pass | 0.008 |
| MageArena.Fire.Sunfall | pass | 0.008 |
| MageArena.Fire.SunfallBeat | pass | 0.008 |
| MageArena.Fire.Thresholds | pass | 0.008 |
| MageArena.Fire.WrathWard | pass | 0.008 |
| MageArena.Greybox.Layout | pass | 0.009 |
| MageArena.Greybox.ThreatColours | pass | 0.008 |
| MageArena.Hands.BlinkVignette.Component | pass | 0.009 |
| MageArena.Hands.BlinkVignette.Mapping | pass | 0.008 |
| MageArena.Hands.BlinkVignette.NoPostProcess | pass | 0.008 |
| MageArena.Hands.HeldSample.Blink | pass | 1.333 |
| MageArena.Hands.HeldSample.BlinkModels | pass | 3.886 |
| MageArena.Hands.HeldSample.CorpusModels | pass (warning) | 28.044 |
| MageArena.Hands.HeldSample.Latency | pass | 0.061 |
| MageArena.Hands.HeldSample.ModelsAreReal | pass | 0.356 |
| MageArena.Hands.HeldSample.SigilCorpus | pass | 18.496 |
| MageArena.Hands.HeldSample.SigilLines | pass | 2.176 |
| MageArena.Hands.HeldSample.Staff | pass | 2.687 |
| MageArena.Hands.HeldSample.TransformIsReal | pass | 0.268 |
| MageArena.Hands.HeldSample.Ward | pass | 0.672 |
| MageArena.Hands.HeldSample.WardModels | pass | 2.015 |
| MageArena.Hands.SystemGesture.GateIsTheCause | pass | 0.673 |
| MageArena.Hands.SystemGesture.SchemaV1Closed | pass | 0.070 |
| MageArena.Hands.SystemGesture.ZeroEvents | pass | 0.651 |
| MageArena.Input.CaptureFlags | pass | 0.169 |
| MageArena.Kernel.Conformance.bolt-hit | pass | 0.010 |
| MageArena.Kernel.Conformance.bot-duel | pass | 0.015 |
| MageArena.Kernel.Conformance.creature-bout | pass | 0.028 |
| MageArena.Kernel.Conformance.duel-competence-1 | pass | 0.025 |
| MageArena.Kernel.Conformance.duel-competence-1-5 | pass | 0.031 |
| MageArena.Kernel.Conformance.dummy-wave | pass | 0.042 |
| MageArena.Kernel.Conformance.flow-crest | pass | 0.012 |
| MageArena.Kernel.Conformance.held-ward | pass | 0.014 |
| MageArena.Kernel.Conformance.lash-1 | pass | 0.010 |
| MageArena.Kernel.Conformance.lash-2a | pass | 0.010 |
| MageArena.Kernel.Conformance.lash-2b | pass | 0.009 |
| MageArena.Kernel.Conformance.lash-3 | pass | 0.010 |
| MageArena.Kernel.Conformance.lash-4 | pass | 0.009 |
| MageArena.Kernel.Conformance.mage-react | pass | 0.011 |
| MageArena.Kernel.Conformance.mana-nerves | pass | 0.009 |
| MageArena.Kernel.Conformance.mend-1 | pass | 0.010 |
| MageArena.Kernel.Conformance.mend-2 | pass | 0.008 |
| MageArena.Kernel.Conformance.mend-3 | pass | 0.011 |
| MageArena.Kernel.Conformance.mend-4 | pass | 0.011 |
| MageArena.Kernel.Conformance.mire-1 | pass | 0.015 |
| MageArena.Kernel.Conformance.mire-2 | pass | 0.008 |
| MageArena.Kernel.Conformance.mire-3 | pass | 0.008 |
| MageArena.Kernel.Conformance.mire-4 | pass | 0.009 |
| MageArena.Kernel.Conformance.mirror-1 | pass | 0.008 |
| MageArena.Kernel.Conformance.mirror-2a | pass | 0.009 |
| MageArena.Kernel.Conformance.mirror-2b | pass | 0.012 |
| MageArena.Kernel.Conformance.mirror-3 | pass | 0.010 |
| MageArena.Kernel.Conformance.mirror-4 | pass | 0.011 |
| MageArena.Kernel.Conformance.perfect-advance | pass | 0.011 |
| MageArena.Kernel.Conformance.perfect-inside | pass | 0.010 |
| MageArena.Kernel.Conformance.perfect-outside | pass | 0.009 |
| MageArena.Kernel.Conformance.physical-ward | pass | 0.010 |
| MageArena.Kernel.Conformance.reset-waves | pass | 0.012 |
| MageArena.Kernel.Conformance.soldier-wave | pass | 0.010 |
| MageArena.Kernel.Conformance.soldiers-bout | pass | 0.033 |
| MageArena.Kernel.Conformance.tide-orb-1 | pass | 0.009 |
| MageArena.Kernel.Conformance.tide-orb-2 | pass | 0.009 |
| MageArena.Kernel.Conformance.tide-orb-3 | pass | 0.008 |
| MageArena.Kernel.Conformance.tide-orb-4a | pass | 0.008 |
| MageArena.Kernel.Conformance.tide-orb-4b | pass | 0.009 |
| MageArena.Kernel.Conformance.tier-clock | pass | 0.009 |
| MageArena.Kernel.Conformance.tiro-ladder | pass | 0.016 |
| MageArena.Kernel.Conformance.tiro-sequence | pass | 0.068 |
| MageArena.Kernel.Conformance.unblockable-ward | pass | 0.010 |
| MageArena.Kernel.Conformance.wave-reset | pass | 0.013 |
| MageArena.Kernel.Absorb | pass | 0.008 |
| MageArena.Kernel.Catalog | pass | 0.008 |
| MageArena.Kernel.Clock | pass | 0.008 |
| MageArena.Kernel.Competence | pass | 0.008 |
| MageArena.Kernel.Determinism | pass | 0.017 |
| MageArena.Kernel.EnemyBouts | pass | 0.008 |
| MageArena.Kernel.EnemyStepCost | pass | 0.009 |
| MageArena.Kernel.FireCatalog | pass | 0.008 |
| MageArena.Kernel.Flow | pass | 0.008 |
| MageArena.Kernel.Games | pass | 0.020 |
| MageArena.Kernel.Geometry | pass | 0.009 |
| MageArena.Kernel.LoadNamesColumn | pass | 0.009 |
| MageArena.Kernel.LoadNamesEnemy | pass | 0.008 |
| MageArena.Kernel.LoadRejectsUnknownReference | pass | 0.029 |
| MageArena.Kernel.LoadSeam | pass | 0.014 |
| MageArena.Kernel.MageStepCost | pass | 0.008 |
| MageArena.Kernel.Movement | pass | 0.008 |
| MageArena.Kernel.Performance | pass | 0.008 |
| MageArena.Kernel.PinHash | pass | 0.021 |
| MageArena.Kernel.Projectiles | pass | 0.008 |
| MageArena.Kernel.Rng | pass | 0.008 |
| MageArena.Kernel.Roster | pass | 0.008 |
| MageArena.Kernel.Staff | pass | 0.008 |
| MageArena.Kernel.StateHashCoverage | pass | 0.008 |
| MageArena.Kernel.StateHashFieldGuard | pass | 0.008 |
| MageArena.Kernel.StepCost | pass | 0.008 |
| MageArena.Kernel.Stepper | pass | 0.008 |
| MageArena.Kernel.Sustain | pass | 0.008 |
| MageArena.Kernel.Timing | pass | 0.008 |
| MageArena.Kernel.Ward | pass | 0.008 |
| MageArena.Kernel.WaterGeometry | pass | 0.008 |
| MageArena.Pause.FreezesKernel | pass | 0.673 |
| MageArena.Pause.ResumeCount | pass | 0.700 |
| MageArena.Pause.SaveContinue | pass | 0.675 |
| MageArena.Pause.SaveRejects | pass (warning) | 0.650 |
| MageArena.Pause.TeachResumes | pass | 0.621 |
| MageArena.Pause.TrackingLoss | pass | 0.659 |
| MageArena.PlayerPower.Method | pass | 0.008 |
| MageArena.PlayerPower.Paths | pass | 0.008 |
| MageArena.Preset.BadFileFailsStart | pass | 0.661 |
| MageArena.Preset.LeviathanOrb | pass | 0.008 |
| MageArena.Preset.NoPickKeepsB | pass | 0.625 |
| MageArena.Preset.OpponentsKeepPinned | pass | 0.631 |
| MageArena.Preset.PlayerHasReflection | pass | 0.585 |
| MageArena.Preset.StonePickSaves | pass (warning) | 1.858 |
| MageArena.Reflection.ReturnsToCaster | pass | 0.008 |
| MageArena.Reflection.RippleAndNullPath | pass | 0.008 |
| MageArena.Reflection.TierGate | pass | 0.008 |
| MageArena.Replay.Deterministic | pass | 1.481 |
| MageArena.Rivals.DoubleBreak | pass | 0.008 |
| MageArena.Rivals.GrantedSunfall | pass | 0.008 |
| MageArena.Rivals.Loads | pass | 0.089 |
| MageArena.Rivals.NullPath | pass | 0.025 |
| MageArena.Rivals.Once | pass | 0.008 |
| MageArena.Rivals.Surge | pass | 0.008 |
| MageArena.Rivals.Thresholds | pass | 0.008 |
| MageArena.Session.ChainAdvance | pass | 0.604 |
| MageArena.Session.ChainDefeat | pass | 0.631 |
| MageArena.Session.DayFinalDefeat | pass | 0.660 |
| MageArena.Session.DayFinalOpponent | pass | 0.742 |
| MageArena.Session.DayFlags | pass | 0.633 |
| MageArena.Session.DayOrderFirstLaunch | pass | 0.800 |
| MageArena.Session.DayOrderResume | pass | 0.604 |
| MageArena.Session.DayPrologue | pass | 1.261 |
| MageArena.Session.DayRitualHold | pass | 1.211 |
| MageArena.Session.DayRitualTimeout | pass | 0.608 |
| MageArena.Session.DaySaveEachBout | pass | 2.502 |
| MageArena.Session.DayStrings | pass | 0.008 |
| MageArena.Session.DeathBurstPerfect | pass | 0.570 |
| MageArena.Session.Defeat | pass | 0.606 |
| MageArena.Session.SaveFailureLogged | pass | 0.600 |
| MageArena.Settings.File | pass (warning) | 0.019 |
| MageArena.Settings.Gentle | pass | 1.531 |
| MageArena.Settings.Mirror | pass | 2.828 |
| MageArena.Settings.Narrow | pass | 1.694 |
| MageArena.Settings.OverlayLog | pass | 0.008 |
| MageArena.Sigils.Accuracy | pass (warning) | 11.013 |
| MageArena.Sigils.EndToEnd | pass | 0.606 |
| MageArena.Sigils.NoFalseCasts | pass | 2.254 |
| MageArena.Sigils.Templates | pass | 1.208 |
| MageArena.Teach.ClipsInOrder | pass | 0.656 |
| MageArena.Teach.MissRepeats | pass | 0.689 |
| MageArena.Teach.PerfectOrThree | pass | 0.651 |
| MageArena.Teach.Skip | pass | 0.584 |
| MageArena.VrOverlay.DaisClear | pass | 0.008 |
| MageArena.VrOverlay.Loads | pass | 0.008 |
| MageArena.VrOverlay.NoRulesMelee | pass | 0.008 |
| MageArena.VrOverlay.ThrownSpearWardAndBlink | pass | 0.008 |
| MageArena.Ward.AlreadyRaised | pass | 0.009 |
| MageArena.Ward.ElevatedThreatFacing | pass | 0.008 |
| MageArena.Ward.FirstRaiseFidget | pass | 0.010 |
| MageArena.Ward.HeldNeverPerfect | pass | 0.008 |
| MageArena.Ward.HoldChord | pass | 0.015 |
| MageArena.Ward.KeyHoldPath | pass | 0.012 |
| MageArena.Ward.KindsAndArc | pass | 0.030 |
| MageArena.Ward.LapStillOnset | pass | 0.012 |
| MageArena.Ward.OnsetAfterLower | pass | 0.032 |
| MageArena.Ward.OnsetCompensation | pass | 0.014 |
| MageArena.Ward.ResumeAfterRelease | pass | 0.039 |
| MageArena.Ward.SettleOnset | pass | 0.015 |
