# Connected components over an edge list, numpy only (Blender's bundled Python has no scipy).
# Hook-and-compress union-find: every pass hooks each edge's larger root under the smaller one, then pointer-jumps
# until every node points at its root. It converges in a few passes even on a 700k-face mesh, where a Python loop
# over the edges takes minutes.
import numpy as np


def labels(n, a, b):
    """Component label (the smallest node id in the component) for nodes 0..n-1 joined by edges a[i]-b[i]."""
    p = np.arange(n, dtype=np.int64)
    a = np.asarray(a, dtype=np.int64); b = np.asarray(b, dtype=np.int64)
    if not len(a):
        return p
    while True:
        pa, pb = p[a], p[b]
        if (pa == pb).all():
            return p
        lo = np.minimum(pa, pb); hi = np.maximum(pa, pb)
        np.minimum.at(p, hi, lo)
        while True:
            pp = p[p]
            if (pp == p).all():
                break
            p = pp


def sizes(lab, weights=None):
    """Component sizes, largest first, as (label, size) pairs."""
    u, inv = np.unique(lab, return_inverse=True)
    s = np.bincount(inv.ravel(), weights=weights)
    order = np.argsort(-s, kind='stable')
    return [(int(u[i]), float(s[i]) if weights is not None else int(s[i])) for i in order]
