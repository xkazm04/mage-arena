// The finishing parameters finish.mjs passes to ../assets/finish.py, apart so they can be tested without Blender
// (finish-args.test.mjs). finish.py's own checks (../mesh/finish_lib.py parse_options) stay the authority; these fail
// early with the same rules so a bad flag never costs a Blender start.
//
// mode flat (default): the T38 recipe, one flat palette colour per material. Its argument list is exactly the one
// finish.mjs built before the bake mode existed: 7 positionals and the palette set, no options.
// mode bake: the TRELLIS albedo baked onto LOD0 as its texture, at budgets.finish.bakeTextureRes unless --bake says
// otherwise; --clamp none|posterize|hue|both (with --strength, --levels) pulls the texture toward the palette;
// --cage, --ray, --margin tune the bake.

export const MODES = ['flat', 'bake'];
export const CLAMPS = ['none', 'posterize', 'hue', 'both'];
const BAKE_OPTIONS = ['clamp', 'strength', 'levels', 'cage', 'ray', 'margin'];

/** opt(name, default) reads a --flag; budgets is budgets.json; cls its class. Throws on a bad mode or clamp. */
export function finishParams(opt, budgets, cls) {
  const f = budgets.finish;
  const mode = opt('mode', 'flat');
  if (!MODES.includes(mode)) throw new Error(`--mode ${mode}: one of ${MODES.join(', ')}`);
  const clamp = opt('clamp', 'none');
  if (!CLAMPS.includes(clamp)) throw new Error(`--clamp ${clamp}: one of ${CLAMPS.join(', ')}`);
  const extra = Object.fromEntries(BAKE_OPTIONS.map((k) => [k, opt(k)]).filter(([k, v]) => v !== undefined && !(k === 'clamp' && v === 'none')));
  if (mode === 'flat' && Object.keys(extra).length) throw new Error(`--${Object.keys(extra).join(', --')} need --mode bake`);
  const bakeDefault = mode === 'bake' ? f.bakeTextureRes : f.bakeRes;
  return {
    palette: opt('palette', cls.palette),
    rotZDeg: Number(opt('rot', f.rotZDeg)),
    bakeRes: Number(opt('bake', bakeDefault)),
    voxelM: Number(opt('voxel', f.voxelM)),
    trisTarget: cls.tris.target,
    mode,
    options: extra,
  };
}

/** The argument list after `--` for finish.py. */
export function finishPyArgs(raw, outdir, name, p) {
  const args = [raw, outdir, name, String(p.rotZDeg), String(p.bakeRes), String(p.voxelM), String(p.trisTarget), p.palette];
  if (p.mode === 'flat') return args;
  return [...args, 'mode=bake', ...Object.entries(p.options).map(([k, v]) => `${k}=${v}`)];
}
