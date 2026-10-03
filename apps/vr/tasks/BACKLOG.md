# Backlog (items found between cards; the orchestrator folds them into the next fitting card)

- [x] `UHandClipPlayer::Step` ignores `bPlaying`: after a clip ends, further `Step` calls re-emit the last frame with
  new sequence numbers, which a gesture detector could read as a held pose (`Hands/HandClipPlayer.cpp`; found by
  an `agy` review on 2026-10-02). Fixed in T04. Regression test: `MageArena.Clips.NoEmitAfterEnd`.
- [ ] Mudra glyph (`apps/vr/art/glyphs/sigil-mudra*`) is an arch traced from the synthetic mudra clip - it needs a deliberate seal design (creative pass for the orchestrator + owner, not a worker). Found in A03 review 2026-10-03.
- [ ] Chalk & Slate glyphs on a light ground are low-contrast by nature; if 30 wins the style pick, define the light-UI variant separately.
- [ ] Fire mage hair is inconsistent between model sheets (01 key poses: short hair; 02 cast sequence: bald). Settle the head when modelling starts.
- [ ] SFX loudness is only peak-normalised (all cues -1 dBFS peak); balance relative loudness (e.g. ui-pause, flow-stack quieter than impacts) when audio is wired in Unreal. Crowd loop join may click softly (A05 report). From A05 review 2026-10-03.
- [ ] Environment kit (A07) wall pieces carry medieval crenellations - not Roman amphitheatre; drop them when modelling. The "low stone seat" came out as a bench with a backrest; the player seat should be a backless low block. From A07 review 2026-10-03.
