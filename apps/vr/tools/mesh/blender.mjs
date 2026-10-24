// Run one Blender script headless and read the JSON it wrote. Node built-ins only.
// Blender: the BLENDER environment variable, else the 4.2 LTS install on the x64 host (the lane's proven version).
import { spawnSync } from 'node:child_process';
import { existsSync, mkdtempSync, readFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

export const HERE = dirname(fileURLToPath(import.meta.url));
export const DEFAULT_BLENDER = 'C:/Program Files/Blender Foundation/Blender 4.2/blender.exe';

export function blenderPath() {
  const b = process.env.BLENDER || DEFAULT_BLENDER;
  if (!existsSync(b)) throw new Error(`Blender not found at ${b}; set BLENDER to blender.exe`);
  return b;
}

/** Run `script` (a path) with `args` after `--`; returns { stdout, secs }. Throws with the tail of the log on failure. */
export function runBlender(script, args, { blend = null, timeoutMs = 60 * 60 * 1000 } = {}) {
  const pre = blend ? ['-b', blend] : ['-b', '--factory-startup'];
  const t0 = Date.now();
  const r = spawnSync(blenderPath(), [...pre, '--python-exit-code', '1', '-P', script, '--', ...args], {
    encoding: 'utf8',
    maxBuffer: 256 * 1024 * 1024,
    timeout: timeoutMs,
    windowsHide: true,
  });
  const secs = Number(((Date.now() - t0) / 1000).toFixed(1));
  if (r.status !== 0) {
    const log = `${r.stdout ?? ''}\n${r.stderr ?? ''}`.trim().split('\n').slice(-30).join('\n');
    throw new Error(`blender ${script} exited ${r.status ?? r.signal ?? r.error}:\n${log}`);
  }
  return { stdout: r.stdout, secs };
}

/** Run an extractor that writes one JSON file and return the parsed numbers plus the seconds it took. */
export function extract(script, input) {
  const dir = mkdtempSync(join(tmpdir(), 'magevr-mesh-'));
  try {
    const out = join(dir, 'out.json');
    const { secs } = runBlender(join(HERE, script), [input, out]);
    return { data: JSON.parse(readFileSync(out, 'utf8')), secs };
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
}
