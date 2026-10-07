# T20 - Cassia's preset: Mirror Reflection, and the Leviathan pick on the stones

Status: open
Max turns: 280

## Goal
Owner decision 2026-10-07 (`docs/DECISIONS.md`, "v2 vision accepted"; `docs/design/V2-VISION.md` wave 3 "signature
moments"). The pitch promises **Mirror Reflection** and the **Leviathan Orb**, but the player gets the pinned
`Rotation` preset: Mirror II branch B (Ripple) and Tide Orb IV branch B (Rain of Orbs), and the kernel parses
Reflection without implementing it. After this card the player plays a **VR-owned preset** (`cassia`): Mirror II
branch **A, Reflection** (a perfect absorb reflects the projectile back at its caster), and at the **first
intermission** the player picks Tide Orb IV on the stones: **Leviathan Orb (A)** or **Rain of Orbs (B)**. Opponents
keep the pinned presets.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md`, `docs/design/V2-ROADMAP.md` (lane A row 4), `docs/gameplay/02-verbs-and-gestures.md`,
  `05-schools/` (Water), `docs/campaign/04-vr-campaign-design.md` section 3 (choices by gesture: palm over a stone,
  0.40 s hold, `teach.json` `offerHoldS`, never blocking).
- Data: pinned `spells-water.csv` (row 6 Leviathan Orb: slow projectile 6 m/s r 1.5 m, UNBLOCKABLE, telegraph 1.00,
  "the orb's path is drawn; leave it or break the caster"; row 7 Rain of Orbs; row 22 Reflection: "passive while
  slotted: a perfect absorb reflects the projectile", note "only projectiles of tier <= own unlocked tier"), pinned
  `runtime.json` `water.presets` and `games.referencePreset`, `apps/vr/data/vr/strings.json`, `teach.json`.
- Code: `Kernel/KernelData.cpp` (~605 branch effect mapping), `Kernel/Water.cpp` (`ReflectProjectile` ~369, which emits
  no event today), `Kernel/ArenaThreats.cpp` (perfect ~294-317), `Session/ArenaSession.cpp` (`FindRotation` ~35/240,
  `PollComfortToggles` ~490-559: the three stones in `cold`), `Session/SessionFlow.cpp` (`FindRotationPreset` ~50 and
  its uses ~269/299/889/893, `AdvanceArc` ~1237, save file `bout.txt` ~177-257), `Session/SessionPresentation.cpp`
  (stones ~943-1015), `Tests/SessionTests.cpp`.

## Deliverables
1. **VR preset data** `apps/vr/data/vr/presets.json`: `cassia` = the pinned `Rotation` lines and branches except Mirror
   II = A and Tide Orb IV = the stone pick (default **B**, the pinned branch, so a player who never picks plays today's
   orb). `_why` lines cite the DECISIONS entry. The session builds the player from it instead of the hard-coded
   `Rotation` lookup; opponents still use the pinned presets. A missing or bad file fails the session start, like the
   overlay.
2. **Reflection** (kernel): with Mirror II A slotted, a perfect absorb of an enemy projectile whose tier is <= the
   player's unlocked tier sends it back at its caster (reuse `ReflectProjectile`); a reflected projectile can hit and
   damage its caster with its own damage; emit a new `reflect` event (actor = reflector, target = original caster,
   value = projectile tier). Ripple stays as it was for branch B. If Reflection changes the null-ruleset path or any
   conformance vector (it should not: the pinned presets use B), stop and report.
3. **Leviathan Orb** check: confirm the kernel casts row 6 as specified (6 m/s, r 1.5 m, unblockable, 1.00 s telegraph,
   45 mana, 40 damage) and the greybox draws its path. Fix what is missing; list what you fixed.
4. **The pick on the stones**: in the intermission after Bout 1, the left and right stones show "Leviathan" and
   "Rain of Orbs" (labels in `strings.json`, 5 words or fewer), the centre stone stays "continue". A palm held over a
   stone for `offerHoldS` picks it (same detection as the comfort stones; on desktop through played clips, never a key
   shortcut), the stone glows during the hold, and if nothing is picked in 20 s the default stays and the run continues.
   The pick is saved (extend `bout.txt` with `tideOrbIV=A|B`, old files without the line still load) and applies from
   Bout 2 on.
5. **Tests**: regular `MageArena.Preset.*` and `MageArena.Reflection.*`: the player preset has Mirror II A; a perfect on
   a tier-1 projectile at player tier 2 reflects it and the caster takes its damage; a tier-3 projectile at player tier 2
   is not reflected (absorbed normally); `reflect` is emitted; the stone pick sets branch A and survives save and load;
   no pick in 20 s keeps B; opponents still use Ripple. Report `MageArenaDesign.Duel` and `MageArenaDesign.Session`
   before and after (Reflection changes the duel: measure, do not tune).
6. **Capture** into `runs/T20/shots/`: the intermission stones with the two labels, a reflected fire bolt flying back
   at the Fire mage, a Leviathan Orb in flight with its drawn path. Look at each and describe it.

## Constraints
- Do not commit, push, or touch any repository other than this one. No paid services. Do not edit
  `docs/DECISIONS.md`, `apps/vr/data/pinned/` or `calibration-proposal.json`.
- Conformance stays byte-identical. Keys play clips only; debug keys stay F-keys.
- Regenerate the sigil corpus as `CLAUDE.md` says before the full suite in a fresh checkout.
- One Unreal build at a time; one `Automation RunTests` filter per editor process. Do not end your turn while a build,
  test or capture is running. Never write placeholder evidence.

## Acceptance (the orchestrator re-runs these and looks at every still)
```
node apps/vr/tools/conformance/generate.mjs && git status --porcelain apps/vr/data/conformance   # no output
powershell -NoProfile -File apps/vr/tools/build.ps1
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArena.;Quit" -unattended -nullrhi -nosplash -log
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArenaDesign.Duel;Quit" -unattended -nullrhi -nosplash -log
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArenaDesign.Session;Quit" -unattended -nullrhi -nosplash -log
<the capture command you add or extend>   # stills in runs/T20/shots/
```

## Report
Write `runs/T20/REPORT.md`: files, every acceptance command with real output, the design numbers before and after,
a description of each still, and every decision the card did not specify.
