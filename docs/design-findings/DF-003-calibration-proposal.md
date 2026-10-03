# DF-003 - Seated calibration proposal, not signed off

Found 2026-10-03 by T12 (`runs/T12/REPORT.md`; the official census is committed beside this file as `DF-003-census/sweep.csv` and `DF-003-census/summary.json` - `runs/` is not tracked). Orchestrator re-ran the census: `sweep.csv` reproduced byte-identical. Rewritten the same day after Retry 1. The first census (600 rows, wall 251.7 s) measured a fire wall that destroyed the caster's own bolts and paid her Heat for them. That bug lengthened every duel in which the mage raised a wall. The table below is the census after the fix.

The census wrote `apps/vr/data/vr/calibration-proposal.json`. That file is applied only with `-MageArenaProposal` (or `MageArenaProposal=1` / `true`). The live overlay in `combat.vr.json` still has the T11 hands and dome, plus the fire wall at the SCHOOL-DEFENCES C proposal (4 m, 90 degrees, 3 s, 12 mana, Heat 4, burn 12). Nothing in the census was copied into those live values.

The seated Fire duel is a different fight from DF-002. DF-002 is the walking reference player, and that player loses in 16.0 s and 18.4 s. From a seat, on the live overlay, the player wins 17 of 20 duels at competence 1 (median 30.47 s) and 12 of 20 at competence 1.5 (median 28.52 s). Sunfall is cast in none of those 40 duels.

## Measurement

Official census: 20 seeds, six knob sets, reference script, plus the policy sweep on `baseline` and on the winner. Wall time of that run was 193.5 s (`runs/T12/retry-census.log`, `Census done seeds=20 rows=720 wall=193.5s winner=wide official=1`, 2026.10.03-19.14.11). No set meets every target. The scorer still writes the lowest-penalty set. That set is `wide`.

Targets: Wave 1 victory inside 25–40 s on at least 80% of seeds, and the median HP left on wins in 20–60%. Fire duel median length 45–80 s and Sunfall in at least 70% of duels, at both competences. Player win rate at least 70% at competence 1 and 40–60% at competence 1.5.

HP below is the median fraction left on winning seeds. A row with no wins has no HP median. Wave 1 does not raise a fire wall, so the shared candidates keep the first census's wave window, HP median, and time.

| Set | Knobs | Penalty | Wave in 25–40 | Wave HP | Wave time | C1 time | C1 Sunfall | C1 win | C1.5 time | C1.5 Sunfall | C1.5 win |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| baseline (live) | 0 | 14.87 | 0/20 | — | 18.65 s | 30.47 s | 0/20 | 17/20 | 28.52 s | 0/20 | 12/20 |
| shade | 2 | 17.10 | 0/20 | — | 18.65 s | 38.66 s | 4/20 | 7/20 | 28.97 s | 0/20 | 0/20 |
| opendome | 5 | 13.29 | 9/20 | 8.4% | 25.63 s | 26.17 s | 0/20 | 20/20 | 26.65 s | 0/20 | 20/20 |
| wide (written) | 9 | 6.54 | 17/20 | 17.2% | 39.05 s | 44.83 s | 1/20 | 19/20 | 40.56 s | 3/20 | 15/20 |
| halfward | 10 | 6.63 | 17/20 | 16.4% | 38.82 s | 44.83 s | 1/20 | 19/20 | 40.56 s | 3/20 | 15/20 |
| quarterward | 10 | 6.67 | 20/20 | 15.8% | 39.30 s | 44.83 s | 1/20 | 19/20 | 40.56 s | 3/20 | 15/20 |

`shade` changes the fire wall only: 12 s and 6 mana. Distance, arc, Heat, and burn stay at the live proposal. Wave 1 is the same 20 losses as today. With her own bolts passing the curtain, the long wall now favours the mage. Competence 1 wins 7 of 20, median 38.66 s, Sunfall in 4 of 20. Competence 1.5 wins none, median 28.97 s, Sunfall in none. Penalty 17.10 is the worst of the six.

`opendome` leaves the live 3 s wall alone and opens the player's dome: mana regen ×1.25, one-hand power 1.0, dome 14 s at 1.5 mana/s, blocking 85% of a physical hit (a 10-damage spear becomes 1.5). Wave 1 is won inside the window on 9 of 20 seeds, and those wins finish on about 8% HP. Both duels end near 26 s. The player wins all 40, and Sunfall is never cast.

`wide` is opendome with a thicker dome and shade's wall, plus a wider arc: physical block 0.90 (a spear becomes 1), magic block 0.92, wall 12 s / 6 mana / 150 degrees. Ward drain while split-casting stays at the live 27 mana/s. Wave 1 is won inside 25–40 s on 17 of 20 seeds. The median win leaves 17.2% HP, under the 20% floor. Three seeds die around 26 s having dealt 152 of 208. Competence 1 lands at 44.83 s, 0.17 s under the 45 s floor, with a 95% player win and Sunfall in 1 of 20. Competence 1.5 lands at 40.56 s, Sunfall in 3 of 20, and the player wins 15 of 20 (75%, above the 40–60% band).

`halfward` is `wide` with the palm ward draining half of 27 mana/s (13.5/s) while the other hand is casting. `quarterward` drains a quarter (6.75/s) in that same window. The reference duel almost never split-casts (mean 0.20 at competence 1, 0 at competence 1.5), so those two knobs do not move the duel medians, the Sunfall counts, or the win counts. The one duel row that changes is competence 1, seed 7: 43.07 s under `wide`, 42.67 s at half drain, 41.67 s at quarter drain.

On Wave 1 the reference script splits once, so the drain does move the wave. Half drain turns `wide`'s three deaths (seeds 12, 17, 20) into in-window wins at 7.6, 13.0, and 5.7 HP, and it pushes seeds 7 and 10 past 40 s (41.82 s and 41.10 s) and turns seed 19 into a loss at 38.72 s. The window stays 17 of 20. The new wins are thin, so the HP median falls to 16.4%. Quarter drain converts the same three deaths (6.2, 13.0, and 5.7 HP) and leaves every other seed inside the window, so the window is 20 of 20 and the HP median falls to 15.8%. Both carry an extra knob. Both score worse than `wide`.

## What each one costs in feel

`shade` is a longer, cheaper curtain. The mage holds it up for 12 s instead of 3 s and pays 6 mana instead of 12. The arc stays 90 degrees. She now shoots through that curtain, and at competence 1.5 the player does not survive it. Soldiers, the dome, the hands, and bolt damage stay as they are today.

`opendome` makes the planted staff the fight. Fourteen seconds is a long blink lock. While it is up, a spear is 1.5 and the drain is 1.5 mana/s instead of 4. The one split cast hits at full bolt damage instead of 0.6. Regen is a quarter higher, which is what pays a second plant. The Fire mage is still on the live 3 s / 12 mana wall, so the player who lives through the dome kills the mage before tier IV.

`wide` stacks those costs and adds two more. The dome blocks 90% of a spear and 92% of a fire bolt. The curtain covers 150 degrees and lasts 12 s for 6 mana, and the mage's own bolts cross it. Nine overlay fields move, all of them player-side. Enemy count, spawn timing, cadence, and damage stay pinned. Bolt and line damage stay pinned.

`halfward` and `quarterward` add the split-cast ward. The live drain is 27 mana/s at Nerve 1 whenever the other hand is casting. Half is 13.5/s and quarter is 6.75/s, and only in that window. The cheaper ward leaves mana for another plant (mean plants 1.65 and 1.70, against 1.45 on `wide`). The clears it buys are the thin ones above.

## Script policy

The same 20 seeds were repeated for `baseline` and `wide` with three other scripts: eager to split, never split, never plant. A balance that only works for one script is a finding. Wave HP is the median fraction on wins.

| Candidate | Policy | Wave in 25–40 | Wave HP | Wave time | Plants | Splits | C1 time | C1 Sunfall | C1 win | C1.5 time | C1.5 Sunfall | C1.5 win |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| baseline | reference | 0/20 | — | 18.65 s | 1.00 | 1.00 | 30.47 s | 0/20 | 17/20 | 28.52 s | 0/20 | 12/20 |
| baseline | eager-split | 0/20 | — | 18.65 s | 1.00 | 1.25 | 30.47 s | 0/20 | 17/20 | 28.52 s | 0/20 | 12/20 |
| baseline | no-split | 0/20 | — | 13.60 s | 2.00 | 0 | 30.47 s | 0/20 | 18/20 | 28.42 s | 0/20 | 13/20 |
| baseline | no-plant | 0/20 | 38.5% (6 wins, none in window) | 11.11 s | 0 | 3.20 | 26.85 s | 0/20 | 11/20 | 21.49 s | 0/20 | 3/20 |
| wide | reference | 17/20 | 17.2% | 39.05 s | 1.45 | 1.00 | 44.83 s | 1/20 | 19/20 | 40.56 s | 3/20 | 15/20 |
| wide | eager-split | 8/20 | 14.7% | 39.68 s | 1.35 | 2.15 | 44.83 s | 1/20 | 19/20 | 40.56 s | 3/20 | 15/20 |
| wide | no-split | 20/20 | 41.1% | 35.58 s | 2.10 | 0 | 44.83 s | 1/20 | 19/20 | 40.56 s | 3/20 | 16/20 |
| wide | no-plant | 2/20 | 54.3% | 11.47 s | 0 | 4.50 | 27.31 s | 0/20 | 3/20 | 16.02 s | 0/20 | 0/20 |

On the live overlay, eager-split still loses all 20 Wave 1 seeds, at the same 18.65 s median. Five wave rows differ from the reference script, and the duel win counts stay 17 and 12. Never splitting dies faster (median 13.60 s) even though it plants twice: the live dome is 6 s at 60% block. Never planting loses the wave (six wins, all outside 25–40 s, median of all rows 11.11 s) and shortens both duels. The plant is load-bearing.

On `wide`, never splitting wins Wave 1 on all 20 seeds, median 35.58 s, median HP left 41.1%, which is inside 20–60%. That script plants about twice (mean 2.10) because it never spends the pool on the ward. The duel stays on the `wide` medians, with one extra competence-1.5 win (16 of 20). Eager-split holds the ward longer and the in-window rate falls to 8 of 20, HP 14.7%. Never planting drops the wave to 2 of 20 inside the window (median of all rows 11.47 s) and wrecks the duel: competence 1 is 27.31 s with 3 wins and no Sunfall, competence 1.5 is 16.02 s with no wins. Once the mage's bolts pass her own wall, the dome is what stops them. A balance that needs the player never to split still clears the wave only. A balance that needs the player never to plant clears neither.

## Recommendation

Leave `wide` in the proposal file, and do not sign it off. It is the lowest penalty the census measured, and it gets there without making the soldiers or the mage hit softer. It passes the Wave 1 window (17 of 20) and the competence-1 win rate (19 of 20). It misses the HP floor (17.2% against 20%), the competence-1 time by 0.17 s, the competence-1.5 time (40.56 s against 45 s), the Sunfall rate (1 of 20 and 3 of 20 against 70%), and it wins the competence-1.5 duel too often (75%, against a 40–60% band).

`shade` is no longer the duel candidate. Two defence knobs, and the longer curtain now kills the player, especially at competence 1.5.

`opendome` shows the dome lever on its own. It buys some Wave 1 clears at the cost of the whole duel.

`halfward` and `quarterward` are the follow-up the first write-up named and did not measure. Quarter drain does put every Wave 1 seed inside 25–40 s. The three new wins leave 6.2, 13.0, and 5.7 HP, and the median of all wins falls to 15.8%. The duel does not move, because this script barely split-casts in a duel. Neither drain beats `wide` on the scorer.

The two gates still pull apart. A curtain the mage can afford, once her own bolts pass it, is a weapon aimed at the player. A dome the player can live in ends the duel before tier IV, because a seated Water mage who is still alive bolt-spams a 95 HP Ember. The few Sunfalls in this census are the long fights: shade's competence-1 casts land at 56–67 s, and `wide`'s competence-1.5 casts are three losses at 56.02 s. Seeing Sunfall is a long fight. It is not a player win. The census did not find one overlay that lands both gates.
