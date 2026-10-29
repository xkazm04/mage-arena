"""MiniMax H3 first/last-frame to video (local fl2va, int8, 8-step turbo LoRA), after the ComfyUI blueprint
"Image to Video (MiniMax H3)". usage: python h3_fl2va.py <asset> <tag> <first.png> <last.png> <frames> <w> <h> <seed>
prompt from <asset>/<PROMPT or h3>.prompt.txt. Writes <asset>/h3-<tag>/f_####.png and its settings json."""
import json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
import comfy

HERE = os.path.dirname(os.path.abspath(__file__))
asset, tag, first, last, frames, w, h, seed = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], int(sys.argv[5]), int(sys.argv[6]), int(sys.argv[7]), int(sys.argv[8])
base = os.path.join(HERE, asset)
prompt = open(os.path.join(base, os.environ.get("PROMPT", "h3") + ".prompt.txt"), encoding="utf-8").read().strip()
steps = int(os.environ.get("STEPS", "8"))
g = {
    "1": {"class_type": "UNETLoader", "inputs": {"unet_name": "minimax_h3_fl2va_pruned_int8_convrot.safetensors", "weight_dtype": "default"}},
    "2": {"class_type": "LoraLoaderModelOnly", "inputs": {"model": ["1", 0], "lora_name": "minimax_h3_fl2v_turbo_8step_v1.0_comfyui_bf16.safetensors", "strength_model": 1.0}},
    "3": {"class_type": "CLIPLoader", "inputs": {"clip_name": "qwen3vl_32b_minimax_h3_int8_convrot.safetensors", "type": "minimax", "device": "default"}},
    "4": {"class_type": "VAELoader", "inputs": {"vae_name": "minimax_h3_video_vae_int8_convrot.safetensors"}},
    "5": {"class_type": "LoadImage", "inputs": {"image": comfy.upload(first)}},
    "6": {"class_type": "LoadImage", "inputs": {"image": comfy.upload(last)}},
    "7": {"class_type": "MiniMaxH3ImageToVideo", "inputs": {"clip": ["3", 0], "vae": ["4", 0], "prompt": prompt, "width": w, "height": h, "length": frames, "first_frame": ["5", 0], "last_frame": ["6", 0]}},
    "8": {"class_type": "BasicGuider", "inputs": {"model": ["2", 0], "conditioning": ["7", 0]}},
    "9": {"class_type": "KSamplerSelect", "inputs": {"sampler_name": "res_multistep"}},
    "10": {"class_type": "BasicScheduler", "inputs": {"model": ["2", 0], "scheduler": "simple", "steps": steps, "denoise": 1.0}},
    "11": {"class_type": "RandomNoise", "inputs": {"noise_seed": seed}},
    "12": {"class_type": "SamplerCustomAdvanced", "inputs": {"noise": ["11", 0], "guider": ["8", 0], "sampler": ["9", 0], "sigmas": ["10", 0], "latent_image": ["7", 1]}},
    "13": {"class_type": "VAEDecode", "inputs": {"samples": ["12", 0], "vae": ["4", 0]}},
    "14": {"class_type": "SaveImage", "inputs": {"images": ["13", 0], "filename_prefix": f"mage-s2/h3-{asset}-{tag}"}},
}
pid, outs, wall = comfy.run(g, cap_s=2400)
out = os.path.join(base, f"h3-{tag}")
os.makedirs(out, exist_ok=True)
files = comfy.files_of(outs)
for i, x in enumerate(files):
    comfy.fetch(x, os.path.join(out, "f_%04d.png" % i))
json.dump({"asset": asset, "first": first, "last": last, "frames": frames, "size": [w, h], "seed": seed, "steps": steps, "prompt": prompt, "model": "minimax_h3_fl2va int8 + turbo 8-step LoRA", "wall_s": round(wall, 1), "prompt_id": pid, "graph": g}, open(out + ".json", "w"), indent=1)
print(out, len(files), "frames", round(wall, 1), "s", flush=True)
