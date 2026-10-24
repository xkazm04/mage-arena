#!/usr/bin/env node
// The T38 evidence renders of a rigged FBX on the lane's Blender 4.2: apps/vr/tools/assets/render_evidence.py, unchanged
// on disk, run from a temp copy whose EEVEE engine id fits this Blender (blender-compat.mjs). Fails when Blender fails
// (--python-exit-code 1) or when a PNG it should have written is missing.
//
//   node apps/vr/tools/mesh/evidence.mjs <in.fbx> <outdir> <name> [<name>_rig.json] [<attack clip, default Cast>]
//        [<framing: Humanoid (default) | CinderHound | MireMaw>]
//
// Exit: 0 every PNG written; 1 a PNG is missing; 2 usage or a Blender error.
import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';
import { adaptEevee, evidenceFiles } from './blender-compat.mjs';
import { HERE, runBlender } from './blender.mjs';

const SCRIPT = resolve(HERE, '..', 'assets', 'render_evidence.py');

const args = process.argv.slice(2);
const [fbx, outdir, name, rig, attack = 'Cast'] = args;
if (!fbx || !outdir || !name) {
  console.error('usage: evidence.mjs <in.fbx> <outdir> <name> [<name>_rig.json] [<attack clip>] [<framing>]');
  process.exit(2);
}

const dir = mkdtempSync(join(tmpdir(), 'magevr-evidence-'));
try {
  const { source, replaced } = adaptEevee(readFileSync(SCRIPT, 'utf8'));
  const copy = join(dir, 'render_evidence.py');
  writeFileSync(copy, source);
  mkdirSync(resolve(outdir), { recursive: true });
  const pass = [resolve(fbx), resolve(outdir), name, ...(rig ? [resolve(rig)] : []), ...args.slice(4)];
  const { secs } = runBlender(copy, pass);
  const files = evidenceFiles(name, attack).map((f) => ({ file: f, written: existsSync(join(resolve(outdir), f)) }));
  const missing = files.filter((f) => !f.written).map((f) => f.file);
  console.log(JSON.stringify({ script: SCRIPT, eeveeIdsAdapted: replaced, secs, files: files.map((f) => f.file), missing }));
  process.exitCode = missing.length ? 1 : 0;
} catch (e) {
  console.error(e.message);
  process.exitCode = 2;
} finally {
  rmSync(dir, { recursive: true, force: true });
}
