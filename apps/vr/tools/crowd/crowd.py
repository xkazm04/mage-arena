# Crowd cards for the arena stands (A07 follow-up). Blender's bundled Python only (bpy, numpy, stdlib); no install, no network.
#
#   blender -b -P crowd.py -- build  <04-crowd-cards.jpg> <crowd dir> <kit3d dir> <work dir>   atlas, FBX, placement, manifest.json
#   blender -b -P crowd.py -- render <crowd dir> <kit3d dir> <layout json>                       crowd_lineup.png, stands_seated_640x480.png
#
# build cuts the four spectator figures off the model sheet (flood-fill the grey grid background from the border, split the
# figures by their empty columns, trim the stone bench off the seated pair), packs them in one power-of-two atlas, writes one
# quad card FBX per figure variant (pivot at the feet, +X forward, metres, 2 tris) and a placement set that fills the three
# treads of every straight and curved stand tier of the kit (kit3d/manifest.json, Unreal axes). Everything is deterministic:
# no clock, a fixed seed, our own PNG writer, and the FBX header timestamp pinned, so a re-run gives the same sha256 values.
import bpy, bmesh, sys, os, json, math, hashlib, struct, zlib, datetime, types
from collections import deque
import numpy as np
from mathutils import Vector

DATE = '2026-10-22'
SEED = 20261022
STAND_H, SEAT_H = 1.75, 1.30        # metres: the pointing figure's height, the calm seated figure's head above the feet
PAD = 8                             # atlas gutter in px, colour-bled so mips and filtering never pick up the grid grey
ATLAS_SIZES = [(512, 512), (1024, 512), (512, 1024), (1024, 1024), (2048, 1024), (1024, 2048), (2048, 2048)]
VARIANTS = ['SeatedCalm', 'SeatedLean', 'StandingCheer', 'StandingPoint']   # left to right on the sheet
SEATED = {'SeatedCalm', 'SeatedLean'}
BENCH_ROWS = (437, 451)             # sheet rows of the stone bench under the seated pair
MATERIAL = 'M_Crowd_Atlas'
PITCH = 0.55                        # m between spectators along a tread
EDGE_MARGIN = 0.30                  # m kept clear at each end of a tread
TREAD_TOPS = [3.50, 4.35, 5.20]; TREAD_D = 1.55; FRONT_HALF = 1.5; SPLAY = (1.958 - 1.5) / 4.65; CURVE_R = 8.65
CURVE_HALF = math.asin(1.189 / 8.65)  # the curved tier's half span in radians (front edge ends at y = 1.189)
# the share of each variant on a tread, front tread first: more sitting in front, more standing up the back
MIX = [[('SeatedCalm', 40), ('SeatedLean', 30), ('StandingCheer', 18), ('StandingPoint', 12)],
       [('SeatedCalm', 35), ('SeatedLean', 25), ('StandingCheer', 22), ('StandingPoint', 18)],
       [('SeatedCalm', 25), ('SeatedLean', 20), ('StandingCheer', 30), ('StandingPoint', 25)]]


def sha(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f: h.update(f.read())
    return h.hexdigest()


def write_png(path, rgba):
    # our own encoder: byte-identical on every run (Blender's writer is not guaranteed to be)
    h = rgba.shape[0]; raw = b''.join(b'\x00' + rgba[y].tobytes() for y in range(h))

    def chunk(t, d): return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', rgba.shape[1], h, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))


def load_sheet(path):
    im = bpy.data.images.load(path); w, h = im.size
    a = np.rint(np.array(im.pixels[:], dtype=np.float32).reshape(h, w, 4)[::-1, :, :3] * 255).astype(np.uint8)   # top-down RGB
    bpy.data.images.remove(im); return a


def dil(m):
    o = m.copy(); o[1:] |= m[:-1]; o[:-1] |= m[1:]; o[:, 1:] |= m[:, :-1]; o[:, :-1] |= m[:, 1:]; return o


def ero(m): return ~dil(~m)


def components(fg):
    h, w = fg.shape; flat = fg.ravel(); seen = np.zeros(h * w, bool); out = []
    for s in np.flatnonzero(flat):
        if seen[s]: continue
        q = deque([s]); seen[s] = True; px = []
        while q:
            p = q.popleft(); px.append(p); y, x = divmod(p, w)
            for n, ok in ((p - w, y > 0), (p + w, y < h - 1), (p - 1, x > 0), (p + 1, x < w - 1)):
                if ok and flat[n] and not seen[n]: seen[n] = True; q.append(n)
        out.append(np.array(px))
    return out


def cut_figures(rgb):
    # The sheet's background is a flat light grey (189) with a faint square grid. Model it: the grid's columns and rows are read
    # from the empty strips at the top and the left edge, so a background pixel is one within 5 of flat grey minus its column
    # and row line darkness. Hair and cloth highlights sit in the same grey range as the lines, but not on the model, so they stay.
    # The background is flooded in from the border (figure pixels that match it by chance stay in), closed pockets that match
    # it are dropped too, and the figure mask is eroded 1 px so no light halo survives.
    g = rgb.astype(np.int32).mean(axis=2); chroma = rgb.max(axis=2).astype(np.int32) - rgb.min(axis=2)
    col = np.median(g[:60, :], axis=0) - 189; row = np.median(g[:, :40], axis=1) - 189
    model = 189 + np.minimum(col[None, :], 0) + np.minimum(row[:, None], 0)
    bgl = (np.abs(g - model) <= 5) & (chroma < 10)
    flood = np.zeros_like(bgl); flood[0, :] = bgl[0, :]; flood[-1, :] = bgl[-1, :]; flood[:, 0] = bgl[:, 0]; flood[:, -1] = bgl[:, -1]
    while True:
        n = (dil(flood) & bgl) | flood
        if (n == flood).all(): break
        flood = n
    for comp in components(bgl & ~flood):
        if len(comp) >= 150: flood.ravel()[comp] = True
    fg = ero(~flood)
    keep = np.zeros_like(fg)
    for comp in components(fg):
        if len(comp) >= 3000: keep.ravel()[comp] = True
    cols = keep.any(axis=0); segs = []; x = 0
    while x < len(cols):
        if cols[x]:
            x0 = x
            while x < len(cols) and cols[x]: x += 1
            segs.append((x0, x))
        else: x += 1
    assert len(segs) == len(VARIANTS), f'expected {len(VARIANTS)} figures, found {len(segs)}: {segs}'
    return keep, segs


def trim_bench(mask, x0, x1):
    # The seated pair sit on a thin stone bench wider than the figure; the stands have their own treads, so the bench goes.
    # Sheet-specific: on 04-crowd-cards.jpg both benches run over rows BENCH_ROWS. Inside those rows only the span the figure
    # itself covers in the 6 rows above is kept (robe and cloak), plus 3 px.
    b0, b1 = BENCH_ROWS
    cols = np.flatnonzero(mask[b0 - 6:b0, x0:x1].any(axis=0)); lo, hi = x0 + int(cols[0]), x0 + int(cols[-1])
    out = mask.copy(); xs = np.arange(mask.shape[1])[None, :]
    band = np.zeros_like(mask); band[b0:b1, x0:x1] = True
    out &= ~(band & ((xs < lo - 3) | (xs > hi + 3)))
    return out, (b0, b1, lo, hi)


def bleed(rgb, a, iters):
    # push the figure's own colour outwards through transparent texels, so a bilinear or mip sample at the edge is figure colour
    rgb = rgb.copy(); filled = a.copy()
    for _ in range(iters):
        grow = dil(filled) & ~filled
        if not grow.any(): break
        acc = np.zeros(rgb.shape, np.float32); cnt = np.zeros(filled.shape, np.float32)
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            s = np.roll(np.roll(filled, dy, 0), dx, 1); c = np.roll(np.roll(rgb, dy, 0), dx, 1).astype(np.float32)
            acc += c * s[..., None]; cnt += s
        sel = grow & (cnt > 0); rgb[sel] = np.rint(acc[sel] / cnt[sel][:, None]).astype(np.uint8); filled = filled | sel
    return rgb


def pack(sizes):
    # shelf packing in the sheet's left-to-right order: wrap to a new shelf at the right edge; first power-of-two size that fits
    for W, H in ATLAS_SIZES:
        x = y = sh = 0; pos = []; ok = True
        for (w, h) in sizes:
            w2, h2 = w + 2 * PAD, h + 2 * PAD
            if x + w2 > W: x, y, sh = 0, y + sh, 0
            if w2 > W or y + h2 > H: ok = False; break
            pos.append((x + PAD, y + PAD)); x += w2; sh = max(sh, h2)
        if ok: return W, H, pos
    raise SystemExit('no atlas size up to 2048 fits the figures')


# ================================================================ placement
class Rng:   # mulberry32: the same sequence on every host
    def __init__(self, s): self.s = s & 0xffffffff

    def next(self):
        self.s = (self.s + 0x6D2B79F5) & 0xffffffff; t = self.s
        t = ((t ^ (t >> 15)) * (t | 1)) & 0xffffffff; t ^= (t + (((t ^ (t >> 7)) * (t | 61)) & 0xffffffff)) & 0xffffffff
        return ((t ^ (t >> 14)) & 0xffffffff) / 4294967296.0


def pick(rng, tread):
    tot = sum(w for _, w in MIX[tread]); r = rng.next() * tot
    for n, w in MIX[tread]:
        if r < w: return n
        r -= w
    return MIX[tread][-1][0]


def place(man):
    # Unreal axes, as the kit's assembly: locationM, yawDeg about +Z (+X toward +Y), scale. A card faces its local +X, which is
    # toward the arena centre for a tier. Each tier instance (kit TierStraight / TierCurved) is filled tread by tread, one row on
    # the middle of each tread, PITCH apart, EDGE_MARGIN clear at the ends; the tier's Y scale stretches the row with it.
    rng = Rng(SEED); out = []; per_tier = []
    for idx, it in enumerate(man['assembly']['instances']):
        if it['piece'] not in ('TierStraight', 'TierCurved'): continue
        px, py, pz = it['locationM']; yaw = math.radians(it['yawDeg']); sx, sy, sz = it['scale']; c, s = math.cos(yaw), math.sin(yaw); n0 = len(out)
        for t in range(3):
            d_mid = TREAD_D * t + TREAD_D / 2
            if it['piece'] == 'TierStraight':
                half = (FRONT_HALF + SPLAY * d_mid) * sy - EDGE_MARGIN; slots = [(-d_mid, ly, 0.0) for ly in row(half)]
            else:
                R = CURVE_R + d_mid; half_a = CURVE_HALF * sy - EDGE_MARGIN / R
                slots = [(CURVE_R - R * math.cos(a), R * math.sin(a) * sy, -a) for a in (row(half_a * R, R=R))]
            for k, (lx, ly, face) in enumerate(slots):
                v = pick(rng, t); jy = (rng.next() - 0.5) * 0.12; jyaw = (rng.next() - 0.5) * 12.0; js = 0.92 + rng.next() * 0.16
                lx2 = lx * sx - (0.06 if k % 2 else 0.0)          # alternate cards sit 6 cm back so neighbours never z-fight
                ly2 = ly + jy
                out.append({'variant': v, 'locationM': [round(px + c * lx2 - s * ly2, 4), round(py + s * lx2 + c * ly2, 4), round(pz + TREAD_TOPS[t], 4)],
                            'yawDeg': round(math.degrees(yaw + face) + jyaw, 3), 'scale': round(js, 3)})
        per_tier.append({'piece': it['piece'], 'instance': idx, 'cards': len(out) - n0})
    return out, per_tier


def row(half, R=None):
    # symmetric offsets about 0, PITCH apart, within +-half (metres along the row; angles in radians when R is given)
    n = int(math.floor(2 * half / PITCH)) + 1 if half > 0 else 0
    offs = [(i - (n - 1) / 2) * PITCH for i in range(n)]
    return [o / R for o in offs] if R else offs


# ================================================================ build
def pin_fbx_time():
    import io_scene_fbx.export_fbx_bin as eb

    class Fixed(datetime.datetime):
        @classmethod
        def now(cls, tz=None): return cls(2026, 10, 22, 0, 0, 0)
    eb.datetime = types.SimpleNamespace(datetime=Fixed)
    import io_scene_fbx.fbx_utils as fu   # the exporter derives object ids from hash(key), which Python randomises per process
    fu.hash = lambda key: int(hashlib.sha1(repr(key).encode()).hexdigest()[:15], 16)


def export_fbx(path, ob):
    bpy.ops.object.select_all(action='DESELECT'); ob.select_set(True); bpy.context.view_layer.objects.active = ob
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={'MESH'}, apply_scale_options='FBX_SCALE_ALL',
                             mesh_smooth_type='FACE', bake_anim=False, path_mode='STRIP', embed_textures=False, use_mesh_modifiers=False)


def mat_atlas():
    m = bpy.data.materials.new(MATERIAL); m.use_nodes = True
    pb = next(n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'); pb.inputs['Base Color'].default_value = (0.55, 0.55, 0.55, 1); pb.inputs['Roughness'].default_value = 1.0
    return m


def build(sheet, out, kit, work):
    os.makedirs(out, exist_ok=True); os.makedirs(os.path.join(out, 'export'), exist_ok=True); os.makedirs(work, exist_ok=True)
    rgb = load_sheet(sheet); keep, segs = cut_figures(rgb)
    cuts = []
    for name, (x0, x1) in zip(VARIANTS, segs):
        m = keep.copy(); m[:, :x0] = False; m[:, x1:] = False; note = None
        if name in SEATED: m, note = trim_bench(m, x0, x1)
        ys = np.flatnonzero(m.any(axis=1)); xs = np.flatnonzero(m.any(axis=0))
        box = (int(xs[0]), int(ys[0]), int(xs[-1]) + 1, int(ys[-1]) + 1)
        feet = np.flatnonzero(m[box[3] - 18:box[3]].any(axis=0)); feet_x = (int(feet[0]) + int(feet[-1])) / 2.0   # the feet's centre, a column of the sheet
        cuts.append({'name': name, 'mask': m, 'box': box, 'feetX': feet_x, 'bench': note})
    ppm = {'stand': (next(c for c in cuts if c['name'] == 'StandingPoint')['box'][3] - next(c for c in cuts if c['name'] == 'StandingPoint')['box'][1]) / STAND_H,
           'seat': (cuts[0]['box'][3] - cuts[0]['box'][1]) / SEAT_H}
    W, H, pos = pack([(c['box'][2] - c['box'][0], c['box'][3] - c['box'][1]) for c in cuts])
    atlas = np.zeros((H, W, 4), np.uint8); atl_a = np.zeros((H, W), bool)
    for c, (ax, ay) in zip(cuts, pos):
        x0, y0, x1, y1 = c['box']; w, h = x1 - x0, y1 - y0
        a = c['mask'][y0:y1, x0:x1]; atlas[ay:ay + h, ax:ax + w, :3] = rgb[y0:y1, x0:x1]; atl_a[ay:ay + h, ax:ax + w] = a; c['atlas'] = (ax, ay, w, h)
    atlas[..., :3] = bleed(atlas[..., :3], atl_a, PAD + 2); atlas[..., 3] = np.where(atl_a, 255, 0)
    # outside the figures and gutters the texels are not used; make them neutral grey so the file compresses well
    atlas[~atl_a & ~dil_n(atl_a, PAD + 2)] = (128, 128, 128, 0)
    png = os.path.join(out, 'crowd_atlas.png'); write_png(png, atlas)

    pin_fbx_time(); bpy.ops.wm.read_factory_settings(use_empty=True); mat = mat_atlas(); variants = {}
    for c in cuts:
        name = c['name']; k = ppm['seat' if name in SEATED else 'stand']; x0, y0, x1, y1 = c['box']; ax, ay, w, h = c['atlas']
        yl, yr = (x0 - c['feetX']) / k, (x1 - c['feetX']) / k; ht = (y1 - y0) / k        # Blender: +Y is the viewer's right looking down -X
        me = bpy.data.meshes.new('SM_Crowd_' + name); bm = bmesh.new()
        vs = [bm.verts.new((0, yl, 0)), bm.verts.new((0, yr, 0)), bm.verts.new((0, yr, ht)), bm.verts.new((0, yl, ht))]
        f = bm.faces.new(vs); uv = bm.loops.layers.uv.new('UVMap')
        u0, u1 = ax / W, (ax + w) / W; v0, v1 = 1 - (ay + h) / H, 1 - ay / H                  # v up from the bottom of the atlas
        for lp, (u, v) in zip(f.loops, ((u0, v0), (u1, v0), (u1, v1), (u0, v1))): lp[uv].uv = (u, v)
        bmesh.ops.triangulate(bm, faces=bm.faces); bm.normal_update(); bm.to_mesh(me); bm.free()
        assert all(p.normal.x > 0.99 for p in me.polygons)
        me.materials.append(mat); ob = bpy.data.objects.new('SM_Crowd_' + name, me); bpy.context.scene.collection.objects.link(ob)
        fp = os.path.join(out, 'export', f'SM_Crowd_{name}.fbx'); export_fbx(fp, ob)
        variants[name] = {'file': f'export/SM_Crowd_{name}.fbx', 'kind': 'seated' if name in SEATED else 'standing',
                          'uvRect': {'u0': round(u0, 6), 'v0': round(v0, 6), 'u1': round(u1, 6), 'v1': round(v1, 6)}, 'pxRect': [ax, ay, w, h],
                          'sheetBoxPx': [x0, y0, x1, y1], 'benchTrimPx': c['bench'],
                          'heightM': round(ht, 4), 'widthM': round(yr - yl, 4), 'tris': len(me.polygons), 'verts': len(me.vertices),
                          'boundsM': {'min': [0.0, round(yl, 4), 0.0], 'max': [0.0, round(yr, 4), round(ht, 4)]},
                          'pivot': 'base of the feet, centred between the feet', 'material': MATERIAL, 'sha256': sha(fp), 'bytes': os.path.getsize(fp)}
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(work, 'crowd.blend'))
    # re-open each FBX fresh and check it is what the manifest says
    for name, v in variants.items():
        bpy.ops.wm.read_factory_settings(use_empty=True); bpy.ops.import_scene.fbx(filepath=os.path.join(out, v['file']))
        ob = next(o for o in bpy.data.objects if o.type == 'MESH'); tris = sum(len(p.vertices) - 2 for p in ob.data.polygons); zs = [vv.co.z for vv in ob.data.vertices]
        assert tris == v['tris'] == 2 and abs(min(zs)) < 1e-6 and abs(max(zs) - v['heightM']) < 1e-3, (name, tris, min(zs), max(zs))

    man = json.load(open(os.path.join(kit, 'manifest.json'))); cards, per_tier = place(man)
    counts = {n: sum(1 for c in cards if c['variant'] == n) for n in VARIANTS}
    pl = {'card': 'A07 crowd', 'axes': man['assembly']['axes'] + '. A card faces its local +X (toward the arena centre); locationM is the pivot at the feet on the tread top.',
          'seed': SEED, 'pitchM': PITCH, 'edgeMarginM': EDGE_MARGIN, 'treadTopsM': TREAD_TOPS, 'tiersFilled': len(per_tier), 'perTier': per_tier,
          'cards': cards}
    plp = os.path.join(out, 'placement.json'); json.dump(pl, open(plp, 'w', newline='\n'), indent=1); open(plp, 'a', newline='\n').write('\n')
    prov = json.load(open(os.path.join(os.path.dirname(kit), 'manifest.json')))
    src = next(i for i in prov['items'] if i['asset'] == '04-crowd-cards')
    tris = sum(variants[c['variant']]['tris'] for c in cards)
    mf = {'card': 'A07 crowd cards', 'generated': DATE, 'tool': 'Blender 5.2.2 LTS (Windows ARM64), apps/vr/tools/crowd/crowd.py; image cut-out from the sheet, no AI step in this pass, 0 credits',
          'units': 'metres, unit scale; origin at the feet; +X forward (the card faces +X); Z up. Placement in Unreal axes as kit3d/manifest.json',
          'sheet': {'path': 'apps/vr/art/environment-kit/04-crowd-cards.jpg', 'sha256': sha(sheet), 'bytes': os.path.getsize(sheet), 'sizePx': [rgb.shape[1], rgb.shape[0]],
                    'provenance': {k: src[k] for k in ('tool', 'model', 'attempts', 'aspect_requested', 'aspect_delivered', 'regen_reasons', 'seconds')},
                    'record': 'apps/vr/art/environment-kit/manifest.json (item 04-crowd-cards) and prompts.json'},
          'scale': {'standingHeightM': STAND_H, 'seatedHeightM': SEAT_H, 'standingPxPerM': round(ppm['stand'], 3), 'seatedPxPerM': round(ppm['seat'], 3),
                    'rule': 'StandingPoint is 1.75 m head to foot and SeatedCalm 1.30 m head above the feet; each class has one scale, so StandingCheer (arms up) and SeatedLean follow from it'},
          'atlas': {'file': 'crowd_atlas.png', 'sizePx': [W, H], 'format': 'RGBA8, straight alpha, alpha 0 or 255 (mask, no blend)', 'gutterPx': PAD,
                    'sha256': sha(png), 'bytes': os.path.getsize(png)},
          'material': {'name': MATERIAL, 'note': 'one material for all four meshes: the atlas as base colour, alpha as opacity mask (clip 0.5), two-sided off, grey clay until the T16 style pick'},
          'variants': variants,
          'placement': {'file': 'placement.json', 'sha256': sha(plp), 'bytes': os.path.getsize(plp), 'cardCount': len(cards), 'totalTris': tris,
                        'perVariant': counts, 'tiersFilled': len(per_tier), 'drawCallEstimate': 4,
                        'note': 'two triangles per card; the kit assembly is 10,620 tris, so the crowd adds ' + f'{tris} tris'}}
    json.dump(mf, open(os.path.join(out, 'manifest.json'), 'w', newline='\n'), indent=1); open(os.path.join(out, 'manifest.json'), 'a', newline='\n').write('\n')
    print('BUILD atlas', W, H, 'cards', len(cards), 'tris', tris, counts, 'ppm', ppm, {n: v['heightM'] for n, v in variants.items()})


def dil_n(m, n):
    for _ in range(n): m = dil(m)
    return m


# ================================================================ render
def render(out, kit, layout):
    L = json.load(open(layout)); man = json.load(open(os.path.join(out, 'manifest.json'))); pl = json.load(open(os.path.join(out, 'placement.json')))
    kman = json.load(open(os.path.join(kit, 'manifest.json')))
    bpy.ops.wm.read_factory_settings(use_empty=True); s = bpy.context.scene
    s.render.engine = 'BLENDER_EEVEE'
    img = bpy.data.images.load(os.path.join(out, 'crowd_atlas.png')); img.alpha_mode = 'STRAIGHT'; img.colorspace_settings.name = 'sRGB'
    cm = bpy.data.materials.new(MATERIAL); cm.use_nodes = True; nt = cm.node_tree; pb = next(n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED')
    tx = nt.nodes.new('ShaderNodeTexImage'); tx.image = img; tx.interpolation = 'Linear'
    nt.links.new(tx.outputs['Color'], pb.inputs['Base Color']); nt.links.new(tx.outputs['Alpha'], pb.inputs['Alpha']); pb.inputs['Roughness'].default_value = 1.0
    cm.surface_render_method = 'DITHERED'
    cards = {}
    for n in VARIANTS:
        before = set(bpy.data.objects); bpy.ops.import_scene.fbx(filepath=os.path.join(out, man['variants'][n]['file']))
        ob = next(o for o in set(bpy.data.objects) - before if o.type == 'MESH'); ob.data.materials.clear(); ob.data.materials.append(cm); cards[n] = ob
    src = {}
    for name in ('TierStraight', 'TierCurved', 'TierStair', 'WallStraight', 'WallCurved', 'GateArch', 'Dais', 'RunePad', 'Seat', 'Brazier', 'Banner'):
        before = set(bpy.data.objects); bpy.ops.import_scene.fbx(filepath=os.path.join(kit, 'export', f'SM_Arena_{name}.fbx'))
        src[name] = next(o for o in set(bpy.data.objects) - before if o.type == 'MESH')
    for ob in src.values():
        for sl in ob.material_slots: sl.material = bpy.data.materials[sl.material.name.split('.')[0]]
    for m in bpy.data.materials:
        if m.node_tree and m.name != MATERIAL:
            pbn = next((n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None)
            if pbn: pbn.inputs['Roughness'].default_value = 0.9; pbn.inputs['Specular IOR Level'].default_value = 0.2
    cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam')); s.collection.objects.link(cam); s.camera = cam
    s.render.image_settings.file_format = 'PNG'; s.render.resolution_percentage = 100
    w_ = bpy.data.worlds.new('w'); s.world = w_; w_.use_nodes = True; bg = w_.node_tree.nodes['Background']
    suns = []

    def sun(d, e, col=(1, 1, 1)):
        Ls = bpy.data.objects.new('l', bpy.data.lights.new('l', 'SUN')); Ls.data.energy = e; Ls.data.color = col; s.collection.objects.link(Ls)
        Ls.rotation_euler = (-Vector(d)).to_track_quat('-Z', 'Y').to_euler(); suns.append(Ls)

    def aim(loc, target): cam.location = loc; cam.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()

    for o in src.values(): o.hide_render = True
    # -- lineup: the four cards side by side, front view, with a 1 m tick column on each side for scale
    bg.inputs[0].default_value = (0.12, 0.13, 0.16, 1); sun((1.0, 0.25, 0.5), 3.0); sun((0.4, -0.6, 0.7), 1.2)
    x = 0.0
    for n in VARIANTS:
        v = man['variants'][n]; cards[n].location = (0, x - v['boundsM']['min'][1], 0); x += v['widthM'] + 0.35
    mk = bpy.data.materials.new('mk'); mk.use_nodes = True; mk.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (0.8, 0.8, 0.8, 1)
    for i in range(3):   # 0, 1 and 2 m ticks along the left
        t = bpy.data.objects.new('tick', bpy.data.meshes.new('tick')); bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0); bm.to_mesh(t.data); bm.free()
        t.scale = (0.02, 0.5, 0.02); t.location = (0, -0.6, i + 0.01); t.data.materials.append(mk); s.collection.objects.link(t)
    cam.data.type = 'ORTHO'; cam.data.ortho_scale = x + 0.1; s.render.resolution_x, s.render.resolution_y = 1600, 800
    cy = (x - 0.35) / 2; cam.location = (10, cy, 1.0); cam.rotation_euler = (math.radians(90), 0, math.radians(90))
    s.render.filepath = os.path.join(out, 'crowd_lineup.png'); bpy.ops.render.render(write_still=True)
    for o in list(s.objects):
        if o.name.startswith('tick'): bpy.data.objects.remove(o)
    for l_ in suns: bpy.data.objects.remove(l_)
    suns.clear()
    # -- the stands from the seated eye (the kit's arena_seated render, plus the crowd)
    lt = L['lighting']; bg.inputs[0].default_value = tuple(0.5 * h + 0.5 * k for h, k in zip(lt['skyHorizon'], lt['skyLightColour'])) + (1,)
    sun((-0.9, 0.2, 0.45), 2.2, tuple(lt['directionalColour'])); sun((0.2, 0.3, 1.0), 1.4, tuple(lt['skyLightColour']))
    kit_floor(L, s)
    for it in kman['assembly']['instances']:
        o = bpy.data.objects.new(it['piece'], src[it['piece']].data); s.collection.objects.link(o)
        x_, y_, z_ = it['locationM']; o.location = (x_, -y_, z_); o.rotation_euler = (0, 0, -math.radians(it['yawDeg'])); o.scale = it['scale']
    for c in pl['cards']:
        o = bpy.data.objects.new(c['variant'], cards[c['variant']].data); s.collection.objects.link(o)
        x_, y_, z_ = c['locationM']; o.location = (x_, -y_, z_); o.rotation_euler = (0, 0, -math.radians(c['yawDeg'])); o.scale = (c['scale'],) * 3
    for o in cards.values(): o.hide_render = True
    centre = next(p for p in L['pads']['items'] if p['id'] == 'centre')['positionM']
    eye = Vector((centre[0], -centre[1], centre[2] + L['seatedEyeHeightAbovePadM'])); tgt = L['lookTargetM']
    cam.data.type = 'PERSP'; cam.data.sensor_fit = 'HORIZONTAL'; cam.data.angle = math.radians(L['cameraFovDeg']); cam.data.clip_end = 200
    aim(eye, (tgt[0], -tgt[1], tgt[2]))
    s.render.resolution_x, s.render.resolution_y = 640, 480; s.render.filepath = os.path.join(out, 'stands_seated_640x480.png'); bpy.ops.render.render(write_still=True)
    print('RENDER eye', [round(v, 3) for v in eye], 'cards', len(pl['cards']))


def kit_floor(L, s):
    FLOOR_SEGS = 64
    A_, B_ = L['floor']['radiusXM'], L['floor']['radiusYM']
    fl = bpy.data.meshes.new('floor'); fo = bpy.data.objects.new('SandFloorStandIn', fl); s.collection.objects.link(fo)
    bm = bmesh.new(); top = [bm.verts.new((A_ * math.cos(2 * math.pi * i / FLOOR_SEGS), B_ * math.sin(2 * math.pi * i / FLOOR_SEGS), 0)) for i in range(FLOOR_SEGS)]
    bm.faces.new(top); bm.to_mesh(fl); bm.free(); fl.materials.append(bpy.data.materials['M_Arena_Sand'])


if __name__ == '__main__':
    a = sys.argv[sys.argv.index('--') + 1:]; a = a[:1] + [os.path.abspath(x) for x in a[1:]]
    if a[0] == 'build': build(*a[1:5])
    elif a[0] == 'render': render(*a[1:4])
    else: raise SystemExit(__doc__)
