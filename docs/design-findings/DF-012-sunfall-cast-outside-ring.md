# DF-012 - Brennic casts Sunfall from outside the ring wall, behind the seat

Found 2026-10-24 on Wolf by T49 (the threat-colour card), while it tried to frame the first opponent unblockable of the
seated Fire duel. Written up read-only on the T49 fill-fix branch. Status: **owner-to-rule; nothing decided.** No code
or data changes for this finding.

## What was seen

In the seated Fire duel the only opponent unblockable is Brennic's Sunfall. When he casts it he stands at
layout X **-17.07 m**: outside the ring wall (`arena-layout.json` `floor.radiusXM 16`, so the wall at Y 0 is X -16) and
inside the stands geometry, about 5.1 m behind the centre pad (`pads` centre `positionM` X -12). The capture's own log
line (T49 card, "What I could not do" item 2; raw log in the lost `runs/T49/`):

```
MAGEVR_T49_AIM kind=area eye=X=-1162.500 ... bar=X=-1707.148
```

`eye` is the seated camera, `bar` is Brennic's HP bar, which `SessionPresentation.cpp` places straight above the body.
Both are Unreal cm. No seated view can show him, so T49 dropped that capture and used the Air still instead.

## Where the cast position comes from

1. **The body moves in the kernel's field, not the VR ring.** `MageAI.cpp:929-948`: past the decision cadence the mage
   closes in beyond `magePreferredDistanceM` 8, backs off under 5, and otherwise **strafes perpendicular to the seat**,
   flipping every `mageStrafePeriodS` 2.4 s (pinned `runtime.json:42`). The only containment is
   `ArenaContains(Self.Pos, spawnMarginM 2)` (`MageAI.cpp:947`), and every step goes through `ConstrainToArena`
   (`ArenaKernel.cpp:136`, Fire's dash at `Fire.cpp:327`). Both test the kernel's ellipse: centre (16, 62), axes from
   the scale contract's `arena_metres` 192 x 144 (`KernelData.cpp:529`, `GeometryTests.cpp:45-46`). On that field a
   body 5 m behind the seat is deep inside.
2. **The VR overlay keeps bodies off the dais, not inside the wall.** `combat.vr.json` `daisNoEntry` (hold box: dais
   footprint plus 0.6 m, layout X -14.9 to -8.5) is the only VR geometry rule on body positions (`VrRules.cpp:62-160`,
   `Enemies.cpp:360`, Air's `KeepOpponentPoint`). Nothing maps the 16 x 10 m ring wall into the kernel.
3. **The presentation is a pure translation.** `FArenaSession::KernelToUnrealCm` (`ArenaSession.cpp:503`) maps kernel X
   to layout X as `-12 + (Pos.X - playerSpawn.X)`, with no clamp. Kernel X 2.93 (playerSpawn X 8 minus 5.07) draws at -17.07.
4. **Sunfall resolves from wherever he stands.** `Fire.cpp:300-322`: `Telegraph.Source = Telegraph.Origin = Actor.Pos`.
   Only narrow view (`Rules->bNarrow`) presses the origin into the arc, and it presses the telegraph, not the body.

So a mage that strafes round the seat at 5-8 m sweeps behind the dais, and the ring wall at X -16 is only 4 m behind
the seat. Lio (Air) and every Fire mage use the same brain, so the finding is not Brennic's alone. It is just where T49
looked. A census of how often an opponent stands outside the ring, or behind the seat, has not been run. It needs one
`MageArenaDesign` pass with a position line, and that is a card, not this draft.

## Options

- **A. Accept as design.** The mage circles the seat and the seat turns to find him: the strafe is the TV brain the pin
  froze. Costs nothing in code and keeps every DF-011 number as measured. Costs readability: the signature
  unblockable can come from a body drawn inside the stands, 180 degrees from the seated forward, which the threat
  language (read the colour, then blink) assumes you can see. No still can frame it.
- **B. Clamp opponent bodies inside the ring.** On the VR-ruleset path only (the conformance vectors run without a
  ruleset and stay on the pinned path), constrain an opponent's position to the `arena-layout.json` ellipse minus the
  wall thickness and body radius. This would be a new overlay field beside `daisNoEntry`, read by the same `KeepOpponentPoint` / hold
  code. Cost: one kernel seam plus an overlay field, the `check-overlay` schema and a test, and a re-measure of the
  three DF-011 reds and the rival census, because a mage that meets the wall stops strafing there. It still leaves a
  1.1 m sliver behind the dais (X -16 to -14.9) where he can stand behind the seat.
- **C. Keep opponent bodies in the forward threat arc.** Press opponent body positions (not only telegraphs) into the
  forward arc the layout already promises (±70 deg, `docs/gameplay/01-body-and-space.md:149`). It reuses the narrow
  view's `CompressToArc` with the default arc, and it implies B's ring clamp. Cost: the same seam, test and re-measure
  as B, plus a larger behaviour change. The strafe turns into an arc walk in front of the seat, so the Fire and Air
  duel lengths will move, and the direction is unknown.
- **D. Move the spawn or the waypoints.** Spawn further out (`games.enemySpawn` is pinned; an overlay spawn) or
  give mages waypoints. It does not fix the finding by itself: the strafe still orbits the seat once the mage is in
  its 5-8 m band. Listed for completeness, not recommended.

## Recommendation

**C, with B as the floor if C moves the duels too far.** The finding is "behind the seat", not only "outside the wall".
B alone still lets Brennic stand in the 1.1 m behind the dais. C matches the seated, forward-facing threat arc the
layout and the spawn markers are already built around. Either one is a kernel-seam card with a re-measure, so it
should land before DF-011 is re-ruled. Otherwise DF-011's numbers are measured on a mage the seat cannot see.
