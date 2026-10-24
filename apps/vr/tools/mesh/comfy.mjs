#!/usr/bin/env node
// The local ComfyUI for TRELLIS.2 on the x64 host: start it, guard the GPU, run one job through the repo's
// unchanged apps/vr/tools/assets/trellis.mjs, stop it. Node built-ins only.
//
//   node apps/vr/tools/mesh/comfy.mjs start     start ComfyUI on 127.0.0.1:8188 and wait until it answers
//   node apps/vr/tools/mesh/comfy.mjs guard     wait until the GPU is free for a job (below), polling every 2 minutes
//   node apps/vr/tools/mesh/comfy.mjs run <image> <label> <seed> <outdir> [--faces N]
//                                               guard, then trellis.mjs run, sampling VRAM; writes <outdir>/gpu-run.json
//   node apps/vr/tools/mesh/comfy.mjs run <front> <label> <seed> <outdir> --back <image> [--faces N]
//                                               the same with a second view: trellis-mv.mjs builds the front + back TRELLIS.2 workflow
//   node apps/vr/tools/mesh/comfy.mjs run <front> <label> <seed> <outdir> [--back <image>] --dry-run
//                                               print the workflow JSON that would be sent; no guard, no ComfyUI, no upload
//   node apps/vr/tools/mesh/comfy.mjs free      unload the models (trellis.mjs free)
//   node apps/vr/tools/mesh/comfy.mjs stop      stop the ComfyUI this script started, and nothing else
//   node apps/vr/tools/mesh/comfy.mjs status
//
// The install lives outside every repo: MAGEVR_COMFY_DIR, default C:/Users/kazda/ai-tools/ComfyUI, with its own venv
// (see README.md). start refuses when anything already answers on 8188 (never a second ComfyUI). stop kills only the
// process id start recorded, after checking it is still that install's main.py.
// The guard, before any GPU job: the ComfyUI queue is empty; free RAM is at least 8 GB; the GPU is not busy with
// another job: utilisation under 40 % over three samples, and the VRAM held outside this ComfyUI under 6 GB. On this
// WDDM host nvidia-smi lists every desktop app as a compute process, so the GPU itself is measured, not a process
// list. Unreal processes are reported, never touched.
import { spawn, spawnSync } from 'node:child_process';
import { existsSync, mkdirSync, openSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { freemem } from 'node:os';
import { join, resolve } from 'node:path';
import { HERE } from './blender.mjs';
import { dryRun, parseRunArgs } from './trellis-mv.mjs';

const COMFY = resolve(process.env.MAGEVR_COMFY_DIR || 'C:/Users/kazda/ai-tools/ComfyUI');
const PY = join(COMFY, 'venv', 'Scripts', 'python.exe');
const URL_ = 'http://127.0.0.1:8188';
const PIDFILE = join(COMFY, 'magevr-comfy.pid');
const LOG = join(COMFY, 'user', 'magevr-comfy.log');
const TRELLIS = join(HERE, '..', 'assets', 'trellis.mjs');
const TRELLIS_MV = join(HERE, 'trellis-mv.mjs');
const MIN_FREE_RAM = 8 * 2 ** 30;
const MAX_UTIL = 40;
const MAX_OTHER_VRAM_MIB = 6144;
const POLL_MS = 120000;

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const stamp = () => new Date().toISOString();
const gib = (b) => (b / 2 ** 30).toFixed(1);

async function getJson(p) {
  const r = await fetch(URL_ + p, { signal: AbortSignal.timeout(5000) });
  return r.json();
}
async function alive() {
  try {
    return await getJson('/system_stats');
  } catch {
    return null;
  }
}

function gpu() {
  const r = spawnSync('nvidia-smi', ['--query-gpu=memory.used,memory.total,utilization.gpu', '--format=csv,noheader,nounits'], { encoding: 'utf8', windowsHide: true });
  if (r.status !== 0) throw new Error('nvidia-smi failed');
  const [used, total, util] = r.stdout.trim().split('\n')[0].split(',').map((x) => Number(x.trim()));
  return { usedMiB: used, totalMiB: total, util };
}

function unrealProcesses() {
  const r = spawnSync('tasklist', ['/fo', 'csv', '/nh'], { encoding: 'utf8', windowsHide: true });
  return [...new Set((r.stdout || '').split('\n').map((l) => l.split('","')[0].replace(/^"/, '')).filter((n) => /unreal/i.test(n)))];
}

function processCommandLine(pid) {
  const r = spawnSync('powershell', ['-NoProfile', '-Command', `(Get-CimInstance Win32_Process -Filter "ProcessId=${Number(pid)}").CommandLine`], { encoding: 'utf8', windowsHide: true });
  return (r.stdout || '').trim();
}

/** One look at everything the guard needs; ok is true when a job may start now. */
async function look() {
  const stats = await alive();
  const q = stats ? await getJson('/queue') : null;
  const busy = q ? q.queue_running.length + q.queue_pending.length : null;
  const samples = [];
  for (let i = 0; i < 3; i++) {
    samples.push(gpu());
    if (i < 2) await sleep(1000);
  }
  const ours = stats?.devices?.[0]?.torch_vram_total ? stats.devices[0].torch_vram_total / 2 ** 20 : 0;
  const otherVram = Math.max(0, Math.round(samples[2].usedMiB - ours));
  const util = Math.max(...samples.map((s) => s.util));
  const free = freemem();
  const reasons = [];
  if (busy == null) reasons.push('ComfyUI not answering on 8188');
  else if (busy) reasons.push(`ComfyUI queue busy (${busy})`);
  if (free < MIN_FREE_RAM) reasons.push(`free RAM ${gib(free)} GB < 8 GB`);
  if (util >= MAX_UTIL) reasons.push(`GPU utilisation ${util} %`);
  if (otherVram >= MAX_OTHER_VRAM_MIB) reasons.push(`${otherVram} MiB VRAM held outside this ComfyUI`);
  return { ok: !reasons.length, reasons, freeRamGB: Number(gib(free)), gpuUtil: util, otherVramMiB: otherVram, queue: busy, unreal: unrealProcesses() };
}

async function guard() {
  for (;;) {
    const l = await look();
    console.log(`${stamp()} guard ${l.ok ? 'clear' : 'wait'}: free RAM ${l.freeRamGB} GB, GPU ${l.gpuUtil} %, other VRAM ${l.otherVramMiB} MiB, queue ${l.queue}${l.unreal.length ? `, Unreal running (not touched): ${l.unreal.join(' ')}` : ''}${l.ok ? '' : `; ${l.reasons.join('; ')}; next look in 2 min`}`);
    if (l.ok) return l;
    if (l.queue == null) throw new Error('ComfyUI is not running: node apps/vr/tools/mesh/comfy.mjs start');
    await sleep(POLL_MS);
  }
}

async function start() {
  const s = await alive();
  if (s) throw new Error(`something already answers on ${URL_} (ComfyUI ${s.system?.comfyui_version ?? '?'}); not starting a second one`);
  if (!existsSync(PY)) throw new Error(`no venv python at ${PY}; see README.md for the install`);
  mkdirSync(join(COMFY, 'user'), { recursive: true });
  const fd = openSync(LOG, 'a');
  const child = spawn(PY, ['main.py', '--listen', '127.0.0.1', '--port', '8188', '--disable-auto-launch'], { cwd: COMFY, detached: true, stdio: ['ignore', fd, fd], windowsHide: true });
  child.unref();
  writeFileSync(PIDFILE, JSON.stringify({ pid: child.pid, startedAt: stamp(), comfy: COMFY }) + '\n');
  const t0 = Date.now();
  while (Date.now() - t0 < 5 * 60 * 1000) {
    await sleep(3000);
    const st = await alive();
    if (st) {
      console.log(`${stamp()} ComfyUI ${st.system?.comfyui_version} up as pid ${child.pid} after ${Math.round((Date.now() - t0) / 1000)} s (python ${st.system?.python_version?.split(' ')[0]}, torch ${st.system?.pytorch_version}, ${st.devices?.[0]?.name}); log ${LOG}`);
      return;
    }
  }
  throw new Error(`ComfyUI did not answer within 5 min; see ${LOG}`);
}

function stop() {
  if (!existsSync(PIDFILE)) {
    console.log('no pidfile: this script started no ComfyUI; nothing stopped');
    return;
  }
  const { pid } = JSON.parse(readFileSync(PIDFILE, 'utf8'));
  const cmd = processCommandLine(pid);
  if (!cmd) {
    console.log(`pid ${pid} is gone; pidfile removed`);
  } else if (/main\.py/.test(cmd) && cmd.toLowerCase().includes(COMFY.toLowerCase().replace(/\//g, '\\'))) {
    const r = spawnSync('taskkill', ['/PID', String(pid), '/T', '/F'], { encoding: 'utf8', windowsHide: true });
    console.log(`${stamp()} stopped ComfyUI pid ${pid}: ${(r.stdout || r.stderr).trim()}`);
  } else {
    throw new Error(`pid ${pid} is not this ComfyUI any more (${cmd.slice(0, 120)}); not touching it`);
  }
  rmSync(PIDFILE, { force: true });
}

async function run(args) {
  const a = parseRunArgs(args);
  if (a.dryRun) {
    console.log(JSON.stringify(dryRun(a), null, 1));
    return;
  }
  const outdir = a.outdir;
  const before = await guard();
  const baseline = gpu();
  let peak = baseline.usedMiB;
  const timer = setInterval(() => {
    try {
      peak = Math.max(peak, gpu().usedMiB);
    } catch {}
  }, 2000);
  const t0 = Date.now();
  const code = await new Promise((res) => spawn(process.execPath, a.back ? [TRELLIS_MV, 'run', ...args] : [TRELLIS, 'run', ...args], { stdio: 'inherit' }).on('exit', res));
  clearInterval(timer);
  const record = {
    at: new Date(t0).toISOString(),
    secs: Math.round((Date.now() - t0) / 1000),
    exit: code,
    vramBaselineMiB: baseline.usedMiB,
    vramPeakMiB: peak,
    vramPeakDeltaMiB: peak - baseline.usedMiB,
    vramTotalMiB: baseline.totalMiB,
    guard: before,
    comfy: (await alive())?.system ?? null,
  };
  mkdirSync(outdir, { recursive: true });
  writeFileSync(join(outdir, 'gpu-run.json'), JSON.stringify(record, null, 1) + '\n');
  console.log(`${stamp()} trellis.mjs exit ${code} after ${record.secs} s; VRAM peak ${peak} MiB (baseline ${baseline.usedMiB}, +${record.vramPeakDeltaMiB})`);
  process.exitCode = code;
}

const [cmd, ...rest] = process.argv.slice(2);
try {
  if (cmd === 'start') await start();
  else if (cmd === 'stop') stop();
  else if (cmd === 'guard') await guard();
  else if (cmd === 'run') await run(rest);
  else if (cmd === 'free') spawnSync(process.execPath, [TRELLIS, 'free'], { stdio: 'inherit' });
  else if (cmd === 'status') console.log(JSON.stringify({ comfy: COMFY, pidfile: existsSync(PIDFILE) ? JSON.parse(readFileSync(PIDFILE, 'utf8')) : null, ...(await look()) }, null, 1));
  else {
    console.error('usage: comfy.mjs start | guard | run <image> <label> <seed> <outdir> [--back <image>] [--faces N] [--dry-run] | free | stop | status');
    process.exitCode = 2;
  }
} catch (e) {
  console.error(e.message);
  process.exitCode = 2;
}
