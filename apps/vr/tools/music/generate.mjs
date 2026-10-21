// Two music loops (arena, intermission) via ElevenLabs. Node built-ins only, plus ffmpeg for the WAV conversion.
// One call per loop; at most one regeneration per loop, for a stated defect (--regen <id> --reason "<text>").
// The API key is read from the environment or bp's .env and is never printed, logged, written or put on a command line.

import crypto from 'node:crypto';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const APP = path.resolve(HERE, '../..');
const REPO = path.resolve(APP, '../..');
const OUT_DIR = path.join(APP, 'art', 'audio', 'music');
const SRC_DIR = path.join(OUT_DIR, 'src');
const LOG_PATH = path.join(OUT_DIR, 'elevenlabs-log.json');
const MANIFEST_PATH = path.join(OUT_DIR, 'manifest.json');
const ENV_PATH = 'C:\\Users\\kazda\\kiro\\bp\\.env';
const KEY_NAME = 'ELEVEN_LABS_API_KEY';
const FFMPEG_FALLBACK = 'C:\\Users\\kazda\\ai\\tools\\ffmpeg-master-latest-winarm64-gpl\\bin\\ffmpeg.exe';
const ENDPOINT = 'https://api.elevenlabs.io/v1/music';
const MODEL_ID = 'music_v1';
const OUTPUT_FORMAT = 'mp3_44100_128';
const SAMPLE_RATE = 48000;
const CHANNELS = 2;
const LENGTH_MS = 60000;
const CROSSFADE_S = 1.5;
const SILENCE_LINEAR = 0.00316227766; // -50 dBFS
const PEAK_TARGET = Math.round(32768 * 10 ** (-1 / 20)); // -1 dBFS

const COMMON =
  'Instrumental only, no vocals, no choir words. Roman-arena fantasy spell duel, ancient stone amphitheatre, ' +
  'cinematic, a seamless loop with a steady pulse and no intro or final cadence. Mixed to sit under sound effects.';
const LOOPS = [
  {
    id: 'arena',
    text:
      'Bout music under combat: slow, steady low war drums and frame drums, cellos and low brass playing a repeating dark ' +
      'minor ostinato, sparse and restrained tension. No lead melody, nothing busy or bright in the high-mid range, ' +
      'no shrill strings or cymbal washes, so alert sound cues stay clear on top.',
  },
  {
    id: 'intermission',
    text:
      'The calm bed between bouts and after a duel: warm sustained strings and soft low drone, a gentle lyre or harp ' +
      'figure far in the background, slow and reflective, breathing room after the fight. No drums, no lead melody, quiet and even.',
  },
];

let apiKey = '';
const log = (m) => fs.writeSync(1, `${m}\n`);
const rel = (f) => path.relative(REPO, f).split(path.sep).join('/');
const round3 = (n) => Math.round(n * 1000) / 1000;

function sanitize(text) {
  let out = String(text ?? '');
  if (apiKey) out = out.split(apiKey).join('[redacted]');
  out = out.replace(/\bsk_[A-Za-z0-9_-]{8,}\b/g, '[redacted]');
  return out.replace(/\s+/g, ' ').slice(0, 300);
}

function loadKey() {
  const fromEnv = process.env[KEY_NAME];
  if (fromEnv && fromEnv.trim()) return fromEnv.trim();
  let text;
  try {
    text = fs.readFileSync(ENV_PATH, 'utf8');
  } catch {
    throw new Error(`${KEY_NAME} is not set and the bp .env could not be read`);
  }
  if (text.charCodeAt(0) === 0xfeff) text = text.slice(1);
  for (const raw of text.split(/\r?\n/)) {
    const line = raw.trim();
    if (!line || line.startsWith('#')) continue;
    const eq = line.indexOf('=');
    if (eq < 0) continue;
    let name = line.slice(0, eq).trim();
    if (name.startsWith('export ')) name = name.slice(7).trim();
    if (name !== KEY_NAME) continue;
    let value = line.slice(eq + 1).trim();
    if ((value.startsWith('"') && value.endsWith('"')) || (value.startsWith("'") && value.endsWith("'"))) {
      value = value.slice(1, -1);
    }
    if (value) return value;
    break;
  }
  throw new Error(`${KEY_NAME} is not set`);
}

function run(bin, args) {
  return new Promise((resolve, reject) => {
    const child = spawn(bin, args, { windowsHide: true });
    const err = [];
    child.stderr.on('data', (c) => err.push(c));
    child.stdout.on('data', () => {});
    child.on('error', reject);
    child.on('close', (code) => {
      if (code === 0) resolve();
      else reject(new Error(sanitize(`${bin} exited ${code}: ${Buffer.concat(err).toString('utf8').slice(-600)}`)));
    });
  });
}

let ffmpegBin = null;
async function ffmpeg(args) {
  const full = ['-hide_banner', '-loglevel', 'error', '-y', ...args];
  if (!ffmpegBin) {
    try {
      await run('ffmpeg', ['-version']);
      ffmpegBin = 'ffmpeg';
    } catch {
      ffmpegBin = FFMPEG_FALLBACK;
    }
  }
  return run(ffmpegBin, full);
}

function parseWav(buf) {
  if (buf.length < 44 || buf.toString('ascii', 0, 4) !== 'RIFF' || buf.toString('ascii', 8, 12) !== 'WAVE') {
    throw new Error('not a wav');
  }
  let offset = 12;
  let fmt = null;
  let data = null;
  while (offset + 8 <= buf.length) {
    const id = buf.toString('ascii', offset, offset + 4);
    const size = buf.readUInt32LE(offset + 4);
    const start = offset + 8;
    if (id === 'fmt ') {
      fmt = { audioFormat: buf.readUInt16LE(start), channels: buf.readUInt16LE(start + 2), sampleRate: buf.readUInt32LE(start + 4), bits: buf.readUInt16LE(start + 14) };
    } else if (id === 'data') {
      data = Buffer.from(buf.subarray(start, start + Math.min(size, buf.length - start)));
    }
    offset = start + size + (size % 2);
  }
  if (!fmt || !data || fmt.audioFormat !== 1 || fmt.bits !== 16) throw new Error('expected pcm 16-bit wav');
  return { ...fmt, data, frames: data.length / (fmt.channels * 2) };
}

function writeWav(file, pcm, sampleRate, channels) {
  const h = Buffer.alloc(44);
  h.write('RIFF', 0);
  h.writeUInt32LE(36 + pcm.length, 4);
  h.write('WAVE', 8);
  h.write('fmt ', 12);
  h.writeUInt32LE(16, 16);
  h.writeUInt16LE(1, 20);
  h.writeUInt16LE(channels, 22);
  h.writeUInt32LE(sampleRate, 24);
  h.writeUInt32LE(sampleRate * channels * 2, 28);
  h.writeUInt16LE(channels * 2, 32);
  h.writeUInt16LE(16, 34);
  h.write('data', 36);
  h.writeUInt32LE(pcm.length, 40);
  fs.writeFileSync(file, Buffer.concat([h, pcm]));
}

function measureWav(file) {
  const p = parseWav(fs.readFileSync(file));
  let peak = 0;
  for (let i = 0; i < p.data.length; i += 2) peak = Math.max(peak, Math.abs(p.data.readInt16LE(i)));
  return { ...p, peak, peakDbfs: peak ? 20 * Math.log10(peak / 32768) : -Infinity, duration: p.frames / p.sampleRate };
}

// Peak level of the first and last 100 ms in dBFS: the "no leading or trailing silence below -50 dBFS" check.
function edgeDbfs(info) {
  const n = Math.min(info.frames, Math.round(info.sampleRate * 0.1)) * info.channels;
  const peakOf = (from) => {
    let m = 0;
    for (let i = 0; i < n; i++) m = Math.max(m, Math.abs(info.data.readInt16LE((from + i) * 2)));
    return m ? 20 * Math.log10(m / 32768) : -Infinity;
  };
  return { head: peakOf(0), tail: peakOf(info.data.length / 2 - n) };
}

function frameDiff(pcm, ch, a, b) {
  let max = 0;
  for (let c = 0; c < ch; c++) max = Math.max(max, Math.abs(pcm.readInt16LE((a * ch + c) * 2) - pcm.readInt16LE((b * ch + c) * 2)));
  return max;
}

function typicalStep(pcm, ch) {
  const frames = pcm.length / 2 / ch;
  const start = Math.floor(frames * 0.4);
  const steps = [];
  for (let i = start; i < start + 400 && i + 1 < frames; i++) steps.push(frameDiff(pcm, ch, i, i + 1));
  steps.sort((a, b) => a - b);
  return Math.max(1, steps[Math.floor(steps.length / 2)] || 1);
}

const silenceFilter = () =>
  `silenceremove=start_periods=1:start_duration=0:start_threshold=${SILENCE_LINEAR}:detection=peak:window=0.005`;

// Same method as tools/sfx/generate.mjs: trim silence, fade the last X s out and the first X s in (quarter sine),
// mix at unity into the head, append the untouched middle.
function loopGraph(frameCount, x) {
  const midEnd = frameCount - x;
  const fade = (x / SAMPLE_RATE).toFixed(6);
  const fmt = `aformat=sample_rates=${SAMPLE_RATE}:channel_layouts=stereo`;
  return [
    '[0:a]asplit=3[s0][s1][s2]',
    `[s0]atrim=start_sample=0:end_sample=${x},asetpts=PTS-STARTPTS,afade=t=in:st=0:d=${fade}:curve=qsin[head]`,
    `[s1]atrim=start_sample=${midEnd}:end_sample=${frameCount},asetpts=PTS-STARTPTS,afade=t=out:st=0:d=${fade}:curve=qsin[tail]`,
    '[tail][head]amix=inputs=2:duration=longest:dropout_transition=0:normalize=0[xf]',
    `[s2]atrim=start_sample=${x}:end_sample=${midEnd},asetpts=PTS-STARTPTS[mid]`,
    `[xf]${fmt}[xf2]`,
    `[mid]${fmt}[mid2]`,
    '[xf2][mid2]concat=n=2:v=0:a=1[out]',
  ].join(';');
}

async function convert(input, output) {
  const work = fs.mkdtempSync(path.join(os.tmpdir(), 'music-wav-'));
  try {
    const lead = silenceFilter();
    const trimmed = path.join(work, 'trimmed.wav');
    const looped = path.join(work, 'looped.wav');
    const enc = ['-ar', String(SAMPLE_RATE), '-ac', String(CHANNELS), '-c:a', 'pcm_s16le'];
    await ffmpeg(['-i', input, '-af', `aresample=${SAMPLE_RATE},${lead},areverse,${lead},areverse`, ...enc, trimmed]);
    const t = measureWav(trimmed);
    const x = Math.min(Math.round(CROSSFADE_S * SAMPLE_RATE), Math.floor(t.frames / 4));
    await ffmpeg(['-i', trimmed, '-filter_complex', loopGraph(t.frames, x), '-map', '[out]', ...enc, looped]);
    const p = parseWav(fs.readFileSync(looped));
    let peak = 0;
    for (let i = 0; i < p.data.length; i += 2) peak = Math.max(peak, Math.abs(p.data.readInt16LE(i)));
    if (!peak) throw new Error('audio is silent');
    const gain = PEAK_TARGET / peak;
    const pcm = Buffer.from(p.data);
    for (let i = 0; i < pcm.length; i += 2) {
      pcm.writeInt16LE(Math.max(-32768, Math.min(32767, Math.round(pcm.readInt16LE(i) * gain))), i);
    }
    writeWav(output, pcm, p.sampleRate, p.channels);
    const m = measureWav(output);
    const edge = edgeDbfs(m);
    return {
      sample_rate: m.sampleRate, channels: m.channels, bit_depth: m.bits, frames: m.frames,
      duration_s: round3(m.duration), peak_dbfs: round3(m.peakDbfs), crossfade_s: round3(x / SAMPLE_RATE),
      head_100ms_dbfs: round3(edge.head), tail_100ms_dbfs: round3(edge.tail),
      seam_delta: frameDiff(m.data, m.channels, m.frames - 1, 0), typical_step: typicalStep(m.data, m.channels),
    };
  } finally {
    fs.rmSync(work, { recursive: true, force: true });
  }
}

function isFatal(status, body) {
  if (status === 401 || status === 402) return true;
  const t = body.toLowerCase();
  return t.includes('quota') || t.includes('insufficient credit') || t.includes('payment_required') || t.includes('credit limit');
}

function isMp3(b) {
  return b.length >= 512 && ((b[0] === 0x49 && b[1] === 0x44 && b[2] === 0x33) || (b[0] === 0xff && (b[1] & 0xe0) === 0xe0));
}

function readLog() {
  try {
    const d = JSON.parse(fs.readFileSync(LOG_PATH, 'utf8'));
    if (Array.isArray(d.calls)) return d;
  } catch {
    // first run
  }
  return { endpoint: ENDPOINT, calls: [] };
}

function writeJson(file, data) {
  fs.writeFileSync(file, `${JSON.stringify(data, null, 2)}\n`);
}

async function generate(loop, regenReason) {
  const prompt = `${COMMON} ${loop.text}`;
  const body = { prompt, music_length_ms: LENGTH_MS, model_id: MODEL_ID, force_instrumental: true };
  const t0 = Date.now();
  const res = await fetch(`${ENDPOINT}?output_format=${OUTPUT_FORMAT}`, {
    method: 'POST',
    headers: { 'xi-api-key': apiKey, 'Content-Type': 'application/json', Accept: 'audio/mpeg' },
    body: JSON.stringify(body),
    signal: AbortSignal.timeout(600000),
  });
  const bytes = Buffer.from(await res.arrayBuffer());
  const entry = {
    id: loop.id, timestamp: new Date(t0).toISOString(), endpoint: ENDPOINT, model_id: MODEL_ID, prompt,
    force_instrumental: true, seed: null, requested_ms: LENGTH_MS, output_format: OUTPUT_FORMAT,
    status: res.status, bytes: bytes.length, elapsed_s: round3((Date.now() - t0) / 1000),
    song_id: res.headers.get('song-id'), regen_reason: regenReason || null,
  };
  return { entry, bytes };
}

async function main() {
  const argv = process.argv.slice(2);
  let regen = null;
  let reason = null;
  let convertOnly = false;
  for (let i = 0; i < argv.length; i++) {
    if (argv[i] === '--regen') regen = argv[++i];
    else if (argv[i] === '--reason') reason = argv[++i];
    else if (argv[i] === '--convert-only') convertOnly = true;
    else throw new Error(`unknown argument ${argv[i]}`);
  }
  if (regen && !LOOPS.some((l) => l.id === regen)) throw new Error(`unknown loop ${regen}`);
  if (regen && !reason) throw new Error('--regen needs --reason "<stated defect>"');

  fs.mkdirSync(SRC_DIR, { recursive: true });
  const ledger = readLog();
  if (!convertOnly) apiKey = loadKey();
  let stopped = null;

  if (!convertOnly) {
    for (const loop of LOOPS) {
      const mp3 = path.join(SRC_DIR, `${loop.id}.mp3`);
      const made = ledger.calls.filter((c) => c.id === loop.id && c.status === 200).length;
      const isRegen = regen === loop.id;
      if (regen && !isRegen) continue;
      if (made >= 1 && !isRegen) {
        log(`skip ${loop.id}`);
        continue;
      }
      if (isRegen && made >= 2) throw new Error(`${loop.id} has used its one regeneration`);
      const { entry, bytes } = await generate(loop, isRegen ? reason : null);
      const ok = entry.status === 200 && isMp3(bytes);
      entry.ok = ok;
      if (!ok) entry.error = sanitize(bytes.toString('utf8'));
      ledger.calls.push(entry);
      writeJson(LOG_PATH, ledger);
      if (ok) {
        if (isRegen && fs.existsSync(mp3)) fs.renameSync(mp3, path.join(SRC_DIR, `${loop.id}.first.mp3`));
        fs.writeFileSync(mp3, bytes);
        log(`generated ${loop.id} ${bytes.length} bytes`);
      } else {
        log(`fail ${loop.id} HTTP ${entry.status} ${entry.error}`);
        if (isFatal(entry.status, bytes.toString('utf8'))) stopped = `stopped on HTTP ${entry.status}`;
        break;
      }
    }
  }

  const rows = [];
  for (const loop of LOOPS) {
    const mp3 = path.join(SRC_DIR, `${loop.id}.mp3`);
    if (!fs.existsSync(mp3)) continue;
    const wav = path.join(OUT_DIR, `${loop.id}-loop.wav`);
    const m = await convert(mp3, wav);
    log(`wav ${loop.id} ${m.duration_s}s peak ${m.peak_dbfs} dBFS head ${m.head_100ms_dbfs} tail ${m.tail_100ms_dbfs} seam ${m.seam_delta}/${m.typical_step}`);
    rows.push({
      id: loop.id, file: `${loop.id}-loop.wav`, loop: true, ...m,
      mp3: rel(mp3), wav: rel(wav), bytes: fs.statSync(wav).size,
      sha256: crypto.createHash('sha256').update(fs.readFileSync(wav)).digest('hex'),
    });
  }
  writeJson(MANIFEST_PATH, {
    card: 'T40', provider: 'ElevenLabs', endpoint: ENDPOINT, model_id: MODEL_ID, source_format: OUTPUT_FORMAT,
    sample_rate: SAMPLE_RATE, bit_depth: 16, peak_target_dbfs: -1,
    loop_crossfade: `Same method as tools/sfx/generate.mjs: silence below -50 dBFS trimmed from both ends, the last ${CROSSFADE_S} s faded out and mixed at unity gain with the first ${CROSSFADE_S} s faded in (quarter sine), then the untouched middle appended; the file is ${CROSSFADE_S} s shorter than the trimmed take.`,
    run: { calls: ledger.calls.length, stopped },
    cues: rows,
  });
  log(`calls: ${ledger.calls.length}`);
  if (stopped) {
    log(stopped);
    process.exitCode = 1;
  }
}

main().catch((e) => {
  log(`error ${sanitize(e.message)}`);
  process.exitCode = 1;
});
