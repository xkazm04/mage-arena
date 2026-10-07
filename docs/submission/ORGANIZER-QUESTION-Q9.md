# Question to the organizers (Q9): reuse of our own design, kernel and data

- **Where to post it:** the competition's Discussions page, `https://start-developer-competition-26.devpost.com/forum_topics`
  (linked from both the rules page and the overview page; whether it accepts new topics from entrants is [confirm]).
  Fallback: the Devpost contact page, `https://info.devpost.com/contact`. The pages show no mailto and no manager email.
- **When it is due:** before 12 Oct 2026.
- **Status:** draft, not sent. The owner posts it. (DECISIONS.md, 2026-10-07: the reuse is provisionally accepted meanwhile.)

## Before you post

Facts only the owner can confirm. The body below does not assert any of them.

- [confirm] The sibling TV channel has not been released, and has not been shown publicly as a shipped title.
- [confirm] The same individual is the entrant for both the TV and the VR project.
- [confirm] The TV git dates are real. 5 of 116 TV commits have an author date that differs from the committer date by seconds to minutes (history rewrite, 2026-10-07); the three commits cited below have equal dates.
- [confirm] Whether the second question (tooling) stays in or is cut.

## Subject

Eligibility question, New Experience division: reusing our own post-window design, rules kernel and data

## Body

Hello,

The New Experience rule says: "conceived of and built within the competition window (starting September 24, 2026). No pre-existing codebases, no shipped titles, no early access builds repurposed".

Our VR entry shares a game design, a rules kernel and combat data with a sibling project of the same game. The window opened on 24 Sep 2026. The shared design, kernel and data were first committed on 1 Oct 2026 at 23:46 CEST. The VR project was first committed on 2 Oct 2026.

1. Does reusing our own design, kernel and data, created after the window opened in a sibling project of the same game, keep the VR entry eligible for the New Experience division? (yes/no)
2. May we use our own development tooling from before the window? It runs only in the editor and is not shipped in the APK. (yes/no)

Thank you.

## Evidence (fetched 2026-10-07)

Page fetches, Chrome desktop User-Agent:

```
curl -sS -A '<Chrome 141 desktop UA>' -o runs/rules/rules.html -w '%{http_code} %{size_download}' https://start-developer-competition-26.devpost.com/rules
  -> exit 0, 200 183554
curl -sS -A '<Chrome 141 desktop UA>' -o runs/rules/overview.html -w '%{http_code} %{size_download}' https://start-developer-competition-26.devpost.com/
  -> exit 0, 200 166849
grep -F 'conceived of and built within the competition window (starting September 24, 2026). No pre-existing codebases, no shipped titles, no early access builds repurposed' runs/rules/rules.html
  -> exit 0
```

Git dates (read-only; `TV` is `C:/Users/kazda/kiro/mage-arena-tv`):

```
git -C TV log --reverse --format='%h %ad %cd %s' --date=iso | head -5
  1e9de1c 2026-10-01 23:46:39 +0200 2026-10-01 23:46:39 +0200 docs: Mage Arena plan, design baseline and objective review
  5b19173 2026-10-01 23:58:08 +0200 ...  W0: reconcile season design and generate traceable golden nights
  13256c6 2026-10-02 00:08:42 +0200 ...  W2: build deterministic arena kernel and mouse keyboard proving ground
git -C TV log -1 --format='%h %ad %cd %s' --date=iso baeac66
  baeac66 2026-10-02 13:48:12 +0200 2026-10-02 13:48:12 +0200 W4b: implement confirmed oblique camera and ground-plane aiming
git -C TV log --format='%ad' --date=short | sort | head -1
  2026-10-01
git log --reverse --format='%h %ad %s' --date=iso | head -1     (this repo)
  5bc06ee 2026-10-02 17:09:32 +0200 chore: scaffold Mage Arena VR - UE 5.8 C++ project, decisions, plan, desktop input
git -C TV grep -nE '20(1[0-9]|2[0-5])-[01][0-9]-[0-3][0-9]|2026-0[1-8]-[0-3][0-9]|2026-09-(0[1-9]|1[0-9]|2[0-3])' 1e9de1c   -> no hit (exit 1)
the same at baeac66                                                                                                             -> no hit (exit 1)
```

`baeac66` is the pinned combat data commit (it was `68a4d68` before the 2026-10-07 TV history rewrite; see `docs/REPO-LAYOUT.md`).
