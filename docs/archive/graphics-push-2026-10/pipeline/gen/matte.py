"""BiRefNet matte of a frame folder through ComfyUI (LoadVideo -> RemoveBackground -> MaskToImage -> SaveImage).
usage: python matte.py <frames_dir> <out_dir>   (frames f_####.png; writes m_####.png, white = figure)"""
import json, os, subprocess, sys
sys.path.insert(0, os.path.dirname(__file__))
import comfy

src, out = sys.argv[1], sys.argv[2]
os.makedirs(out, exist_ok=True)
mp4 = os.path.join(out, "_in.mp4")
subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-framerate", "30", "-i", os.path.join(src, "f_%04d.png"), "-c:v", "libx264", "-crf", "8", "-pix_fmt", "yuv444p", mp4], check=True)
name = comfy.upload(mp4, sub="")
g = {
    "1": {"class_type": "LoadVideo", "inputs": {"file": name}},
    "2": {"class_type": "GetVideoComponents", "inputs": {"video": ["1", 0]}},
    "3": {"class_type": "LoadBackgroundRemovalModel", "inputs": {"bg_removal_name": "birefnet.safetensors"}},
    "4": {"class_type": "RemoveBackground", "inputs": {"bg_removal_model": ["3", 0], "image": ["2", 0]}},
    "5": {"class_type": "MaskToImage", "inputs": {"mask": ["4", 0]}},
    "6": {"class_type": "SaveImage", "inputs": {"images": ["5", 0], "filename_prefix": "mage-s2/matte"}},
}
pid, outs, wall = comfy.run(g)
for i, x in enumerate(comfy.files_of(outs)):
    comfy.fetch(x, os.path.join(out, "m_%04d.png" % i))
json.dump({"src": src, "model": "birefnet", "wall_s": round(wall, 1), "graph": g}, open(os.path.join(out, "matte.json"), "w"), indent=1)
print(out, len(comfy.files_of(outs)), "mattes", round(wall, 1), "s")
