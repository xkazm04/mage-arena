# Registry routing ledger, 2026-10

Which findings of `docs/research/STACK-OPPORTUNITIES-2026-10.md` (F1-F12) and `docs/research/FEASIBILITY-2026-10.md`
(G1-G4) that carry a `- **Registry:**` line have reached the ai-registry, and which have not.

- Checked 2026-10-16 against ai-registry `main` at `2ec4f3a9` (read-only: `git ls-tree`, `git log`, and reading each file).
  `main` there is 10 commits ahead of `origin/main`; "on main" means the local branch of `C:/Users/kazda/kiro/ai-registry`.
- A finding's own status line was not trusted. A ROUTED verdict means the application file exists on main **and** its text
  covers the finding's own case, not only the same subject.
- Paths below are relative to `knowledge/game-production/` in the registry. Commit = last commit touching that path.
- Subject names with no folder on main: `standalone-headset-frame-budgets` (N5) and `tracked-input-record-and-replay` (N6).
  The registry's own schedule (`docs/subject-proposal-immersive-interaction.md`) puts N5 at "V1, 4 Nov: Phase-1 draft",
  N6 at "D4, 31 Oct: draft", and the N1-N8 `forged` reconcile at "19-30 Nov: Phase-2" (line 405).

## Ledger

| Finding | Mapping as the doc states it | Verdict |
|---|---|---|
| F1 Target SDK 32 cannot be uploaded for a new app | EXTENDS `engine-pitfall-corpus`, new incident entry (hotfix raised a packaging default above a store cap) | **ROUTED** `engine-integration/engine-pitfall-corpus/applications/cpp--incident-entry-shape.md`, `54d3bd8c`. Its entry `engine-default-target-sdk-above-store-cap` is this case. |
| F2 The APK would claim Quest 2 / Pro / 3S | EXTENDS `ship-pipeline-gating` (package gate also checks the device list) | **ROUTED** `engine-integration/ship-pipeline-gating/applications/cpp--preflight-before-an-expensive-cook.md`, `3bd58cb9`. Text covers `com.oculus.supportedDevices` and the `MARKER_SUPPORTED_DEVICES` gate. |
| F3 Desktop AA key dead, Quest AA key missing | N5 `standalone-headset-frame-budgets` | **OPEN** subject N5 not on registry main; Phase-1 draft due V1 4 Nov, forged reconcile 19-30 Nov (`docs/subject-proposal-immersive-interaction.md:405`). |
| F4 Mobile HDR on by default; blink vignette needs it | N5 (overdraw and full-screen effects) | **OPEN** same block as F3: N5 not on main. HDR change itself is due at V1, so device evidence is not yet in hand. |
| F5 Pinned combat data not staged into a package | N7 `cross-runtime-rules-conformance` (data pins) | **OPEN** N7 is on main (`systems-canon/cross-runtime-rules-conformance/applications/cpp--pinned-rules-data-across-runtimes.md`, `11037a6c`), but its text covers the hash check on both sides, not staging the pin into the package or the boot-time log. That half needs a paragraph added to the application (or its own), and the packaged exe has not been run (F5 "not proven"). |
| F6 Foveation off, per project only | N5 | **OPEN** N5 not on main; the change is V1 and not headless-verifiable. |
| F7 Keep hand tracking LOW, stop assuming 72 Hz | N2 `hand-tracked-timing-windows` | **ROUTED** `couch-and-tv-play/immersive-interaction/hand-tracked-timing-windows/applications/cpp--sensor-latency-inside-the-window-budget.md`, `f1c273eb` (held and extrapolated 30 Hz models, `HeldSample.Latency`); also `.../cpp--onset-recovered-from-tracking-history.md`, `f1c273eb`. Both are `status: draft`. The V2 FMM A/B in F7 is not covered and has no evidence yet. |
| F8 System gesture must not fire a game action | N1 `drawn-gesture-command-recognition` (the reject class) | **OPEN** `.../drawn-gesture-command-recognition/applications/cpp--reject-class-recognition.md` (`13d39368`) is about the sigil floor; it has no text on a reserved system gesture or `FHandFrame::bSystemGesture`. Wiring is V1, no device evidence. The mapping may be wrong: it needs a decision on whether N1 or a different subject owns it. |
| F9 Engine moved to 5.8.3 under a 5.8.2 record | EXTENDS `engine-pitfall-corpus` | **ROUTED** `engine-integration/engine-pitfall-corpus/applications/cpp--provenance-on-every-entry.md`, `770c1e85`. Entry `prebuilt-plugin-build-id-vs-engine`, probed on 5.8.2 and 5.8.3. |
| F10 AppSW on 5.8 is engine frame synthesis | N5 | **OPEN** N5 not on main; the finding itself is "none now", only for V1 if over budget. |
| F11 JDK 22 is not Epic's documented JDK 21 | EXTENDS `ship-pipeline-gating` (check toolchain against documented versions before the first package) | **OPEN** no application in `ship-pipeline-gating` covers a toolchain version check (the only Gradle text is `kotlin--ship-only-what-the-runtime-reads.md`, a different rule). Evidence only after the first package run; the finding is docs-only so far. |
| F12 5.8.4 hotfix announced; freeze-or-take due | EXTENDS `engine-pitfall-corpus`, same incident as F9 | **ROUTED** same file as F9, `cpp--provenance-on-every-entry.md`, `770c1e85`. The 5.8.4 decision (`docs/DECISIONS.md`, 2026-10-13) is not applied yet, so the entry has no 5.8.4 row; the incident class it routes is the same. |
| G1 Runtime extrapolates by default; held model is the worst case | N2 | **ROUTED** `.../hand-tracked-timing-windows/applications/cpp--onset-recovered-from-tracking-history.md`, `f1c273eb` (onset error, held and extrapolated 30 Hz, line 77), and `cpp--sensor-latency-inside-the-window-budget.md`, `f1c273eb`. Both draft; model figures only, no device. |
| G2 v207 path never exposes camera sample time | N6 `tracked-input-record-and-replay` and N2 | **OPEN** subject N6 not on registry main (proposal: draft D4, 31 Oct). The N2 half only notes the day-one probe as pending (`cpp--onset-recovered-from-tracking-history.md:101`). Data needs the V1 device probe. |
| G3 Manifest says controllers supported, game has no controller path | EXTENDS `ship-pipeline-gating` (assert the manifest's input declaration) | **OPEN** `cpp--preflight-before-an-expensive-cook.md` is the device list, not the input declaration; no other application covers it. The change is V1, after hands work on device. |
| G4 MAX equals HIGH; HIGH costs headroom, 50 Hz on 50 Hz mains | N2 and N5 `standalone-headset-frame-budgets` | **OPEN** N5 not on main (see F3); no N2 application mentions FMM or the 50 Hz case. A/B is V2, device only. |

## Count

mapped 16, routed 6, open 10, declined 0.

Routed: F1, F2, F7, F9, F12, G1. Open: F3, F4, F5, F6, F8, F10, F11, G2, G3, G4.

Open by blocker: N5 not on main (F3, F4, F6, F10, G4); device or package evidence missing (F11, G2, G3); application text
does not cover the case (F5, F8).

## Not covered by this ledger

Registry applications on main that this repo's findings do not map to (for example `cpp--stroke-plane-projection.md`,
`cpp--vocabulary-distinctiveness-audit.md`, `cpp--tracking-loss-as-a-game-state.md`,
`cpp--introspect-before-you-call.md`) are not rows here, because no finding carries a `Registry:` line for them.
