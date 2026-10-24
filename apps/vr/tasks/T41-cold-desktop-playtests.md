# T41 - Two cold desktop testers finish the session: protocol and dated record

Status: protocol written (`docs/research/PLAYTEST-PROTOCOL.md`, 2026-10-24); the sessions and the dated record remain open (the sessions run once a Wolf build of the full Tiro day exists, after T39 and T16/T38 land; the
build is made on the build machine, Wolf, the x64 UE host: UE 5.8.3. Written 2026-10-20 on the ARM64 laptop, where no UE
build runs). The testers are people the owner finds; a builder cannot be one.
Max turns: 80

## Goal
Two people who have never seen the game finish the whole session on the desktop build, and the result is a dated record in
the repo. It moves K3 row 12 ("2 cold testers finish", 0: no record, `docs/research/KPI-2026-10-20.md`) and gives the owner
evidence for row 15 (felt). "Cold" means no coaching: the teach, the HUD and `docs/DESKTOP-INPUT.md` keys on screen are all
the help there is. "Finish" means the aftermath screen is reached (won or lost final; retries allowed), with no restart by a
helper.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (they outrank this card if they conflict)
- `docs/PROJECT-PLAN.md` section 1 (gate row) and the playtest lines of D4 and V3
- `docs/DESKTOP-INPUT.md` (what the testers are shown), `apps/vr/tasks/T13-teach-and-pause.md` (the teach)
- `docs/research/` for the house style of dated records (for example `BUDGET-DG4-2026-10.md`)

## Deliverables (exact paths)
- `docs/research/PLAYTEST-PROTOCOL.md` - the protocol, written first (this part can be done on any host): who counts as cold
  (never played, never watched a recording, not part of the project; one may be non-gamer); the build to use (a commit hash
  and the stage path, recorded before the first session); the setup (mouse and keyboard, a stated resolution, the one-page
  key sheet and nothing else); the script a host reads, word for word, and the rule of silence (the host answers no question
  about how to play; a stuck tester is timed 3 min, then the stall is logged, not fixed); what is recorded per session:
  tester id (anonymous, no names), date, commit, start and end wall time, the bout reached, retries per bout, each stall
  over 30 s with the moment and the cause as said by the tester, the three-question debrief (what was unclear, what felt
  good, would you play again), and whether the aftermath was reached
- `docs/research/PLAYTEST-2026-10-DD.md` (DD = the day the second session ends) - the committed record: one section per
  tester, the table from the protocol, the verdict for K3 row 12 (pass only if both reached the aftermath unhelped), and
  every stall as a defect line for the stabilise loop. No names, no faces, no audio of a person (the contest forbids
  identifiable persons in the submission; keep it out of the repo as well). Written by the App Master or the owner from the
  host's notes; a builder does not invent a session
- A one-line pointer for each finding that is a bug, appended to `apps/vr/tasks/BACKLOG.md`

## Constraints
- Do not commit, push, or touch any repository other than this one.
- Do not call paid services. Do not edit `docs/DECISIONS.md`. No code changes in this card.
- Never claim a session that did not happen, a build that was not run, or a tester who was not cold. With fewer than two
  finished sessions the K3 row 12 reading stays 0 and the record says how many ran.
- If a session finds a blocker, file it; fixing it is another card.

## Acceptance (the orchestrator re-runs these; quote their real output in your report)
```
test -f docs/research/PLAYTEST-PROTOCOL.md && grep -c "cold" docs/research/PLAYTEST-PROTOCOL.md      # expected: the file exists, count above 0
ls docs/research/PLAYTEST-2026-10-*.md                                                               # expected: one dated record after the sessions
grep -n "K3 row 12" docs/research/PLAYTEST-2026-10-*.md                                              # expected: the verdict line, with the number of testers who finished
node tools/check-commits.mjs                                                                         # expected: exit 0
```

## Report
Write `runs/T41/REPORT.md`: the protocol path, the build commit used, the record path, each acceptance command with its real
output, how many testers ran and finished, and anything that could not be done and why.
