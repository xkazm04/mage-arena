"""Brennic key frames via local Flux 2 dev fp8 with reference latents admitted late (gravitone flux2.ts graph).
usage: python flux_key.py <name> <seed,seed,...> [late] [refs...]   prompt from gen/brennic/<name>.prompt.txt"""
import json, os, sys, time
sys.path.insert(0, os.path.dirname(__file__))
import comfy

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = "C:/Users/kazda/kiro/mage-arena-epic/apps/vr/art"
DEFAULT_REFS = [REPO + "/characters/brennic/concept-front-s3801-medium.jpg", REPO + "/key-art/style30/s30-key.jpg"]


def graph(prompt, seed, w, h, refs, late, prefix, steps=20, guidance=4.0, init=None, denoise=1.0, mask=None):
    g = {
        "1": {"class_type": "UNETLoader", "inputs": {"unet_name": "flux2_dev_fp8mixed.safetensors", "weight_dtype": "default"}},
        "2": {"class_type": "CLIPLoader", "inputs": {"clip_name": "mistral_3_small_flux2_fp8.safetensors", "type": "flux2", "device": "default"}},
        "3": {"class_type": "VAELoader", "inputs": {"vae_name": "flux2-vae.safetensors"}},
        "4": {"class_type": "CLIPTextEncode", "inputs": {"clip": ["2", 0], "text": prompt}},
        "6": {"class_type": "EmptyFlux2LatentImage", "inputs": {"width": w, "height": h, "batch_size": 1}},
        "7": {"class_type": "Flux2Scheduler", "inputs": {"steps": steps, "width": w, "height": h}},
        "8": {"class_type": "KSamplerSelect", "inputs": {"sampler_name": "euler"}},
        "9": {"class_type": "RandomNoise", "inputs": {"noise_seed": seed}},
    }
    cond = ["4", 0]
    for i, name in enumerate(refs):
        g[f"20{i}"] = {"class_type": "LoadImage", "inputs": {"image": name}}
        g[f"21{i}"] = {"class_type": "VAEEncode", "inputs": {"pixels": [f"20{i}", 0], "vae": ["3", 0]}}
        g[f"22{i}"] = {"class_type": "ReferenceLatent", "inputs": {"conditioning": cond, "latent": [f"21{i}", 0]}}
        cond = [f"22{i}", 0]
    if refs and late > 0:
        g["30"] = {"class_type": "ConditioningSetTimestepRange", "inputs": {"conditioning": ["4", 0], "start": 0.0, "end": late}}
        g["31"] = {"class_type": "ConditioningSetTimestepRange", "inputs": {"conditioning": cond, "start": late, "end": 1.0}}
        g["32"] = {"class_type": "ConditioningCombine", "inputs": {"conditioning_1": ["30", 0], "conditioning_2": ["31", 0]}}
        cond = ["32", 0]
    if init:
        # img2img: the init image's latent, the schedule's last `denoise` part (pose and composition from the init).
        g["40"] = {"class_type": "LoadImage", "inputs": {"image": init}}
        g["41"] = {"class_type": "VAEEncode", "inputs": {"pixels": ["40", 0], "vae": ["3", 0]}}
        g["42"] = {"class_type": "SplitSigmasDenoise", "inputs": {"sigmas": ["7", 0], "denoise": denoise}}
        if mask:
            # Inpaint: only the white of the mask is re-noised and repainted; the rest stays the init image.
            g["43"] = {"class_type": "LoadImage", "inputs": {"image": mask}}
            g["44"] = {"class_type": "ImageToMask", "inputs": {"image": ["43", 0], "channel": "red"}}
            g["45"] = {"class_type": "SetLatentNoiseMask", "inputs": {"samples": ["41", 0], "mask": ["44", 0]}}
    g["5"] = {"class_type": "FluxGuidance", "inputs": {"conditioning": cond, "guidance": guidance}}
    g["10"] = {"class_type": "BasicGuider", "inputs": {"model": ["1", 0], "conditioning": ["5", 0]}}
    g["11"] = {"class_type": "SamplerCustomAdvanced", "inputs": {"noise": ["9", 0], "guider": ["10", 0], "sampler": ["8", 0], "sigmas": ["42", 1] if init else ["7", 0], "latent_image": (["45", 0] if mask else ["41", 0]) if init else ["6", 0]}}
    g["12"] = {"class_type": "VAEDecode", "inputs": {"samples": ["11", 0], "vae": ["3", 0]}}
    g["13"] = {"class_type": "SaveImage", "inputs": {"images": ["12", 0], "filename_prefix": prefix}}
    return g


if __name__ == "__main__":
    name = sys.argv[1]
    seeds = [int(s) for s in sys.argv[2].split(",")]
    late = float(sys.argv[3]) if len(sys.argv) > 3 else 0.25
    refpaths = [] if os.environ.get("NOREFS") == "1" else (sys.argv[4:] or DEFAULT_REFS)
    w, h = int(os.environ.get("W", 768)), int(os.environ.get("H", 1344))
    out = os.path.join(HERE, os.environ.get("ASSET", "brennic"))
    prompt = open(os.path.join(out, name + ".prompt.txt"), encoding="utf-8").read().strip()
    refs = [comfy.upload(p) for p in refpaths]
    init_path = os.environ.get("INIT")
    init = comfy.upload(init_path) if init_path else None
    denoise = float(os.environ.get("DENOISE", "1.0"))
    mask_path = os.environ.get("MASK")
    mask = comfy.upload(mask_path) if mask_path else None
    for seed in seeds:
        g = graph(prompt, seed, w, h, refs, late, f"mage-s2/{name}-{seed}", init=init, denoise=denoise, mask=mask)
        pid, outs, wall = comfy.run(g)
        for x in comfy.files_of(outs):
            dest = os.path.join(out, f"{name}-s{seed}.png")
            comfy.fetch(x, dest)
            meta = {"prompt": prompt, "seed": seed, "width": w, "height": h, "late": late, "refs": refpaths, "init": init_path, "mask": mask_path, "denoise": denoise, "model": "flux2_dev_fp8mixed", "steps": 20, "guidance": 4.0, "sampler": "euler", "wall_s": round(wall, 1), "prompt_id": pid, "graph": g}
            json.dump(meta, open(dest[:-4] + ".json", "w"), indent=1)
            print(dest, round(wall, 1), "s", flush=True)
