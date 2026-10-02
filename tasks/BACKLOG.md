# Backlog (items found between cards; the orchestrator folds them into the next fitting card)

- [ ] `UHandClipPlayer::Step` ignores `bPlaying`: after a clip ends, further `Step` calls re-emit the last frame with
  new sequence numbers, which a gesture detector could read as a held pose (`Hands/HandClipPlayer.cpp:36-48`; found by
  an `agy` review on 2026-10-02). Fold into the ward/gesture detector card with a test.
