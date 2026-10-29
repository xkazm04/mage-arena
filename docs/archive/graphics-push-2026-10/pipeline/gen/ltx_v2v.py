"""LTX-2.5 distilled video-to-video repaint of a rig clip (SDEdit-style: the rig video's latent, noised to sigma `start`,
denoised under the chalk prompt, frame 0 laid in from the painted key frame).
usage: python ltx_v2v.py <clip> <drive.mp4> <frame0.png> <frames> <start_sigma> <seed> [tag]   prompt from brennic/v2v.prompt.txt"""
import json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
import comfy

HERE = os.path.dirname(os.path.abspath(__file__))
SIGMAS = [1.0, 0.99375, 0.9875, 0.98125, 0.975, 0.909375, 0.725, 0.421875, 0.0]
NEG = "photo, 3d render, cgi, plastic, smooth shading, blurry, morphing face, extra limbs, camera movement, zoom, text, watermark"


def graph(prompt, video, frame0, frames, start, seed, prefix, fps=30, guide=1.0, guides=()):
    sig = [s for s in SIGMAS if s <= start + 1e-6]
    if sig[0] < start:
        sig = [start] + sig
    g = {
        "10": {"class_type": "UNETLoader", "inputs": {"unet_name": "ltx-2.5-22b-distilled-transformer-comfy-int8-convrot.safetensors", "weight_dtype": "default"}},
        "11": {"class_type": "CLIPLoader", "inputs": {"clip_name": "gemma4-12b-with-proj-ltx-2.5-comfy-int8-convrot.safetensors", "type": "ltxv", "device": "default"}},
        "12": {"class_type": "VAELoader", "inputs": {"vae_name": "ltx-2.5-video-vae-bf16.safetensors"}},
        "13": {"class_type": "VAELoader", "inputs": {"vae_name": "ltx-2.5-audio-vae-bf16.safetensors"}},
        "14": {"class_type": "CLIPTextEncode", "inputs": {"clip": ["11", 0], "text": prompt}},
        "15": {"class_type": "CLIPTextEncode", "inputs": {"clip": ["11", 0], "text": NEG}},
        "16": {"class_type": "LTXVConditioning", "inputs": {"positive": ["14", 0], "negative": ["15", 0], "frame_rate": float(fps)}},
        "20": {"class_type": "LoadVideo", "inputs": {"file": video}},
        "21": {"class_type": "GetVideoComponents", "inputs": {"video": ["20", 0]}},
        "22": {"class_type": "VAEEncode", "inputs": {"pixels": ["21", 0], "vae": ["12", 0]}},
        "23": {"class_type": "LoadImage", "inputs": {"image": frame0}},
        "24": {"class_type": "LTXVPreprocess", "inputs": {"image": ["23", 0], "img_compression": 18}},
        "25": {"class_type": "LTXVImgToVideoInplace", "inputs": {"vae": ["12", 0], "image": ["24", 0], "latent": ["22", 0], "strength": guide, "bypass": False}},
        "26": {"class_type": "LTXVEmptyLatentAudio", "inputs": {"frames_number": frames, "frame_rate": fps, "batch_size": 1, "audio_vae": ["13", 0]}},
        "27": {"class_type": "LTXVConcatAVLatent", "inputs": {"video_latent": ["25", 0], "audio_latent": ["26", 0]}},
        "28": {"class_type": "ManualSigmas", "inputs": {"sigmas": ", ".join(str(s) for s in sig)}},
        "29": {"class_type": "LTXVDualCFGGuider", "inputs": {"model": ["10", 0], "positive": ["16", 0], "negative": ["16", 1], "video_cfg": 1, "audio_cfg": 1}},
        "30": {"class_type": "KSamplerSelect", "inputs": {"sampler_name": "euler_ancestral"}},
        "31": {"class_type": "RandomNoise", "inputs": {"noise_seed": seed}},
        "32": {"class_type": "SamplerCustomAdvanced", "inputs": {"noise": ["31", 0], "guider": ["29", 0], "sampler": ["30", 0], "sigmas": ["28", 0], "latent_image": ["27", 0]}},
        "33": {"class_type": "LTXVSeparateAVLatent", "inputs": {"av_latent": ["32", 0]}},
        "34": {"class_type": "VAEDecodeTiled", "inputs": {"samples": ["33", 0], "vae": ["12", 0], "tile_size": 512, "overlap": 64, "temporal_size": 64, "temporal_overlap": 16}},
        "35": {"class_type": "SaveImage", "inputs": {"images": ["34", 0], "filename_prefix": prefix}},
    }
    # Painted keyframes laid in at their frame (a multiple of 8, or -1 for the last): style and pose pinned mid-clip too.
    if guides:
        pos, neg, lat = ["16", 0], ["16", 1], ["25", 0]
        for i, (idx, name, strength) in enumerate(guides):
            g[f"4{i}"] = {"class_type": "LoadImage", "inputs": {"image": name}}
            g[f"5{i}"] = {"class_type": "LTXVPreprocess", "inputs": {"image": [f"4{i}", 0], "img_compression": 18}}
            g[f"6{i}"] = {"class_type": "LTXVAddGuide", "inputs": {"positive": pos, "negative": neg, "vae": ["12", 0], "latent": lat, "image": [f"5{i}", 0], "frame_idx": idx, "strength": strength}}
            pos, neg, lat = [f"6{i}", 0], [f"6{i}", 1], [f"6{i}", 2]
        g["27"]["inputs"]["video_latent"] = lat
        g["29"]["inputs"]["positive"], g["29"]["inputs"]["negative"] = pos, neg
        g["36"] = {"class_type": "LTXVCropGuides", "inputs": {"positive": pos, "negative": neg, "latent": ["33", 0]}}
        g["34"]["inputs"]["samples"] = ["36", 2]
    return g, sig


if __name__ == "__main__":
    clip, drive, frame0, frames, start, seed = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4]), float(sys.argv[5]), int(sys.argv[6])
    tag = sys.argv[7] if len(sys.argv) > 7 else f"s{start:.2f}"
    base = os.path.join(HERE, "brennic")
    prompt = open(os.path.join(base, os.environ.get("PROMPT", "v2v") + ".prompt.txt"), encoding="utf-8").read().strip()
    video = comfy.upload(drive, sub="")
    f0 = comfy.upload(frame0)
    guides = []
    for item in filter(None, os.environ.get("GUIDES", "").split(",")):
        idx, path = item.split(":", 1)
        guides.append((int(idx), comfy.upload(path), float(os.environ.get("GUIDE_STRENGTH", "1.0"))))
    g, sig = graph(prompt, video, f0, frames, start, seed, f"mage-s2/v2v-{clip}-{tag}", guide=float(os.environ.get("GUIDE", "1.0")), guides=guides)
    pid, outs, wall = comfy.run(g, cap_s=2400)
    out = os.path.join(base, "v2v", f"{clip}-{tag}")
    os.makedirs(out, exist_ok=True)
    files = comfy.files_of(outs)
    for i, x in enumerate(files):
        comfy.fetch(x, os.path.join(out, "f_%04d.png" % i))
    json.dump({"clip": clip, "drive": drive, "frame0": frame0, "guides": os.environ.get("GUIDES", ""), "frames": frames, "sigmas": sig, "seed": seed, "prompt": prompt, "negative": NEG, "model": "ltx-2.5-22b-distilled int8", "fps": 30, "wall_s": round(wall, 1), "prompt_id": pid, "graph": g}, open(out + ".json", "w"), indent=1)
    print(out, len(files), "frames", round(wall, 1), "s", flush=True)
