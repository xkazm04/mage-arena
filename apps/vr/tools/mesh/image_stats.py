# Input gate, extraction half: measure one source image and write the numbers as JSON. It decides nothing; the
# verdict comes from gate-lib.mjs (input-gate.mjs runs both). Blender is the decoder because the repo's Node tooling
# is built-ins only and has no JPEG decoder; any Blender 4.2+ runs this headless.
#   blender -b --factory-startup -P image_stats.py -- <image> <out.json>
# Measured on a copy shrunk to at most MAX_SIDE px (block mean):
#   background  the border band (BAND of the short side): median colour, share of border pixels near it, the
#               spread of the four sides' medians (a gradient or a vignette). A transparent border is the background
#               itself, and alpha then makes the mask.
#   foreground  pixels farther than FG_TOL from the background colour (or alpha >= 0.5), closed by one 3x3 dilation,
#               split into 8-connected components: area, share and normalised bbox (y down from the top).
import bpy, sys, os, json, math, numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import components  # noqa: E402

MAX_SIDE = 512
BAND = 0.02
BG_TOL = 0.06  # sRGB 0-1, Euclidean: JPEG noise on a flat field stays well under this
FG_TOL = 0.10
KEEP = 32

a = sys.argv[sys.argv.index('--') + 1:]
src, outp = a[:2]
img = bpy.data.images.load(os.path.abspath(src))
W, H = img.size
ch = img.channels
px = np.empty(W * H * 4, dtype=np.float32)
img.pixels.foreach_get(px)  # byte images give their stored (sRGB) values, rows bottom-up
px = px.reshape(H, W, 4)[::-1]

f = max(1, math.ceil(max(W, H) / MAX_SIDE))
h, w = H // f, W // f
small = px[:h * f, :w * f].reshape(h, f, w, f, 4).mean(axis=(1, 3))
rgb, alpha = small[..., :3], small[..., 3]

band = max(2, round(BAND * min(h, w)))
edge = np.zeros((h, w), dtype=bool)
edge[:band] = edge[-band:] = True
edge[:, :band] = edge[:, -band:] = True
transparent = ch == 4 and float((alpha[edge] < 0.5).mean()) > 0.9

sides = {'top': rgb[:band], 'bottom': rgb[-band:], 'left': rgb[:, :band], 'right': rgb[:, -band:]}
side_med = {k: np.median(v.reshape(-1, 3), axis=0) for k, v in sides.items()}
names = list(side_med)
side_spread = max(float(np.linalg.norm(side_med[x] - side_med[y])) for i, x in enumerate(names) for y in names[i + 1:])
bg = np.median(rgb[edge], axis=0)
dist = np.linalg.norm(rgb - bg, axis=2)
background = {
    'mode': 'transparent' if transparent else 'opaque',
    'srgb': [round(float(x), 4) for x in bg],
    'luminance': round(float(0.2126 * bg[0] + 0.7152 * bg[1] + 0.0722 * bg[2]), 4),
    'borderNearShare': round(float((dist[edge] <= BG_TOL).mean()), 4),
    'borderP95': round(float(np.percentile(dist[edge], 95)), 4),
    'sideMediansSrgb': {k: [round(float(x), 4) for x in v] for k, v in side_med.items()},
    'sideSpread': round(side_spread, 4),
}

mask = alpha >= 0.5 if transparent else dist > FG_TOL
grown = mask.copy()
for dy in (-1, 0, 1):
    for dx in (-1, 0, 1):
        grown |= np.roll(np.roll(mask, dy, 0), dx, 1)
grown[0] |= mask[0]; grown[-1] |= mask[-1]  # np.roll wraps; the frame edge must not leak into the far side
grown[:, 0] |= mask[:, 0]; grown[:, -1] |= mask[:, -1]
ys, xs = np.nonzero(grown)
idx = -np.ones((h, w), dtype=np.int64)
idx[ys, xs] = np.arange(len(ys))
ea, eb = [], []
for dy, dx in ((0, 1), (1, 0), (1, 1), (1, -1)):
    y2, x2 = ys + dy, xs + dx
    ok = (y2 < h) & (x2 >= 0) & (x2 < w)
    j = idx[y2[ok], x2[ok]]
    keep = j >= 0
    ea.append(idx[ys[ok], xs[ok]][keep]); eb.append(j[keep])
lab = components.labels(len(ys), np.concatenate(ea) if ea else [], np.concatenate(eb) if eb else [])
fg_px = int(mask.sum())
comps = []
ranked = components.sizes(lab, mask[ys, xs].astype(np.float64)) if len(ys) else []  # area: the mask, not the dilation
if ranked:
    for label, area in ranked[:KEEP]:
        sel = lab == label
        y, x = ys[sel], xs[sel]
        comps.append({'area': int(area), 'share': round(area / max(1, fg_px), 5),
                      'bbox': [round(float(x.min()) / w, 4), round(float(y.min()) / h, 4),
                               round(float(x.max() + 1) / w, 4), round(float(y.max() + 1) / h, 4)]})
R = {
    'image': {'path': os.path.abspath(src), 'width': W, 'height': H, 'channels': ch, 'bytes': os.path.getsize(src)},
    'sample': {'factor': f, 'width': w, 'height': h, 'band': band},
    'params': {'maxSide': MAX_SIDE, 'band': BAND, 'bgTol': BG_TOL, 'fgTol': FG_TOL},
    'background': background,
    'foreground': {'share': round(fg_px / (h * w), 5), 'components': comps, 'componentsOmitted': max(0, len(ranked) - KEEP)},
}
json.dump(R, open(outp, 'w'), indent=1)
print('IMAGE_STATS', json.dumps({'size': [W, H], 'bg': background['srgb'], 'fgShare': R['foreground']['share'], 'components': len(ranked)}))
