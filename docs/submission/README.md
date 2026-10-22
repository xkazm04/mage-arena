Status: draft, for the owner

# Submission package: index of the 10 items

The ten items are the K6 list in `docs/research/KPI-2026-10-20.md` (from `docs/PROJECT-PLAN.md` sections 2 and 11).
K6 counts an item only when a file here records it as final, or done with a date. **Today that count is 1 of 10**: one
item is closed as not needed (1), five have a draft the owner has not finalised (2, 3, 5, 6, 9), two wait on an owner
decision (4, 10), and two have not started (7, 8). Item 1 counts as done under the App Master's K6 reading: it is
withdrawn with a reason and a date, which is the plan's own measure for it (`docs/DECISIONS.md` 2026-10-21, "Deadlines
and Q9", supersedes the earlier "0 of 10").
This index was written 2026-10-20 on Lynx. The owner finalises.

Target: 10 of 10 by Tue 17 Nov; the text items (2-6, 9) by Sun 8 Nov (K6 target: 7 of 10, item 1 already done plus those six). The deadline is Wed 18 Nov 2026, 12:00
PT, which is 21:00 in Prague (`docs/PROJECT-PLAN.md` section 2).

| # | Item | File | Status | What is left |
|---|---|---|---|---|
| 1 | Q9 posted and answered | `ORGANIZER-QUESTION-Q9.md` | **closed, not needed.** Withdrawn, not posted (`docs/DECISIONS.md` 2026-10-21, "Deadlines and Q9") | Nothing. No organizer answer exists; the draft is kept as asked |
| 2 | Description (about 500 words) | `DESCRIPTION.md` | draft, for the owner | The launch date, the entrant, the assets line, and a rewrite of "Where it stands" on the submission day |
| 3 | Tagline (140 characters or fewer) | `DESCRIPTION.md`, "Tagline" | draft, 110 characters | The owner approves the wording |
| 4 | Target launch date (Q5) | `DESCRIPTION.md`, "Launch date" | **open, for the owner** | The owner decides. The plan's proposed default is spring 2027 (`docs/PROJECT-PLAN.md` section 11). Written as [confirm] |
| 5 | Hand-interaction write-up | `HAND-INTERACTION.md` | draft, for the owner | Real-hand and cross-user figures are pending; the mudra seal and the fist pose are designed, not built |
| 6 | Video beat sheet and script | `VIDEO-BEAT-SHEET.md` | draft, 2:50 total | Owner rewrite of the script; the seal beat depends on T24 |
| 7 | Video captured and public | none | **not started.** No footage on record | A style, the animated opponent and a device or simulator capture first: see "Footage that does not exist yet" in the beat sheet. Milestone 4. Key art and 6 concept stills (Leonardo, not gameplay) exist in `apps/vr/art/key-art/` (`index.html`, ledger group 6); gameplay captures are still owed from Wolf |
| 8 | APK on the "Competition" channel, with the invite URL | none | **not started.** No APK has been produced (`docs/XR-TOOLCHAIN.md:9`) | Android engine component, `package-android.ps1`, the dashboard app and channel: milestones 3 and 4 (K5) |
| 9 | Rules checklist | `RULES-CHECKLIST.md` | draft, for the owner. No row is met | Audits on the final build; owner answers on the rows marked "for the owner" |
| 10 | Sponsor-funding confirmation | `RULES-CHECKLIST.md`, row 7 | **open, for the owner** | The owner confirms in writing. Written as [confirm] |

## Where each missing piece comes from

- Items 2-6, 9 and 10 are text. Lynx can draft them; only the owner can finalise them.
- Items 7 and 8 depend on Unreal work on Wolf, the x64 host, by owner ruling (`docs/DECISIONS.md` 2026-10-20, "Unreal host").
  Milestone 2 (31 Oct) is the desktop core gate; milestone 3 (4 Nov) is the first APK booted under the Quest 3 profile;
  milestone 4 (17 Nov) is the submittable build and the package (`docs/research/KPI-2026-10-20.md`, "The four milestones").
- Nothing in this folder claims an Unreal build, an APK, a device run or a video. None is on record.

## Owner decisions this package waits on

1. Nothing on item 1: Q9 is closed as not needed (`docs/DECISIONS.md` 2026-10-21, "Deadlines and Q9").
2. Choose the launch date, Q5 (item 4).
3. Confirm the entrant's identity, that the sibling TV channel is not released or entered, and the sponsor-funding line
   (`RULES-CHECKLIST.md`, items 7 and the "Further rules" table; `ORGANIZER-QUESTION-Q9.md`, "Before you post").
4. Decide which pending figures the hand-interaction write-up needs before filing (`HAND-INTERACTION.md`).
5. Decide the seal-or-reflection finish for the video (`VIDEO-BEAT-SHEET.md`).
