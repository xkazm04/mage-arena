# Music loops - source (T40 owner input)

- Service: ElevenLabs Music. Endpoint: `POST https://api.elevenlabs.io/v1/music?output_format=mp3_44100_128`.
- Model: `music_v1` (sent as `model_id`), `force_instrumental: true`, `music_length_ms: 60000`. No seed (the API takes none with a prompt); the
  ElevenLabs `song-id` of each take is in `elevenlabs-log.json`.
- Date: 2026-10-21. Generation count: 2 calls (one per loop), 0 regenerations. Both returned HTTP 200.
- Raw outputs: `src/arena.mp3`, `src/intermission.mp3` (128 kbps MP3, 44.1 kHz, about 60.1 s, LFS).
- Shipped loops: `arena-loop.wav`, `intermission-loop.wav` - 48 kHz, 16-bit, stereo, peak -1 dBFS, leading and trailing silence below
  -50 dBFS trimmed, 1.5 s loop crossfade by the method of `apps/vr/tools/sfx/generate.mjs` (see `manifest.json`, `loop_crossfade`).
  Rebuild without any API call: `node apps/vr/tools/music/generate.mjs --convert-only` (needs ffmpeg).
- Call log: `elevenlabs-log.json` (endpoint, model, prompt, format, bytes, duration, timestamp, regen reason; never the key).

## Prompts

Common line (both loops): "Instrumental only, no vocals, no choir words. Roman-arena fantasy spell duel, ancient stone amphitheatre,
cinematic, a seamless loop with a steady pulse and no intro or final cadence. Mixed to sit under sound effects."

- arena: "Bout music under combat: slow, steady low war drums and frame drums, cellos and low brass playing a repeating dark minor
  ostinato, sparse and restrained tension. No lead melody, nothing busy or bright in the high-mid range, no shrill strings or cymbal
  washes, so alert sound cues stay clear on top."
- intermission: "The calm bed between bouts and after a duel: warm sustained strings and soft low drone, a gentle lyre or harp figure far
  in the background, slow and reflective, breathing room after the fight. No drums, no lead melody, quiet and even."

PROJECT-PLAN P13 gives no prompt wording; these are the generator's own, written from the card and the arena brief.

## Licence

Generated output from the owner's ElevenLabs account; use under the ElevenLabs terms of service for the plan the key belongs to. The
builder did not read the terms page or the plan tier from the machine: the owner confirms that the plan allows commercial use of
generated music in a shipped game before submission (originality rule, plan section 12 Q8).

## Not heard

No person has heard this music. Peak, length, edge silence and seam size are measured; whether it sounds right, sits under the threat
cues and loops cleanly by ear is the owner's call (K3 row 15, felt).
