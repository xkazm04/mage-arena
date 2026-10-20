---
id: 0005
kind: gotcha
scope: path:apps/vr/data
date: 2026-10-05
supersedes: null
refs: [0002]
---

A pinned enemy's `teaches:` claim, or any number authored for the walking game, is not true from the seat until it is
measured from the seat. DF-004: the cinder hound "teaches perfect absorb on death bursts", but its burst is 1.5 m and,
with every body held off the dais (0002), the nearest a hound can die is about 3.5 m from the player, so Bout 2 was won
with 0 perfect absorbs. DF-002: the Fire school was authored, never calibrated in any channel, and the Ember mage ended
every duel before Sunfall (tier IV, 45 s) could appear. Before building a lesson or a duel on pinned data, run the seated
scenario and check the claimed behaviour actually occurs; a unit test of the rule (a burst that does reach the player
scores a perfect) does not prove the bout produces it.
