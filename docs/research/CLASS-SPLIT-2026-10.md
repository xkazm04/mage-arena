# The section 7 budget by content class: who pays when a row fails

**Label: desk audit of the plan and the D-G4 record. No device reading, no config or code change.** Base: `4e95212`
on `master`. Registry technique: `per-content-class-frame-budget` (subject `standalone-headset-frame-budgets`).

## 1. The question

The budget table (`docs/PROJECT-PLAN.md:358-371`) states one frame time for everything: 13.9 ms at 72 Hz. Only one
stage is split off (game thread ≤6 ms, with the kernel and recognizer as system shares inside it). The count rows are
shared by every content class except one: the particle row belongs to effects. The technique says a budget in that
shape gives every class the whole frame. It also says the first time anyone learns the classes share it is when a row fails.

**Chosen to falsify.** The count rows are already partly class-shaped. The particle row is an effects row, and the
triangle row names its classes ("hands + arena + 20 actors"). If those rows had already done a class split's job, the
technique would be redundant here. The test is the three times a row came under pressure in the D-G4 record. Did the
budget say which class had to cut?

- **Target:** of the three D-G4 budget events, how many the budget can attribute to a class share; and on how many
  quantities the binding headset is named, with the reason.
- **Floor:** every D-G4 verdict and figure stays as recorded. This audit changes no config, no code and no plan row.

## 2. The classes and their desk work counts

Desk work counts may **rank** classes. They do not set device shares. Figures are from the D-G4 record
(`BUDGET-DG4-2026-10.md`) and the A02 baseline (`BASELINE-2026-10-07.md:388-389`), desktop proxy.

| Class | What it is in the stress scene | Per-part components | Desk draws | Desk LOD0 triangles | Own count row |
|---|---|---|---|---|---|
| Environment | `AArenaGreybox` (T42 is to replace it with the arena kit) | ISM shell | 30 (A02, empty greybox) | max 51k (A02) | none |
| Characters | 20 enemy bodies | 20 | together with projectiles, HP bars and hands: +280 over the empty arena (D-G4, occlusion on) | together with the same: +73k at the median (124.5k vs 51k) | none |
| Projectiles (a class this game adds) | 100 shots, 75 spheres and 25 spears | 100 | (in the +280) | (in the +73k) | none |
| Interface | 40 HP bar parts (back and fill), Slate, later the sigil trail, wrist cues, the blink vignette | 40 | (in the +280); Slate 2 | (in the +73k) | none |
| Hands | 2 presentations, 102 ISM instances | ISM | (in the +280) | (in the +73k) | none |
| Effects | 6 Niagara systems (section 10) | - | +11 at the median (fx 194 vs ship 183) | sprites only | **particles ≤2,000, GPU emitters ≤1** |

Per-part components drove the draw count before lever B, one MID per part. Projectiles are 62.5% of them, HP bars 25%
and enemy bodies 12.5%. So the class that owns most of the draw-call overrun is projectiles. HP bars, an interface
element, are the second largest. Both are presented by characters' code, and that is why the row read as a character
cost.

## 3. Arm A: the three events under the budget as written

| # | Event | What the budget said | How it was resolved |
|---|---|---|---|
| E1 | D-G4, 2026-10-07: draw calls median 310 against ≤150 (section 3) | One shared row; fail. No class held a share, so no one was named to cut. | An App Master decision (7.7). Lever B went to lane A **because lane A owns `SessionPresentation.cpp`**, by file ownership, not by class. |
| E2 | Lever A shipped (section 8): occlusion queries off for every platform | The triangle row passed before and after. Nothing in the table records a cost. | Paid out of the triangle row's headroom: all-pass max 293,696 → 321,536 (84% → 92% of 350k), BasePass median +10k. That headroom belonged to environment, characters and hands jointly. None of them was asked. |
| E3 | Section 10: six Niagara systems added | Effects passed **their own** row (1,468 of 2,000, 73%). | They added 11 draws at the median to a row already failing. The verdict reads "fail, as in 8.5". The newest class paid nothing for it. |

**Arm A attributes 0 of 3 events to a class.** Each was settled correctly, but by a person's judgment against a file
map. The budget itself named no one.

## 4. Arm B: the same events with a share per class

The split below is a **plan shape, not a set of numbers.** The technique forbids inventing shares nobody measured, so
every device share is empty until V1, with its reason. Desk counts rank the classes. They do not size them.

| Row | Environment | Characters | Projectiles | Interface | Hands | Effects | Status |
|---|---|---|---|---|---|---|---|
| Thermal reserve (off the top, before any split) | - | - | - | - | - | - | **empty**: no reserve is written. "Recorded in every capture" (`:371`) is a measurement, not a reserve. V1 |
| GPU time | empty | empty | empty | empty | empty | empty | **no row exists.** The interval is the GPU's whole budget today. V1 |
| Render thread | empty | empty | empty | empty | empty | empty | **no row exists.** The CSV already reads `RenderThreadTime` (desk median 1.29 ms, ship1). V1 |
| Game thread ≤6 ms | empty | empty | empty | empty | empty | empty | split by **system** (kernel ≤1, recognizer ≤0.5), not by class |
| Draw calls ≤150 | rank 4 | rank 3 | **rank 1** | rank 2 | rank 5 | 11 (desk) | ranked from section 2. Set after lever B lands, from the post-B desk split |
| Triangles ≤350k | named | named | **unnamed** | **unnamed** | named | sprites | the row lists 3 classes. The 100 projectiles and 40 HP parts are counted but owned by no one |
| Particles ≤2,000 | - | - | - | - | - | **owned** | the one class share the table has |
| Textures ≤500 MB | empty | empty | empty | empty | empty | empty | shared; V1 |

Read the three events again:

- **E1:** projectiles hold rank 1 of the draw driver and interface rank 2. The owners of those two classes are the ones
  who cut. Lever B, one material per colour, is their lever, and it happens to live in one file for both.
- **E2:** lever A becomes a written trade. The environment's culling setting spends triangle headroom that the other
  classes hold, so the trade needs their owners' consent and a line in the table.
- **E3:** effects pay their 11 draws from an effects share on the draw row, or trade for it in writing. A class with its
  own count row still owes a share on every shared row it draws from.

**Arm B attributes 3 of 3 events to a class share.** Owners are not named here. Lanes change between phases, and naming
an owner is the App Master's act. The table gives each owner a row to hold.

## 5. Which headset binds each quantity

The table binds everything to "Quest 3 at 72 Hz". The registry technique, as it was written, said fill binds on the
headset with the larger per-eye display. That would be Quest 3 (2064x2208 against 1832x1920). This project's own research
already holds the better rule. Both headsets render the same default eye buffer, 1680x1760, on the same chip and RAM
(`FEASIBILITY-2026-10.md:78-80`, "Confirmed"). So the panel does not decide fill. The eye buffer does.

| Quantity | Binding headset | Why |
|---|---|---|
| Fill (GPU pixels) | **tie** at 100% render scale | Same default eye buffer on both (`FEASIBILITY-2026-10.md:80`). The tie breaks only if a pixel density is set per device (`:86-87`), and then the headset with the larger eye buffer binds. |
| Submission (CPU) | tie | Same chip (`:78`) |
| Memory | tie, per-app ceiling unverified | Same RAM (`:78`). The per-app ceiling of each is not read |
| Thermal | unverified | No 3S capture (`DECISIONS.md:342-355`) |

So "Quest 3 binds" is right, because nothing differs, not because Quest 3 is the harder device. The binding line
belongs beside each row, so that a per-device pixel density cannot silently move fill to one headset.

## 6. Verdict

- **Target:** events attributable to a class share, 0/3 → 3/3. Binding headset named per quantity, with its reason,
  0/4 → 4/4 (two of them ties, one unverified).
- **Floor:** every D-G4 verdict and figure unchanged; no config, code or plan edit. **Held.**
- **The falsifier fired on one clause.** The registry's "larger display binds fill" picks a headset that only ties here.
  The registry technique is corrected from this tree: the eye buffer binds fill, not the panel.

## 7. What this cannot show

- **No share has a size.** Sizing needs a device capture per class: GPU time with each class hidden in turn, at V1.
- **The +280 draws and +73k triangles are not split between characters, projectiles, interface and hands** on the desk.
  The per-part ranking is by component count, not by measured draws. A per-class split needs the D-G4 driver to spawn
  one class at a time. That is one switch per class in `Budget/`, and this run did not add it.
- **Thermal and the 3S per-app memory ceiling** are not read anywhere in this tree.

## 8. Return

V1 device sprint (from 2026-11-04):
1. Take a thermal reserve off the 13.9 ms first.
2. Capture GPU and render-thread time with one class hidden per run.
3. Write each class's share into section 7, with its unit, 72 Hz and Quest 3 as the basis.
4. Give the three ranked draw-row classes their post-lever-B counts.
