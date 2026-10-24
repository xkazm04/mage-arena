// Run an unchanged apps/vr/tools/assets script on the lane's Blender 4.2. Pure string work, no Blender here.
// Blender 4.2-4.x names EEVEE 'BLENDER_EEVEE_NEXT'; 4.1 and 5.x name it 'BLENDER_EEVEE' (render_evidence.py was written
// on Lynx's 5.2.2). A script that sets the old id dies on 4.2 with "enum BLENDER_EEVEE not found", and Blender still
// exits 0 unless --python-exit-code is given. adaptEevee rewrites each quoted 'BLENDER_EEVEE' to MAGEVR_EEVEE, which a
// one-line prelude sets from bpy.app.version, so the same copy runs on every version.

export const EEVEE_PRELUDE =
  "import bpy as _magevr_bpy; MAGEVR_EEVEE = 'BLENDER_EEVEE_NEXT' if (4, 2) <= _magevr_bpy.app.version < (5, 0) else 'BLENDER_EEVEE';  ";

/**
 * Returns { source, replaced }. The prelude goes in front of line 1 when line 1 is a comment (every line number, and so
 * every traceback, still points at the original file), otherwise on a line of its own. No id: the source is unchanged.
 */
export function adaptEevee(src) {
  let replaced = 0;
  const body = src.replace(/(['"])BLENDER_EEVEE\1/g, () => {
    replaced++;
    return 'MAGEVR_EEVEE';
  });
  if (!replaced) return { source: src, replaced };
  const source = body.startsWith('#') ? EEVEE_PRELUDE + body : `${EEVEE_PRELUDE.trimEnd()}\n${body}`;
  return { source, replaced };
}

/** The PNGs render_evidence.py writes for <name>; the attack clip is Cast for Brennic, Attack for the Bout 2 cast. */
export function evidenceFiles(name, attack = 'Cast') {
  return [`${name}_turnaround.png`, `${name}_game_640x480.png`, ...['Idle', attack, 'Hit', 'Death'].map((c) => `${name}_clip_${c}.png`)];
}
