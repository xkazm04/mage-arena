# Backlog (items found between cards; the orchestrator folds them into the next fitting card)

- [x] `UHandClipPlayer::Step` ignores `bPlaying`: after a clip ends, further `Step` calls re-emit the last frame with
  new sequence numbers, which a gesture detector could read as a held pose (`Hands/HandClipPlayer.cpp`; found by
  an `agy` review on 2026-10-02). Fixed in T04. Regression test: `MageArena.Clips.NoEmitAfterEnd`.
