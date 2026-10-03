// Greybox SFX starter pack. Node built-ins only.
// One ElevenLabs take per cue (one HTTP retry, then stop on 401/402/quota).
// The API key is read from the environment or pof's .env and is never printed,
// logged, written to a file, or placed on a command line.

import crypto from 'node:crypto';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const APP = path.resolve(HERE, '../..');
const REPO = path.resolve(APP, '../..');
const CUES_PATH = path.join(APP, 'art', 'audio', 'sfx', 'cues.json');
const OUT_DIR = path.join(APP, 'art', 'audio', 'sfx');
const SRC_DIR = path.join(OUT_DIR, 'src');
const MANIFEST_PATH = path.join(OUT_DIR, 'manifest.json');
const HTML_PATH = path.join(OUT_DIR, 'index.html');
const ENV_PATH = 'C:\\Users\\kazda\\kiro\\pof\\.env';
const LEDGER_PATH = path.join(os.tmpdir(), 'a05-sfx-ledger.json');
const ENDPOINT = 'https://api.elevenlabs.io/v1/sound-generation';
const PROMPT_INFLUENCE = 0.4;
const SAMPLE_RATE = 48000;
const SILENCE_LINEAR = 0.00316227766; // -50 dBFS
const SHORT_CROSSFADE_S = 0.08;
const CROWD_CROSSFADE_S = 0.25;
const PEAK_TARGET = Math.round(32768 * 10 ** (-1 / 20)); // -1 dBFS in int16, full scale = 32768

const LOOP_NOTE =
  'Leading silence below -50 dBFS is removed from every cue (5 ms detection window). ' +
  'Loop cues also have trailing silence below -50 dBFS removed, because a dead tail cannot meet the head. ' +
  'The last X seconds are then faded out on a quarter-sine curve and mixed at unity gain with the first X seconds faded in on the same curve. ' +
  'That mix becomes the head and the untouched middle (from X to duration minus X) is appended, so the file is X seconds shorter than the trimmed take. ' +
  'The last sample of the middle is the sample just before the old tail, and the first sample of the mix is that tail at full level, so the waveform continues across the loop point while the content eases from the end back to the start. ' +
  'X is 0.080 s (3840 samples) for the short loops and 0.250 s (12000 samples) for arena-crowd-loop.';

let apiKey = '';

function log(message) {
  fs.writeSync(1, `${message}\n`);
}

function sanitize(text) {
  let out = String(text ?? '');
  if (apiKey) out = out.split(apiKey).join('[redacted]');
  out = out.replace(/\bsk_[A-Za-z0-9_-]{8,}\b/g, '[redacted]');
  return out.replace(/\s+/g, ' ').slice(0, 300);
}

function round4(n) {
  return Math.round(n * 10000) / 10000;
}

function round3(n) {
  return Math.round(n * 1000) / 1000;
}

function esc(text) {
  return String(text)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}

function rel(file) {
  return path.relative(REPO, file).split(path.sep).join('/');
}

function assertInside(file, root) {
  const abs = path.resolve(file).toLowerCase();
  const base = path.resolve(root).toLowerCase();
  const sep = path.sep.toLowerCase();
  if (abs !== base && !abs.startsWith(base + sep)) {
    throw new Error('refusing to write outside the sfx directory');
  }
}

function atomicWrite(file, text) {
  const tmp = `${file}.${process.pid}.partial`;
  fs.writeFileSync(tmp, text);
  if (fs.existsSync(file)) fs.unlinkSync(file);
  try {
    fs.renameSync(tmp, file);
  } catch {
    fs.copyFileSync(tmp, file);
    fs.unlinkSync(tmp);
  }
}

function loadKey() {
  const fromEnv = process.env.ELEVENLABS_API_KEY;
  if (fromEnv && fromEnv.trim()) return fromEnv.trim();
  let text;
  try {
    text = fs.readFileSync(ENV_PATH, 'utf8');
  } catch {
    throw new Error('ELEVENLABS_API_KEY is not set and pof .env could not be read');
  }
  if (text.charCodeAt(0) === 0xfeff) text = text.slice(1);
  for (const raw of text.split(/\r?\n/)) {
    const line = raw.trim();
    if (!line || line.startsWith('#')) continue;
    const eq = line.indexOf('=');
    if (eq < 0) continue;
    let name = line.slice(0, eq).trim();
    if (name.startsWith('export ')) name = name.slice(7).trim();
    if (name !== 'ELEVENLABS_API_KEY') continue;
    let value = line.slice(eq + 1).trim();
    if (
      (value.startsWith('"') && value.endsWith('"')) ||
      (value.startsWith("'") && value.endsWith("'"))
    ) {
      value = value.slice(1, -1);
    }
    if (!value) break;
    return value;
  }
  throw new Error('ELEVENLABS_API_KEY is not set');
}

function run(bin, args) {
  return new Promise((resolve, reject) => {
    const child = spawn(bin, args, { windowsHide: true });
    const out = [];
    const err = [];
    child.stdout.on('data', (chunk) => out.push(chunk));
    child.stderr.on('data', (chunk) => err.push(chunk));
    child.on('error', (error) => reject(error));
    child.on('close', (code) => {
      const stderr = Buffer.concat(err).toString('utf8');
      if (code === 0) {
        resolve({ stdout: Buffer.concat(out).toString('utf8'), stderr });
        return;
      }
      reject(new Error(sanitize(`${bin} exited ${code}: ${stderr.slice(-600)}`)));
    });
  });
}

function ffmpeg(args) {
  return run('ffmpeg', ['-hide_banner', '-loglevel', 'error', '-y', ...args]);
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
      fmt = {
        audioFormat: buf.readUInt16LE(start),
        channels: buf.readUInt16LE(start + 2),
        sampleRate: buf.readUInt32LE(start + 4),
        bits: buf.readUInt16LE(start + 14),
      };
    } else if (id === 'data') {
      data = Buffer.from(buf.subarray(start, start + size));
    }
    offset = start + size + (size % 2);
  }
  if (!fmt || !data) throw new Error('wav is missing fmt or data');
  if (fmt.audioFormat !== 1 || fmt.bits !== 16) {
    throw new Error(`expected pcm 16-bit, got format ${fmt.audioFormat} bits ${fmt.bits}`);
  }
  const bytesPerFrame = fmt.channels * 2;
  if (bytesPerFrame <= 0 || data.length % bytesPerFrame !== 0) {
    throw new Error('wav data length is not a whole number of frames');
  }
  return { ...fmt, data, frames: data.length / bytesPerFrame };
}

function writeWav(file, pcm, sampleRate, channels) {
  const header = Buffer.alloc(44);
  header.write('RIFF', 0);
  header.writeUInt32LE(36 + pcm.length, 4);
  header.write('WAVE', 8);
  header.write('fmt ', 12);
  header.writeUInt32LE(16, 16);
  header.writeUInt16LE(1, 20);
  header.writeUInt16LE(channels, 22);
  header.writeUInt32LE(sampleRate, 24);
  header.writeUInt32LE(sampleRate * channels * 2, 28);
  header.writeUInt16LE(channels * 2, 32);
  header.writeUInt16LE(16, 34);
  header.write('data', 36);
  header.writeUInt32LE(pcm.length, 40);
  fs.writeFileSync(file, Buffer.concat([header, pcm]));
}

function measureWav(file) {
  const parsed = parseWav(fs.readFileSync(file));
  let peak = 0;
  for (let i = 0; i < parsed.data.length; i += 2) {
    const sample = Math.abs(parsed.data.readInt16LE(i));
    if (sample > peak) peak = sample;
  }
  const peakDbfs = peak === 0 ? -Infinity : 20 * Math.log10(peak / 32768);
  return {
    ...parsed,
    peak,
    peakDbfs,
    duration: parsed.frames / parsed.sampleRate,
  };
}

function frameDiff(pcm, channels, frameA, frameB) {
  let max = 0;
  for (let c = 0; c < channels; c++) {
    const a = pcm.readInt16LE((frameA * channels + c) * 2);
    const b = pcm.readInt16LE((frameB * channels + c) * 2);
    const diff = Math.abs(a - b);
    if (diff > max) max = diff;
  }
  return max;
}

function continuity(pcm, channels) {
  const frames = pcm.length / 2 / channels;
  if (frames < 8) return { boundary: 0, typical: 1, ratio: 0 };
  const steps = [];
  const start = Math.max(0, Math.floor(frames * 0.4));
  const end = Math.min(frames - 1, start + 400);
  for (let i = start; i < end; i++) steps.push(frameDiff(pcm, channels, i, i + 1));
  steps.sort((a, b) => a - b);
  const typical = Math.max(1, steps[Math.floor(steps.length / 2)] || 1);
  const boundary = frameDiff(pcm, channels, frames - 1, 0);
  return { boundary, typical, ratio: boundary / typical, frames };
}

function silenceFilter() {
  return `silenceremove=start_periods=1:start_duration=0:start_threshold=${SILENCE_LINEAR}:detection=peak:window=0.005`;
}

async function trimToPcm(input, output, channels, loop) {
  const lead = silenceFilter();
  const chain = loop
    ? `aresample=${SAMPLE_RATE},${lead},areverse,${lead},areverse`
    : `aresample=${SAMPLE_RATE},${lead}`;
  const encode = ['-ar', String(SAMPLE_RATE), '-ac', String(channels), '-c:a', 'pcm_s16le', output];
  try {
    await ffmpeg(['-i', input, '-af', chain, ...encode]);
    const info = measureWav(output);
    if (info.frames < Math.round(SAMPLE_RATE * 0.01)) {
      throw new Error('trim left under 10 ms');
    }
    return info;
  } catch (error) {
    await ffmpeg(['-i', input, '-af', `aresample=${SAMPLE_RATE}`, ...encode]);
    log(`trim fallback (${sanitize(error.message)})`);
    return measureWav(output);
  }
}

function loopGraph(frameCount, crossfadeSamples, channels) {
  const midEnd = frameCount - crossfadeSamples;
  const fade = (crossfadeSamples / SAMPLE_RATE).toFixed(6);
  const layout = channels === 1 ? 'mono' : 'stereo';
  const fmt = `aformat=sample_rates=${SAMPLE_RATE}:channel_layouts=${layout}`;
  return [
    '[0:a]asplit=3[s0][s1][s2]',
    `[s0]atrim=start_sample=0:end_sample=${crossfadeSamples},asetpts=PTS-STARTPTS,afade=t=in:st=0:d=${fade}:curve=qsin[head]`,
    `[s1]atrim=start_sample=${midEnd}:end_sample=${frameCount},asetpts=PTS-STARTPTS,afade=t=out:st=0:d=${fade}:curve=qsin[tail]`,
    '[tail][head]amix=inputs=2:duration=longest:dropout_transition=0:normalize=0[xf]',
    `[s2]atrim=start_sample=${crossfadeSamples}:end_sample=${midEnd},asetpts=PTS-STARTPTS[mid]`,
    `[xf]${fmt}[xf2]`,
    `[mid]${fmt}[mid2]`,
    '[xf2][mid2]concat=n=2:v=0:a=1[out]',
  ].join(';');
}

async function applyLoopCrossfade(input, output, channels, crossfadeSeconds) {
  const info = measureWav(input);
  let crossfadeSamples = Math.round(crossfadeSeconds * info.sampleRate);
  const maxSamples = Math.floor(info.frames / 4);
  if (crossfadeSamples > maxSamples) crossfadeSamples = Math.max(1, maxSamples);
  const graph = loopGraph(info.frames, crossfadeSamples, channels);
  await ffmpeg([
    '-i', input,
    '-filter_complex', graph,
    '-map', '[out]',
    '-ar', String(SAMPLE_RATE),
    '-ac', String(channels),
    '-c:a', 'pcm_s16le',
    output,
  ]);
  return { crossfadeSamples, crossfadeSeconds: crossfadeSamples / SAMPLE_RATE };
}

function peakNormalize(file) {
  const parsed = parseWav(fs.readFileSync(file));
  let peak = 0;
  for (let i = 0; i < parsed.data.length; i += 2) {
    const sample = Math.abs(parsed.data.readInt16LE(i));
    if (sample > peak) peak = sample;
  }
  if (peak === 0) throw new Error('audio is silent');
  const gain = PEAK_TARGET / peak;
  const pcm = Buffer.from(parsed.data);
  for (let i = 0; i < pcm.length; i += 2) {
    let sample = Math.round(pcm.readInt16LE(i) * gain);
    if (sample > 32767) sample = 32767;
    if (sample < -32768) sample = -32768;
    pcm.writeInt16LE(sample, i);
  }
  writeWav(file, pcm, parsed.sampleRate, parsed.channels);
}

async function convertFile(input, output, options) {
  const channels = options.channels;
  const work = fs.mkdtempSync(path.join(os.tmpdir(), 'a05-wav-'));
  try {
    const trimmed = path.join(work, 'trimmed.wav');
    const looped = path.join(work, 'looped.wav');
    await trimToPcm(input, trimmed, channels, options.loop);
    let crossfadeSeconds = 0;
    let source = trimmed;
    if (options.loop) {
      const faded = await applyLoopCrossfade(trimmed, looped, channels, options.crossfadeSeconds);
      crossfadeSeconds = faded.crossfadeSeconds;
      source = looped;
    }
    fs.copyFileSync(source, output);
    peakNormalize(output);
    const measured = measureWav(output);
    if (measured.sampleRate !== SAMPLE_RATE || measured.channels !== channels || measured.bits !== 16) {
      throw new Error(`bad wav format ${measured.sampleRate} Hz ${measured.channels} ch ${measured.bits} bit`);
    }
    const cont = continuity(measured.data, channels);
    let join = null;
    if (options.loop && crossfadeSeconds > 0) {
      const at = Math.round(crossfadeSeconds * measured.sampleRate);
      if (at > 0 && at < measured.frames) {
        join = frameDiff(measured.data, channels, at - 1, at);
      }
    }
    return {
      sampleRate: measured.sampleRate,
      channels: measured.channels,
      bits: measured.bits,
      frames: measured.frames,
      duration: measured.duration,
      peak: measured.peak,
      peakDbfs: measured.peakDbfs,
      crossfadeSeconds,
      seam: cont,
      join,
    };
  } finally {
    fs.rmSync(work, { recursive: true, force: true });
  }
}

function hasMp3Magic(buf) {
  if (!buf || buf.length < 2) return false;
  if (buf.length >= 3 && buf[0] === 0x49 && buf[1] === 0x44 && buf[2] === 0x33) return true;
  return buf[0] === 0xff && (buf[1] & 0xe0) === 0xe0;
}

function isMp3(buf) {
  return Boolean(buf) && buf.length >= 512 && hasMp3Magic(buf);
}

function validMp3(file) {
  if (!fs.existsSync(file)) return false;
  const stat = fs.statSync(file);
  if (stat.size < 512) return false;
  const fd = fs.openSync(file, 'r');
  try {
    const head = Buffer.alloc(4);
    fs.readSync(fd, head, 0, 4, 0);
    return hasMp3Magic(head);
  } finally {
    fs.closeSync(fd);
  }
}

function isFatal(status, body) {
  if (status === 401 || status === 402) return true;
  const text = body.toLowerCase();
  return text.includes('quota') || text.includes('insufficient credit') || text.includes('payment_required') || text.includes('credit limit');
}

function readLedger(previousRun) {
  try {
    const data = JSON.parse(fs.readFileSync(LEDGER_PATH, 'utf8'));
    if (data && Array.isArray(data.events)) return data;
  } catch {
    // A missing ledger is the first run, or temp was cleared.
  }
  return {
    baseline_requests: previousRun?.requests || 0,
    baseline_seconds: previousRun?.generated_seconds || 0,
    baseline_retries: previousRun?.retries || 0,
    events: [],
  };
}

function ledgerTotals(ledger) {
  let seconds = Number(ledger.baseline_seconds) || 0;
  let retries = Number(ledger.baseline_retries) || 0;
  for (const event of ledger.events) {
    if (event.ok) seconds += Number(event.seconds) || 0;
    if (event.attempt > 1) retries += 1;
  }
  return {
    requests: (Number(ledger.baseline_requests) || 0) + ledger.events.length,
    generated_seconds: round4(seconds),
    retries,
  };
}

class StopRun extends Error {}

async function postSound(body) {
  const response = await fetch(ENDPOINT, {
    method: 'POST',
    headers: {
      'xi-api-key': apiKey,
      'Content-Type': 'application/json',
      Accept: 'audio/mpeg',
    },
    body: JSON.stringify(body),
    signal: AbortSignal.timeout(180000),
  });
  const bytes = Buffer.from(await response.arrayBuffer());
  const cost = response.headers.get('character-cost');
  return {
    status: response.status,
    bytes,
    contentType: response.headers.get('content-type') || '',
    characterCost: cost && /^\d+$/.test(cost) ? Number(cost) : null,
  };
}

async function generateMp3(cue, prompt, mp3Path, ledger) {
  const body = {
    text: prompt,
    duration_seconds: cue.seconds,
    prompt_influence: PROMPT_INFLUENCE,
  };
  if (cue.loop) body.loop = true;
  let droppedLoop = false;
  for (let attempt = 1; attempt <= 2; attempt++) {
    const sent = droppedLoop ? { ...body } : body;
    if (droppedLoop) delete sent.loop;
    let result;
    try {
      result = await postSound(sent);
    } catch (error) {
      ledger.events.push({
        id: cue.id,
        ok: false,
        seconds: cue.seconds,
        status: 0,
        attempt,
      });
      atomicWrite(LEDGER_PATH, JSON.stringify(ledger));
      if (attempt === 2) throw new Error(sanitize(error.message));
      log(`retry ${cue.id} after a network error`);
      await new Promise((resolve) => setTimeout(resolve, 1500));
      continue;
    }
    const audio = isMp3(result.bytes);
    ledger.events.push({
      id: cue.id,
      ok: result.status === 200 && audio,
      seconds: cue.seconds,
      status: result.status,
      attempt,
      character_cost: result.characterCost,
    });
    atomicWrite(LEDGER_PATH, JSON.stringify(ledger));
    if (result.status === 200 && audio) {
      atomicWrite(mp3Path, result.bytes);
      return;
    }
    const errText = result.bytes.toString('utf8');
    if (isFatal(result.status, errText)) {
      throw new StopRun(`stopped on HTTP ${result.status} for ${cue.id}`);
    }
    if (attempt === 1 && result.status === 422 && body.loop && /loop/i.test(errText)) {
      droppedLoop = true;
      log(`retry ${cue.id} without the loop flag after HTTP 422`);
      await new Promise((resolve) => setTimeout(resolve, 500));
      continue;
    }
    if (attempt === 2) {
      throw new Error(`HTTP ${result.status} for ${cue.id}: ${sanitize(errText)}`);
    }
    log(`retry ${cue.id} after HTTP ${result.status}`);
    await new Promise((resolve) => setTimeout(resolve, 1500));
  }
}

function wavAcceptable(file, channels) {
  try {
    const info = measureWav(file);
    return info.sampleRate === SAMPLE_RATE
      && info.bits === 16
      && info.channels === channels
      && info.frames > Math.round(SAMPLE_RATE * 0.05)
      && info.peakDbfs <= -0.9
      && info.peakDbfs >= -1.2;
  } catch {
    return false;
  }
}

function writeHtml(rows, common) {
  const body = rows.map((row) => {
    const loopAttr = row.loop ? ' loop' : '';
    const badge = row.loop ? ' <span class="badge">loop</span>' : '';
    return `    <article class="cue">
      <h2>${esc(row.id)}${badge}</h2>
      <p>${esc(row.cueText)}</p>
      <audio controls preload="none"${loopAttr} src="${esc(row.file)}"> </audio>
      <p class="dur">${row.duration.toFixed(2)} s</p>
    </article>`;
  }).join('\n');
  const html = `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Greybox SFX starter pack</title>
  <style>
    :root { color-scheme: dark; }
    * { box-sizing: border-box; }
    body { margin: 0; background: #141414; color: #eceae4; font: 18px/1.45 "Segoe UI", system-ui, sans-serif; }
    header, main { padding: 28px 32px; max-width: 64rem; }
    h1 { font-size: 32px; font-weight: 650; margin: 0 0 12px; letter-spacing: -0.02em; }
    h2 { font-size: 20px; font-weight: 650; margin: 0 0 6px; font-family: ui-monospace, Consolas, monospace; }
    p { margin: 0 0 10px; }
    .lede { max-width: 70ch; color: #d4d0c8; }
    .cue { padding: 16px 0; border-top: 1px solid #333; }
    .badge { font-family: "Segoe UI", system-ui, sans-serif; font-size: 16px; color: #9fd0c8; margin-left: 8px; }
    audio { width: min(100%, 36rem); display: block; margin: 8px 0; }
    .dur { color: #b7b3aa; }
    @media (max-width: 700px) {
      header, main { padding: 18px 16px; }
    }
  </style>
</head>
<body>
  <header>
    <h1>Greybox SFX starter pack</h1>
    <p class="lede">Twenty style-neutral placeholders for the mechanics gate. Each prompt was the common line followed by the sentence below. Common line: ${esc(common)}</p>
    <p class="lede">Files are 48 kHz, 16-bit, peak-normalised to -1 dBFS, mono except the stereo arena crowd. Looping rows repeat so the join can be heard. ${esc(LOOP_NOTE)}</p>
  </header>
  <main>
${body}
  </main>
</body>
</html>
`;
  atomicWrite(HTML_PATH, html);
}

function parseArgs(argv) {
  const force = new Set();
  let selfTest = false;
  let reconvert = false;
  for (let i = 2; i < argv.length; i++) {
    const arg = argv[i];
    if (arg === '--self-test') selfTest = true;
    else if (arg === '--reconvert') reconvert = true;
    else if (arg === '--force') {
      const id = argv[++i];
      if (!id) throw new Error('--force needs a cue id');
      force.add(id);
    } else {
      throw new Error(`unknown argument ${arg}`);
    }
  }
  return { force, selfTest, reconvert };
}

function writeTestWav(file, frames, channels, sampleAt) {
  const pcm = Buffer.alloc(frames * channels * 2);
  for (let i = 0; i < frames; i++) {
    for (let c = 0; c < channels; c++) {
      let value = sampleAt(i, c, frames);
      if (value > 1) value = 1;
      if (value < -1) value = -1;
      pcm.writeInt16LE(Math.round(value * 32767), (i * channels + c) * 2);
    }
  }
  writeWav(file, pcm, SAMPLE_RATE, channels);
}

function assertNear(name, value, expected, tolerance) {
  if (Math.abs(value - expected) > tolerance) {
    throw new Error(`${name}: got ${value}, expected ${expected} ± ${tolerance}`);
  }
}

async function selfTest() {
  const work = fs.mkdtempSync(path.join(os.tmpdir(), 'a05-self-'));
  try {
    const rampIn = path.join(work, 'ramp.wav');
    const rampOut = path.join(work, 'ramp-out.wav');
    const rampFrames = SAMPLE_RATE * 2;
    writeTestWav(rampIn, rampFrames, 1, (i, _c, frames) => 0.2 + 0.7 * (i / (frames - 1)));
    const ramp = await convertFile(rampIn, rampOut, { loop: true, channels: 1, crossfadeSeconds: SHORT_CROSSFADE_S });
    const rampLimit = Math.max(64, ramp.seam.typical * 8);
    if (ramp.seam.boundary > rampLimit) {
      throw new Error(`ramp loop seam ${ramp.seam.boundary} exceeds ${rampLimit}`);
    }
    if (ramp.join == null || ramp.join > rampLimit) {
      throw new Error(`ramp crossfade join ${ramp.join} exceeds ${rampLimit}`);
    }
    assertNear('ramp peak dBFS', ramp.peakDbfs, -1, 0.02);
    assertNear('ramp frames', ramp.frames, rampFrames - Math.round(SHORT_CROSSFADE_S * SAMPLE_RATE), 48);
    if (ramp.channels !== 1 || ramp.sampleRate !== SAMPLE_RATE) throw new Error('ramp format');

    const stereoIn = path.join(work, 'stereo.wav');
    const stereoOut = path.join(work, 'stereo-out.wav');
    writeTestWav(stereoIn, rampFrames, 2, (i, c, frames) => (c === 0 ? -0.4 : 0.55) * (i / (frames - 1) * 2 - 1));
    const stereo = await convertFile(stereoIn, stereoOut, { loop: true, channels: 2, crossfadeSeconds: CROWD_CROSSFADE_S });
    if (stereo.channels !== 2) throw new Error(`stereo became ${stereo.channels} ch`);
    const stereoLimit = Math.max(64, stereo.seam.typical * 8);
    if (stereo.seam.boundary > stereoLimit) {
      throw new Error(`stereo loop seam ${stereo.seam.boundary} exceeds ${stereoLimit}`);
    }
    assertNear('stereo peak dBFS', stereo.peakDbfs, -1, 0.02);

    const leadIn = path.join(work, 'lead.wav');
    const leadOut = path.join(work, 'lead-out.wav');
    const silenceFrames = Math.round(SAMPLE_RATE * 0.3);
    const toneFrames = Math.round(SAMPLE_RATE * 0.5);
    writeTestWav(leadIn, silenceFrames + toneFrames, 1, (i) => {
      if (i < silenceFrames) return 0;
      return 0.25 * Math.sin((2 * Math.PI * 440 * (i - silenceFrames)) / SAMPLE_RATE);
    });
    const lead = await convertFile(leadIn, leadOut, { loop: false, channels: 1, crossfadeSeconds: 0 });
    if (lead.duration < 0.45 || lead.duration > 0.56) {
      throw new Error(`leading silence trim left ${lead.duration}s`);
    }
    const leadWav = measureWav(leadOut);
    let early = 0;
    const earlyFrames = Math.round(SAMPLE_RATE * 0.03);
    for (let i = 0; i < earlyFrames; i++) {
      early = Math.max(early, Math.abs(leadWav.data.readInt16LE(i * 2)));
    }
    if (early < leadWav.peak * 0.5) throw new Error('sound still starts with silence');
    assertNear('lead peak dBFS', lead.peakDbfs, -1, 0.02);

    const toneIn = path.join(work, 'tone.wav');
    const toneOut = path.join(work, 'tone-out.wav');
    writeTestWav(toneIn, toneFrames, 1, (i) => 0.3 * Math.sin((2 * Math.PI * 440 * i) / SAMPLE_RATE));
    const tone = await convertFile(toneIn, toneOut, { loop: false, channels: 1, crossfadeSeconds: 0 });
    if (tone.duration < 0.48 || tone.duration > 0.52) {
      throw new Error(`plain tone duration drifted to ${tone.duration}s`);
    }
    log('self-test passed');
    log(`ramp seam ${ramp.seam.boundary} typical ${ramp.seam.typical} join ${ramp.join} frames ${ramp.frames}`);
    log(`stereo seam ${stereo.seam.boundary} typical ${stereo.seam.typical} frames ${stereo.frames} ch ${stereo.channels}`);
    log(`lead ${lead.duration.toFixed(3)}s early-peak ${early} tone ${tone.duration.toFixed(3)}s`);
  } finally {
    fs.rmSync(work, { recursive: true, force: true });
  }
}

async function main() {
  const args = parseArgs(process.argv);
  if (args.selfTest) {
    await selfTest();
    return;
  }
  apiKey = loadKey();
  const sheet = JSON.parse(fs.readFileSync(CUES_PATH, 'utf8'));
  if (!sheet.common || !Array.isArray(sheet.cues) || sheet.cues.length !== 20) {
    throw new Error('cues.json does not contain the common line and 20 cues');
  }
  const ids = new Set();
  for (const cue of sheet.cues) {
    if (!/^[a-z0-9-]+$/.test(cue.id)) throw new Error(`unsafe cue id ${cue.id}`);
    if (ids.has(cue.id)) throw new Error(`duplicate cue id ${cue.id}`);
    ids.add(cue.id);
    if (typeof cue.text !== 'string' || cue.text.length === 0) throw new Error(`missing text for ${cue.id}`);
    if (typeof cue.seconds !== 'number' || cue.seconds < 0.5 || cue.seconds > 30) {
      throw new Error(`duration out of range for ${cue.id}`);
    }
  }
  for (const id of args.force) {
    if (!ids.has(id)) throw new Error(`unknown cue ${id}`);
  }

  fs.mkdirSync(SRC_DIR, { recursive: true });
  let previous = null;
  try {
    previous = JSON.parse(fs.readFileSync(MANIFEST_PATH, 'utf8'));
  } catch {
    previous = null;
  }
  const ledger = readLedger(previous?.run);
  const failures = [];
  let stopped = null;

  try {
    for (const cue of sheet.cues) {
      const mp3Path = path.join(SRC_DIR, `${cue.id}.mp3`);
      assertInside(mp3Path, OUT_DIR);
      if (args.force.has(cue.id) && fs.existsSync(mp3Path)) fs.unlinkSync(mp3Path);
      if (validMp3(mp3Path)) {
        log(`skip ${cue.id}`);
        continue;
      }
      const prompt = `${sheet.common} ${cue.text}`;
      try {
        await generateMp3(cue, prompt, mp3Path, ledger);
        log(`generated ${cue.id} ${cue.seconds}s`);
      } catch (error) {
        if (error instanceof StopRun) {
          stopped = sanitize(error.message);
          log(stopped);
          break;
        }
        failures.push({ id: cue.id, stage: 'generate', error: sanitize(error.message) });
        log(`fail ${cue.id} ${sanitize(error.message)}`);
      }
    }

    const cueRows = [];
    for (const cue of sheet.cues) {
      const mp3Path = path.join(SRC_DIR, `${cue.id}.mp3`);
      const wavPath = path.join(OUT_DIR, `${cue.id}.wav`);
      assertInside(wavPath, OUT_DIR);
      if (!validMp3(mp3Path)) continue;
      const channels = cue.id === 'arena-crowd-loop' ? 2 : 1;
      const crossfadeSeconds = cue.id === 'arena-crowd-loop' ? CROWD_CROSSFADE_S : SHORT_CROSSFADE_S;
      const fresh = !args.reconvert
        && !args.force.has(cue.id)
        && wavAcceptable(wavPath, channels)
        && fs.statSync(wavPath).mtimeMs >= fs.statSync(mp3Path).mtimeMs;
      let measured;
      try {
        if (!fresh) {
          const partial = path.join(OUT_DIR, `.${cue.id}.partial.wav`);
          measured = await convertFile(mp3Path, partial, {
            loop: cue.loop,
            channels,
            crossfadeSeconds,
          });
          if (fs.existsSync(wavPath)) fs.unlinkSync(wavPath);
          fs.renameSync(partial, wavPath);
          log(`wav ${cue.id} ${measured.duration.toFixed(3)}s peak ${measured.peakDbfs.toFixed(3)} dBFS`);
        } else {
          const info = measureWav(wavPath);
          measured = {
            sampleRate: info.sampleRate,
            channels: info.channels,
            bits: info.bits,
            frames: info.frames,
            duration: info.duration,
            peak: info.peak,
            peakDbfs: info.peakDbfs,
            crossfadeSeconds: cue.loop ? crossfadeSeconds : 0,
            seam: continuity(info.data, channels),
            join: null,
          };
          log(`wav-kept ${cue.id}`);
        }
      } catch (error) {
        failures.push({ id: cue.id, stage: 'convert', error: sanitize(error.message) });
        log(`fail ${cue.id} convert ${sanitize(error.message)}`);
        continue;
      }
      const bytes = fs.readFileSync(wavPath);
      const prompt = `${sheet.common} ${cue.text}`;
      cueRows.push({
        id: cue.id,
        text: prompt,
        cue_text: cue.text,
        requested_seconds: cue.seconds,
        actual_seconds: round4(measured.duration),
        sample_rate: measured.sampleRate,
        bit_depth: measured.bits,
        channels: measured.channels,
        peak_dbfs: round3(measured.peakDbfs),
        loop: Boolean(cue.loop),
        crossfade_seconds: cue.loop ? round4(measured.crossfadeSeconds) : 0,
        seam_delta: cue.loop ? measured.seam.boundary : null,
        mp3: rel(mp3Path),
        wav: rel(wavPath),
        path: rel(wavPath),
        sha256: crypto.createHash('sha256').update(bytes).digest('hex'),
        cueText: cue.text,
        file: `${cue.id}.wav`,
        duration: measured.duration,
      });
    }

    const totals = ledgerTotals(ledger);
    const manifest = {
      card: 'A05',
      provider: 'ElevenLabs',
      endpoint: ENDPOINT,
      prompt_influence: PROMPT_INFLUENCE,
      sample_rate: SAMPLE_RATE,
      bit_depth: 16,
      peak_target_dbfs: -1,
      channels_note: 'mono, except arena-crowd-loop which is stereo',
      loop_crossfade: LOOP_NOTE,
      run: {
        requests: totals.requests,
        retries: totals.retries,
        generated_seconds: totals.generated_seconds,
        stopped,
        failures,
      },
      cues: cueRows.map((row) => ({
        id: row.id,
        text: row.text,
        cue_text: row.cue_text,
        requested_seconds: row.requested_seconds,
        actual_seconds: row.actual_seconds,
        sample_rate: row.sample_rate,
        bit_depth: row.bit_depth,
        channels: row.channels,
        peak_dbfs: row.peak_dbfs,
        loop: row.loop,
        crossfade_seconds: row.crossfade_seconds,
        seam_delta: row.seam_delta,
        mp3: row.mp3,
        wav: row.wav,
        path: row.path,
        sha256: row.sha256,
      })),
    };
    atomicWrite(MANIFEST_PATH, `${JSON.stringify(manifest, null, 2)}\n`);
    writeHtml(cueRows, sheet.common);
    log(`wavs: ${cueRows.length}`);
    if (stopped) log(stopped);
    if (failures.length) log(`failures: ${failures.length}`);
    log(`generated_seconds: ${totals.generated_seconds}`);
    log(`requests: ${totals.requests}`);
    if (stopped || failures.length || cueRows.length !== 20) process.exitCode = 1;
  } catch (error) {
    log(`error ${sanitize(error.message)}`);
    log(`requests: ${ledgerTotals(ledger).requests}`);
    process.exitCode = 1;
  }
}

main().catch((error) => {
  log(`error ${sanitize(error.stack || error.message)}`);
  process.exitCode = 1;
});
