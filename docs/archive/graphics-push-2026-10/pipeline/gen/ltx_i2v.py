"""LTX-2.5 distilled image-to-video via gravitone's converted blueprint (pipeline/video/ltx-i2v-api.json, node table in
its .md). usage: python ltx_i2v.py <asset> <still.png> <seconds> <seed> <w> <h>   prompt from <asset>/i2v.prompt.txt
Writes <asset>/i2v-s<seed>/f_####.png and the settings json."""
import copy, json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
import comfy

HERE = os.path.dirname(os.path.abspath(__file__))
GRAPH = "C:/Users/kazda/kiro/gravitone-gcloud/pipeline/video/ltx-i2v-api.json"

asset, still, seconds, seed, w, h = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5]), int(sys.argv[6])
base = os.path.join(HERE, asset)
prompt = open(os.path.join(base, os.environ.get("PROMPT", "i2v") + ".prompt.txt"), encoding="utf-8").read().strip()
g = copy.deepcopy(json.load(open(GRAPH)))
g["1"]["inputs"]["image"] = comfy.upload(still)
g["452:409"]["inputs"]["value"] = prompt
g["452:418"]["inputs"]["value"] = w
g["452:421"]["inputs"]["value"] = h
g["452:413"]["inputs"]["value"] = seconds
g["452:425"]["inputs"]["value"] = 24
g["452:423"]["inputs"]["noise_seed"] = seed
g["452:442"]["inputs"]["noise_seed"] = seed + 1
# Frames, not an mp4: save the decoded images directly.
g["2"] = {"class_type": "SaveImage", "inputs": {"images": ["452:449", 0], "filename_prefix": f"mage-s2/i2v-{asset}-{seed}"}}
del g["452:451"], g["452:450"]
pid, outs, wall = comfy.run(g, cap_s=2400)
out = os.path.join(base, f"i2v-s{seed}")
os.makedirs(out, exist_ok=True)
files = comfy.files_of(outs)
for i, x in enumerate(files):
    comfy.fetch(x, os.path.join(out, "f_%04d.png" % i))
json.dump({"asset": asset, "still": still, "seconds": seconds, "fps": 24, "seed": seed, "size": [w, h], "prompt": prompt, "model": "ltx-2.5-22b-distilled int8, 2-pass x2 latent upscale", "wall_s": round(wall, 1), "prompt_id": pid, "graph": g}, open(out + ".json", "w"), indent=1)
print(out, len(files), "frames", round(wall, 1), "s", flush=True)
