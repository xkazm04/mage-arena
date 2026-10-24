"""Deterministic measurements for reconstruction-input images (image-to-3d-input-gating: compute what can be computed).

    python apps/vr/tools/refs/measure.py <image> [<image> ...]      one JSON object per image on stdout

It needs Pillow and nothing else. It measures, it does not judge: the gate verdicts are written by a separate observer
and bound to the sha256 printed here. Per image:
- sha256, width, height;
- bg: the background colour (median of an 8 px border) and border_plain, the share of border pixels within 12 of it;
- bbox: the subject box (pixels more than 40 away from bg in summed RGB, rows and columns with at least 0.4 % of
  such pixels), its height share of the image, the top and bottom margins, and whether it touches an image edge;
- shadow_below: the share of pixels in the band just under the subject box (4 % of the image height, the box's
  width) that are 6-40 darker than bg: a soft contact or cast shadow shows here;
- element_share: the share of subject pixels in the reserved element colours (turquoise around #19E0E0, vermilion
  around #FF3B1F, both saturated and bright), which a non-magic subject should not carry.
"""
import colorsys
import hashlib
import json
import sys

from PIL import Image


def median(values):
    s = sorted(values)
    return s[len(s) // 2]


def measure(path):
    data = open(path, "rb").read()
    img = Image.open(path).convert("RGB")
    w, h = img.size
    px = img.load()
    border = [px[x, y] for x in range(w) for y in list(range(8)) + list(range(h - 8, h))]
    border += [px[x, y] for y in range(8, h - 8) for x in list(range(8)) + list(range(w - 8, w))]
    bg = tuple(median([p[i] for p in border]) for i in range(3))
    dist = lambda p: abs(p[0] - bg[0]) + abs(p[1] - bg[1]) + abs(p[2] - bg[2])
    plain = sum(1 for p in border if dist(p) <= 12) / len(border)

    fg = [[dist(px[x, y]) > 40 for x in range(w)] for y in range(h)]
    rows = [y for y in range(h) if sum(fg[y]) >= 0.004 * w]
    cols = [x for x in range(w) if sum(fg[y][x] for y in range(h)) >= 0.004 * h]
    if not rows or not cols:
        return {"file": path, "sha256": hashlib.sha256(data).hexdigest(), "width": w, "height": h, "bbox": None}
    top, bottom, left, right = rows[0], rows[-1], cols[0], cols[-1]

    band = range(bottom + 1, min(h, bottom + 1 + int(0.04 * h)))
    band_px = [px[x, y] for y in band for x in range(left, right + 1)]
    darker = lambda p: 6 <= (sum(bg) - sum(p)) <= 40
    shadow = sum(1 for p in band_px if darker(p)) / len(band_px) if band_px else 0.0

    subject = elem = 0
    for y in range(top, bottom + 1):
        for x in range(left, right + 1):
            if not fg[y][x]:
                continue
            subject += 1
            r, g, b = px[x, y]
            hh, ss, vv = colorsys.rgb_to_hsv(r / 255, g / 255, b / 255)
            deg = hh * 360
            if (165 <= deg <= 195 and ss > 0.5 and vv > 0.55) or ((deg <= 15 or deg >= 350) and ss > 0.75 and vv > 0.8):
                elem += 1
    return {
        "file": path.replace("\\", "/"),
        "sha256": hashlib.sha256(data).hexdigest(),
        "width": w,
        "height": h,
        "bg": {"rgb": bg, "border_plain": round(plain, 4)},
        "bbox": {
            "top": top, "bottom": bottom, "left": left, "right": right,
            "height_share": round((bottom - top + 1) / h, 4), "width_px": right - left + 1,
            "margin_top": round(top / h, 4), "margin_bottom": round((h - 1 - bottom) / h, 4),
            "centre_x_share": round((left + right) / 2 / w, 4),
            "touches_edge": top <= 1 or left <= 1 or bottom >= h - 2 or right >= w - 2,
        },
        "shadow_below": round(shadow, 4),
        "element_share": round(elem / subject, 5) if subject else 0.0,
    }


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    for p in sys.argv[1:]:
        print(json.dumps(measure(p)))
