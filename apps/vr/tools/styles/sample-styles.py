"""T16: samples the three shortlisted style skins (13, 25, 30) from their style-study images.

Usage (from the repo root): python apps/vr/tools/styles/sample-styles.py [--sheet DIR]

Writes apps/vr/data/vr/styles/style-<id>.json. Every colour is the per-channel median of the pixels inside one box of one
study image (optionally only the pixels that pass a saturation / value filter), converted from sRGB to linear. The box,
filter and pixel count go into the file's `_source`, so a reader can re-check any colour by eye. With --sheet, it also
writes a contact sheet per style (the crop of each box next to its swatch) for review; the sheets are not committed.

Before writing, the threat language is checked (DECISIONS 2026-10-02, PROJECT-PLAN 6.3): every magic colour is saturated
(absorb), steel is unsaturated (dodge), the unblockable body is near black and its rim is red (leave), and no magic colour
sits near steel, the rim or another school. A failure exits 1 and writes nothing.

Needs Pillow. Deterministic: same images, same output bytes.
"""

import colorsys
import json
import os
import re
import sys

from PIL import Image, ImageDraw

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "..", ".."))
STUDIES = os.path.join(ROOT, "apps", "vr", "art", "style-studies")
OUT = os.path.join(ROOT, "apps", "vr", "data", "vr", "styles")

# The greybox values (ElementColour.cpp FElementPalette::Greybox, arena-layout.json colours) a skin keeps where its
# study has nothing to sample, linear RGB.
GREYBOX_AIR = [0.20, 0.85, 0.22]
GREYBOX_CAST_FLASH = [0.12, 0.82, 0.2]
GREYBOX_REJECT_FLASH = [0.5, 0.5, 0.5]

# key: (image, (x0, y0, x1, y1), filter). filter keys: smin, smax, vmin, vmax (HSV 0..1 on sRGB), hue (centre deg, half
# width deg). A string value instead of a tuple means "not sampled": [greybox value, reason].
STYLES = {
    "13": {
        "name": "Screen-Print Poster",
        "dir": "13-screen-print",
        "kept": "Flat two-to-four ink poster colours on cream paper, a dark ink outline, ochre sand under navy shadow; "
        "the halftone and misregistration are texture work for later, the greybox keeps flat inks.",
        "edge": {"treatment": "ink-outline", "widthPx": 2.0, "note": "Thick dark ink line around every figure and "
                 "threat (07 threat language, 08 enemy lineup); no halftone in the greybox."},
        "colours": {
            "projectiles.magic": ("07-threat-language.jpg", (120, 350, 210, 430), {"smin": 0.7, "vmin": 0.75}),
            "projectiles.fire": ("07-threat-language.jpg", (330, 180, 480, 480), {"smin": 0.75, "vmin": 0.85, "hue": (32, 8)}),
            "projectiles.earth": ("07-threat-language.jpg", (540, 260, 620, 420), {"smin": 0.35}),
            "projectiles.steel": ("07-threat-language.jpg", (975, 180, 1025, 240), {"smax": 0.25, "vmax": 0.6}),
            "projectiles.unblockableBody": ("07-threat-language.jpg", (1080, 360, 1160, 440), {"vmax": 0.3}),
            "projectiles.unblockableRim": ("07-threat-language.jpg", (990, 270, 1250, 530), {"smin": 0.8, "vmin": 0.6, "hue": (358, 6)}),
            "arena.sand": ("05-arena-wide.jpg", (420, 470, 760, 530), {"smin": 0.3}),
            "arena.stone": ("05-arena-wide.jpg", (30, 380, 200, 410), {}),
            "arena.dais": ("01-vr-duel.jpg", (40, 600, 160, 680), {}),
            "arena.padRing": ("01-vr-duel.jpg", (570, 330, 760, 390), {"smin": 0.6, "vmin": 0.7}),
            "arena.seat": ("05-arena-wide.jpg", (260, 230, 560, 260), {}),
            "arena.marker": ("08-enemy-lineup.jpg", (40, 600, 240, 690), {"vmax": 0.25}),
            "arena.brazier": ("05-arena-wide.jpg", (980, 630, 1240, 690), {}),
            "arena.flame": ("01-vr-duel.jpg", (630, 195, 720, 250), {"smin": 0.6, "vmin": 0.8}),
            "arena.bannerCloth": ("01-vr-duel.jpg", (55, 140, 90, 220), {"smin": 0.4}),
            "arena.bannerPole": ("01-vr-duel.jpg", (40, 100, 60, 260), {"vmax": 0.35}),
            "arena.wall": ("05-arena-wide.jpg", (30, 430, 200, 520), {"smin": 0.4}),
            "arena.hand": ("01-vr-duel.jpg", (420, 520, 480, 580), {}),
            "arena.castFlash": [GREYBOX_CAST_FLASH, "greybox kept: the recognizer's cast flash is feedback, not in the study"],
            "arena.rejectFlash": [GREYBOX_REJECT_FLASH, "greybox kept: the recognizer's reject flash is feedback, not in the study"],
            "arena.wardArc": ("01-vr-duel.jpg", (700, 440, 840, 520), {"smin": 0.5, "vmin": 0.7}),
            "sky.zenith": ("01-vr-duel.jpg", (20, 15, 160, 60), {}),
            "sky.horizon": ("01-vr-duel.jpg", (560, 90, 760, 120), {}),
            "figures.soldier": ("08-enemy-lineup.jpg", (230, 280, 330, 450), {"smin": 0.5, "hue": (8, 20)}),
            "figures.slinger": ("08-enemy-lineup.jpg", (410, 300, 520, 420), {"smin": 0.15, "vmax": 0.6}),
            "figures.hound": ("08-enemy-lineup.jpg", (620, 340, 840, 520), {"vmax": 0.25}),
            "figures.mireMaw": ("08-enemy-lineup.jpg", (960, 340, 1180, 500), {"smin": 0.25, "vmax": 0.55}),
            "figures.fireMage": ("01-vr-duel.jpg", (770, 230, 850, 330), {"smin": 0.5}),
            "figures.outline": ("07-threat-language.jpg", (40, 560, 200, 640), {"vmax": 0.3}),
            "cuff.band": ("06-cuff-hud.jpg", (420, 760, 520, 820), {"smin": 0.3}),
            "cuff.glyph": ("06-cuff-hud.jpg", (270, 630, 450, 760), {"smin": 0.5, "vmin": 0.5, "hue": (180, 20)}),
            "projectiles.air": [GREYBOX_AIR, "greybox kept: the study draws air as a pale grey crescent, which would read "
                                "as steel (dodge); air keeps its greybox green so it stays an absorb colour"],
        },
    },
    "25": {
        "name": "Moonlit Silver Nocturne",
        "dir": "25-moonlit-silver",
        "kept": "Silver-blue night: moonlit pale sand and stone, navy sky, figures in cold grey; the only warm and "
        "saturated colours are the threats (turquoise water, ember fire) so they glow out of a grey scene.",
        "edge": {"treatment": "soft-rim", "widthPx": 1.0, "note": "No ink line; a thin pale rim like moonlight on an "
                 "edge (04 turnaround, 08 lineup). The rim is a material tint, never a glow or bloom (unlit)."},
        "colours": {
            "projectiles.magic": ("07-threat-language.jpg", (90, 330, 190, 410), {"smin": 0.4, "vmin": 0.5}),
            "projectiles.fire": ("07-threat-language.jpg", (260, 170, 460, 530), {"smin": 0.75, "vmin": 0.85, "hue": (34, 8)}),
            "projectiles.earth": ("07-threat-language.jpg", (480, 240, 610, 490), {"smin": 0.45, "vmin": 0.7}),
            "projectiles.steel": ("07-threat-language.jpg", (830, 160, 1050, 540), {"smax": 0.2, "vmin": 0.3, "vmax": 0.8}),
            "projectiles.unblockableBody": ("07-threat-language.jpg", (1100, 340, 1160, 390), {"vmax": 0.2}),
            "projectiles.unblockableRim": ("07-threat-language.jpg", (1040, 280, 1240, 460), {"smin": 0.8, "vmin": 0.6, "hue": (358, 6)}),
            "arena.sand": ("05-arena-wide.jpg", (300, 460, 980, 510), {}),
            "arena.stone": ("05-arena-wide.jpg", (60, 360, 240, 420), {}),
            "arena.dais": ("05-arena-wide.jpg", (380, 680, 900, 715), {"smax": 0.25}),
            "arena.padRing": ("05-arena-wide.jpg", (430, 560, 850, 690), {"smin": 0.5, "vmin": 0.6}),
            "arena.seat": ("05-arena-wide.jpg", (330, 240, 450, 290), {}),
            "arena.marker": ("01-vr-duel.jpg", (420, 230, 600, 245), {"vmax": 0.35}),
            "arena.brazier": ("05-arena-wide.jpg", (575, 438, 705, 452), {"vmax": 0.3}),
            "arena.flame": ("05-arena-wide.jpg", (590, 395, 690, 440), {"smin": 0.6, "vmin": 0.8}),
            "arena.bannerCloth": ("05-arena-wide.jpg", (340, 100, 360, 200), {}),
            "arena.bannerPole": ("01-vr-duel.jpg", (225, 55, 235, 70), {}),
            "arena.wall": ("01-vr-duel.jpg", (160, 260, 320, 320), {}),
            "arena.hand": ("06-cuff-hud.jpg", (380, 200, 440, 300), {"smax": 0.3}),
            "arena.castFlash": [GREYBOX_CAST_FLASH, "greybox kept: the recognizer's cast flash is feedback, not in the study"],
            "arena.rejectFlash": [GREYBOX_REJECT_FLASH, "greybox kept: the recognizer's reject flash is feedback, not in the study"],
            "arena.wardArc": ("01-vr-duel.jpg", (770, 380, 1000, 470), {"smin": 0.5, "vmin": 0.6}),
            "sky.zenith": ("05-arena-wide.jpg", (420, 10, 700, 40), {}),
            "sky.horizon": ("05-arena-wide.jpg", (560, 90, 760, 130), {}),
            "figures.soldier": ("08-enemy-lineup.jpg", (110, 230, 230, 330), {}),
            "figures.slinger": ("08-enemy-lineup.jpg", (390, 290, 480, 440), {}),
            "figures.hound": ("08-enemy-lineup.jpg", (640, 380, 780, 460), {"vmax": 0.35}),
            "figures.mireMaw": ("08-enemy-lineup.jpg", (900, 440, 1000, 540), {"vmax": 0.5, "smax": 0.3}),
            "figures.fireMage": ("01-vr-duel.jpg", (440, 270, 500, 340), {"smin": 0.4}),
            "figures.outline": ("05-arena-wide.jpg", (610, 290, 660, 350), {"vmax": 0.2}),
            "cuff.band": ("06-cuff-hud.jpg", (200, 700, 260, 740), {}),
            "cuff.glyph": ("06-cuff-hud.jpg", (420, 730, 500, 820), {"smin": 0.4, "vmin": 0.5, "hue": (175, 20)}),
            "projectiles.air": [GREYBOX_AIR, "greybox kept: the study draws air as a pale lavender-white crescent, which "
                                "would read as steel (dodge); air keeps its greybox green so it stays an absorb colour"],
        },
    },
    "30": {
        "name": "Chalk & Slate",
        "dir": "30-chalk-slate",
        "kept": "Slate-grey board with chalk-white lines, warm sand chalk floor, chalk-pastel figures; threats keep the "
        "study's saturated chalk (turquoise water, red-orange fire, lime-green air from the cuff study's rune ring).",
        "edge": {"treatment": "chalk-line", "widthPx": 1.5, "note": "A pale chalk line around shapes on the dark slate "
                 "(01 duel, 05 arena); smudge and hatch textures are later art work."},
        "colours": {
            "projectiles.magic": ("07-threat-language.jpg", (60, 280, 200, 400), {"smin": 0.3, "vmin": 0.5}),
            "projectiles.fire": ("07-threat-language.jpg", (230, 150, 490, 520), {"smin": 0.75, "vmin": 0.85, "hue": (34, 8)}),
            "projectiles.earth": ("07-threat-language.jpg", (480, 260, 620, 450), {"smin": 0.5, "vmin": 0.8}),
            "projectiles.air": ("06-cuff-hud.jpg", (640, 50, 880, 230), {"smin": 0.55, "vmin": 0.55, "hue": (85, 25)}),
            "projectiles.steel": ("07-threat-language.jpg", (850, 170, 1060, 540), {"smax": 0.2, "vmin": 0.3, "vmax": 0.85}),
            "projectiles.unblockableBody": ("07-threat-language.jpg", (1100, 310, 1180, 400), {"vmax": 0.2}),
            "projectiles.unblockableRim": ("07-threat-language.jpg", (1030, 220, 1260, 500), {"smin": 0.8, "vmin": 0.6, "hue": (358, 6)}),
            "arena.sand": ("05-arena-wide.jpg", (420, 400, 900, 480), {}),
            "arena.stone": ("05-arena-wide.jpg", (60, 360, 140, 400), {}),
            "arena.dais": ("06-cuff-hud.jpg", (800, 900, 1000, 1000), {"vmax": 0.35}),
            "arena.padRing": ("01-vr-duel.jpg", (800, 395, 1150, 490), {"smin": 0.4, "hue": (285, 30)}),
            "arena.seat": ("05-arena-wide.jpg", (500, 160, 640, 230), {}),
            "arena.marker": ("07-threat-language.jpg", (40, 40, 240, 120), {}),
            "arena.brazier": ("01-vr-duel.jpg", (1100, 660, 1260, 715), {}),
            "arena.flame": ("01-vr-duel.jpg", (630, 200, 720, 280), {"smin": 0.6, "vmin": 0.85}),
            "arena.bannerCloth": ("05-arena-wide.jpg", (120, 20, 170, 120), {"smin": 0.3}),
            "arena.bannerPole": ("01-vr-duel.jpg", (770, 70, 780, 140), {}),
            "arena.wall": ("05-arena-wide.jpg", (420, 280, 640, 300), {}),
            "arena.hand": ("06-cuff-hud.jpg", (560, 300, 640, 380), {"smin": 0.2, "hue": (20, 25)}),
            "arena.castFlash": [GREYBOX_CAST_FLASH, "greybox kept: the recognizer's cast flash is feedback, not in the study"],
            "arena.rejectFlash": [GREYBOX_REJECT_FLASH, "greybox kept: the recognizer's reject flash is feedback, not in the study"],
            "arena.wardArc": ("01-vr-duel.jpg", (620, 470, 820, 600), {"smin": 0.5, "vmin": 0.6}),
            "sky.zenith": ("01-vr-duel.jpg", (560, 10, 740, 60), {}),
            "sky.horizon": ("05-arena-wide.jpg", (520, 230, 650, 260), {}),
            "figures.soldier": ("08-enemy-lineup.jpg", (150, 200, 230, 280), {"smax": 0.25}),
            "figures.slinger": ("08-enemy-lineup.jpg", (400, 340, 520, 420), {"smin": 0.2}),
            "figures.hound": ("08-enemy-lineup.jpg", (680, 380, 840, 470), {"smin": 0.4}),
            "figures.mireMaw": ("08-enemy-lineup.jpg", (950, 480, 1210, 590), {"smin": 0.2}),
            "figures.fireMage": ("01-vr-duel.jpg", (820, 320, 950, 420), {"smin": 0.4}),
            "figures.outline": ("08-enemy-lineup.jpg", (200, 250, 330, 450), {"smax": 0.15, "vmin": 0.75}),
            "cuff.band": ("06-cuff-hud.jpg", (430, 760, 500, 820), {}),
            "cuff.glyph": ("06-cuff-hud.jpg", (300, 640, 380, 700), {"smin": 0.4, "vmin": 0.5, "hue": (180, 20)}),
        },
    },
}

THREAT_SHAPES = {
    "_note": "Fixed by the threat language; a skin may recolour within a family, never change these shapes or meanings.",
    "magic": "ring (absorb, element colour)",
    "physical": "angular (dodge, steel)",
    "unblockable": "black core with a red rim (leave)",
}

TELEGRAPHS = {
    "_note": "No colours here on purpose: every telegraph, projectile, cast and the fire wall are drawn through "
    "ElementLook (Session/ElementColour), so they take the `projectiles` palette. One palette stays the authority.",
    "source": "projectiles",
}


def srgb_to_linear(c):
    c = c / 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def hue_in(h_deg, centre, half):
    d = abs((h_deg - centre + 180.0) % 360.0 - 180.0)
    return d <= half


def sample(path, box, flt):
    img = Image.open(path).convert("RGB")
    crop = img.crop(box)
    rs, gs, bs = [], [], []
    data = crop.tobytes()
    for r, g, b in zip(data[0::3], data[1::3], data[2::3]):
        h, s, v = colorsys.rgb_to_hsv(r / 255.0, g / 255.0, b / 255.0)
        if "smin" in flt and s < flt["smin"]:
            continue
        if "smax" in flt and s > flt["smax"]:
            continue
        if "vmin" in flt and v < flt["vmin"]:
            continue
        if "vmax" in flt and v > flt["vmax"]:
            continue
        if "hue" in flt and not hue_in(h * 360.0, *flt["hue"]):
            continue
        rs.append(r)
        gs.append(g)
        bs.append(b)
    n = len(rs)
    if n < 20:
        raise SystemExit(f"{path} {box} {flt}: only {n} pixels pass the filter")
    med = lambda xs: sorted(xs)[len(xs) // 2]
    return (med(rs), med(gs), med(bs)), n, crop


def linear_to_srgb8(lin):
    out = []
    for c in lin:
        s = c * 12.92 if c <= 0.0031308 else 1.055 * (c ** (1 / 2.4)) - 0.055
        out.append(max(0, min(255, round(s * 255))))
    return tuple(out)


def hsv(rgb8):
    return colorsys.rgb_to_hsv(*(c / 255.0 for c in rgb8))


def dist(a, b):
    return sum((x - y) ** 2 for x, y in zip(a, b)) ** 0.5


def check_threat_language(style_id, rgb):
    """rgb: key -> sRGB 8-bit tuple. Returns a list of failures."""
    fails = []
    magic = {k: rgb["projectiles." + k] for k in ("magic", "fire", "air", "earth")}
    steel = rgb["projectiles.steel"]
    body = rgb["projectiles.unblockableBody"]
    rim = rgb["projectiles.unblockableRim"]
    # Thresholds are sRGB 8-bit distances, set from the greybox palette (its closest pairs: fire to rim 110, earth to
    # rim 114, air to steel 127, water to air 138), so the greybox passes and a skin may not crowd the families closer.
    for k, c in magic.items():
        if hsv(c)[1] < 0.35:
            fails.append(f"{k} saturation {hsv(c)[1]:.2f} < 0.35: would not read as an element (absorb) colour")
        if dist(c, steel) < 100:
            fails.append(f"{k} is {dist(c, steel):.0f} from steel (< 100)")
        if dist(c, rim) < 100:
            fails.append(f"{k} is {dist(c, rim):.0f} from the unblockable rim (< 100)")
    # Schools with a caster in the slice must tell apart; Earth has none yet (greybox fire to earth is 49).
    casters = ("magic", "fire", "air")
    for i in range(len(casters)):
        for j in range(i + 1, len(casters)):
            d = dist(magic[casters[i]], magic[casters[j]])
            if d < 100:
                fails.append(f"{casters[i]} and {casters[j]} are {d:.0f} apart (< 100)")
    if hsv(steel)[1] > 0.2:
        fails.append(f"steel saturation {hsv(steel)[1]:.2f} > 0.2")
    if dist(steel, body) < 100:
        fails.append(f"steel is {dist(steel, body):.0f} from the unblockable body (< 100)")
    if hsv(body)[2] > 0.2:
        fails.append(f"unblockable body value {hsv(body)[2]:.2f} > 0.2 (must read as a black core)")
    h, s, v = hsv(rim)
    if not hue_in(h * 360.0, 0, 15) or s < 0.6 or v < 0.5:
        fails.append(f"unblockable rim hsv ({h * 360:.0f}, {s:.2f}, {v:.2f}) is not a strong red")
    # T26: fire moved toward orange so an absorb cue never borrows the leave cue's red.
    fh = hsv(magic["fire"])[0] * 360.0
    rh = h * 360.0
    if abs((fh - rh + 180) % 360 - 180) < 8:
        fails.append(f"fire hue {fh:.0f} within 8 deg of the rim hue {rh:.0f}")
    return [f"style {style_id}: {f}" for f in fails]


def build(style_id, spec, sheet_dir):
    doc = {
        "_note": "T16 style skin, written by apps/vr/tools/styles/sample-styles.py from the style-study images; do not "
        "hand-edit, change the sampler. Linear RGB 0..1, unlit flat colours. Selected with -MageArenaStyle=" + style_id +
        "; without the flag the game draws the greybox. Today only `projectiles` is read (ApplyStylePalette, "
        "Session/ElementColour); `arena` mirrors the arena-layout.json `colours` keys (plus `wall`, the arena perimeter wall); `arena`, `sky`, `figures`, `cuff` and `edge` wait for the skin loader.",
        "id": style_id,
        "name": spec["name"],
        "unlit": True,
        "kept": spec["kept"],
        "threatShapes": THREAT_SHAPES,
        "edge": spec["edge"],
        "telegraphs": TELEGRAPHS,
    }
    sources = {}
    rgb = {}
    crops = []
    for key in sorted(spec["colours"]):
        entry = spec["colours"][key]
        if isinstance(entry, list):
            lin, reason = entry
            rgb[key] = linear_to_srgb8(lin)
            sources[key] = reason
            crops.append((key, None, rgb[key]))
        else:
            image, box, flt = entry
            path = os.path.join(STUDIES, spec["dir"], image)
            rgb8, n, crop = sample(path, box, flt)
            lin = [round(srgb_to_linear(c), 4) for c in rgb8]
            rgb[key] = rgb8
            fdesc = ", ".join(
                f"{k} {v[0]}+-{v[1]}deg" if k == "hue" else f"{k} {v}" for k, v in sorted(flt.items())
            ) or "none"
            sources[key] = (
                f"{spec['dir']}/{image} box x{box[0]}-{box[2]} y{box[1]}-{box[3]}, filter {fdesc}: median of {n} px = "
                f"sRGB #{rgb8[0]:02x}{rgb8[1]:02x}{rgb8[2]:02x}"
            )
            crops.append((key, crop, rgb8))
        group, name = key.split(".")
        doc.setdefault(group, {})[name] = lin
    fails = check_threat_language(style_id, rgb)
    doc["_source"] = sources
    if sheet_dir:
        write_sheet(os.path.join(sheet_dir, f"swatches-{style_id}.png"), crops)
    return doc, fails


def write_sheet(path, crops):
    row_h, w = 70, 520
    sheet = Image.new("RGB", (w, row_h * len(crops)), (255, 255, 255))
    draw = ImageDraw.Draw(sheet)
    for i, (key, crop, rgb8) in enumerate(crops):
        y = i * row_h
        if crop is not None:
            c = crop.copy()
            c.thumbnail((200, row_h - 6))
            sheet.paste(c, (4, y + 3))
        draw.rectangle([210, y + 3, 300, y + row_h - 3], fill=rgb8)
        draw.text((310, y + 25), key, fill=(0, 0, 0))
    os.makedirs(os.path.dirname(path), exist_ok=True)
    sheet.save(path)


def main():
    sheet_dir = None
    if "--sheet" in sys.argv:
        sheet_dir = sys.argv[sys.argv.index("--sheet") + 1]
    docs, fails = {}, []
    for style_id, spec in STYLES.items():
        doc, f = build(style_id, spec, sheet_dir)
        docs[style_id] = doc
        fails += f
    if fails:
        print("threat language check FAILED:")
        for f in fails:
            print("  " + f)
        return 1
    os.makedirs(OUT, exist_ok=True)
    for style_id, doc in docs.items():
        path = os.path.join(OUT, f"style-{style_id}.json")
        with open(path, "w", encoding="utf-8", newline="\n") as fh:
            text = json.dumps(doc, indent=2, ensure_ascii=False)
            # One colour per line: [r, g, b] on a single row.
            text = re.sub(r"\[\s+([-0-9.]+),\s+([-0-9.]+),\s+([-0-9.]+)\s+\]", r"[\1, \2, \3]", text)
            fh.write(text + "\n")
        print(f"wrote {os.path.relpath(path, ROOT)}")
    print("threat language check OK for " + ", ".join(docs))
    return 0


if __name__ == "__main__":
    sys.exit(main())
