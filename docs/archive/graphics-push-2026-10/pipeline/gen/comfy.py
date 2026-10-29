"""Minimal ComfyUI HTTP client for the S2 spike (pattern: gravitone lib/imaging/local/flux2.ts, comfyLtx.ts).
Never POSTs /free, never touches prompts that are not ours."""
import hashlib, json, os, time, uuid, urllib.request, urllib.parse

BASE = os.environ.get("COMFY_URL", "http://127.0.0.1:8188")
CLIENT = "tierb-s2-" + uuid.uuid4().hex[:8]


def _req(rel, data=None, headers=None, timeout=60):
    r = urllib.request.Request(BASE + rel, data=data, headers=headers or {})
    with urllib.request.urlopen(r, timeout=timeout) as f:
        return f.read()


def upload(path, sub="mage-s2"):
    b = open(path, "rb").read()
    ext = os.path.splitext(path)[1].lower() or ".png"
    name = "ref-" + hashlib.sha256(b).hexdigest()[:16] + ext
    bnd = "----s2" + uuid.uuid4().hex
    mime = "image/jpeg" if ext in (".jpg", ".jpeg") else "image/png"
    parts = []
    for k, v in (("type", "input"), ("subfolder", sub), ("overwrite", "true")):
        parts.append(f'--{bnd}\r\nContent-Disposition: form-data; name="{k}"\r\n\r\n{v}\r\n'.encode())
    parts.append(f'--{bnd}\r\nContent-Disposition: form-data; name="image"; filename="{name}"\r\nContent-Type: {mime}\r\n\r\n'.encode() + b + b"\r\n")
    parts.append(f"--{bnd}--\r\n".encode())
    up = json.loads(_req("/upload/image", b"".join(parts), {"Content-Type": f"multipart/form-data; boundary={bnd}"}, 120))
    return f"{up['subfolder']}/{up['name']}" if up.get("subfolder") else up["name"]


def run(graph, cap_s=1800, poll=4):
    body = json.dumps({"prompt": graph, "client_id": CLIENT}).encode()
    try:
        res = json.loads(_req("/prompt", body, {"Content-Type": "application/json"}))
    except urllib.error.HTTPError as e:
        raise RuntimeError("prompt refused: " + e.read().decode()[:3000])
    pid = res["prompt_id"]
    t0 = time.time()
    while True:
        try:
            h = json.loads(_req("/history/" + pid, timeout=120))
        except Exception:  # a busy server can miss a poll; the cap still bounds the wait
            time.sleep(poll)
            continue
        if pid in h:
            e = h[pid]
            st = e.get("status", {})
            if st.get("status_str") == "error":
                raise RuntimeError("comfy error: " + json.dumps(st.get("messages", []))[:3000])
            return pid, e.get("outputs", {}), time.time() - t0
        if time.time() - t0 > cap_s:
            raise RuntimeError("cap reached for " + pid)
        time.sleep(poll)


def files_of(outputs):
    out = []
    for nid, o in outputs.items():
        for key in ("images", "gifs", "videos", "video"):
            for x in o.get(key, []) or []:
                # Only saved stills: a LoadVideo node also reports its input as a preview.
                if isinstance(x, dict) and str(x.get("filename", "")).lower().endswith(".png") and x.get("type", "output") == "output":
                    out.append(x)
    return out


def fetch(x, dest):
    q = urllib.parse.urlencode({"filename": x["filename"], "subfolder": x.get("subfolder", ""), "type": x.get("type", "output")})
    data = _req("/view?" + q, timeout=300)
    open(dest, "wb").write(data)
    return dest
