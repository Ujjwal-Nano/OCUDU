#!/usr/bin/env python3
"""
plot_csi.py — analysis plots for swap_metrics.jsonl (OCUDU swap scheduler CSI log)

Usage:
    python3 plot_csi.py /tmp/swap_metrics.jsonl            # -> csi_plots.png
    python3 plot_csi.py mylog.jsonl -o run3.png            # custom output name

Produces one figure with three panels per user (auto-scales to N users):
  1) Per-RU CSI power over time (dB)      — frequency-selective fading, events
  2) RU x time heatmap (dB)               — the [RU x t] channel matrix at a glance
  3) CCDF of per-RU CSI (dB)              — outage-style statistic (paper metric)

Notes:
  * x-axis is the sample index (log line order), not the raw slot number —
    slot_point wraps every 1024 frames, so raw slots are not monotonic.
  * Duplicate consecutive samples (swap period firing twice per SRS refresh)
    are collapsed by default; use --keep-dups to keep them.
"""
import argparse, json, math, sys
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

EPS = 1e-12

def load(path, keep_dups=False):
    """Return {user_key: {"rnti": [...], "csi": [ [ru...] x t ]}} preserving line order."""
    users = {}
    prev = {}
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                d = json.loads(line)
            except json.JSONDecodeError:
                continue
            for u in d.get("users", []):
                key = u.get("u", 0)
                csi = u.get("csi")
                if csi is None:
                    continue
                if not keep_dups and prev.get(key) == csi:
                    continue
                prev[key] = csi
                rec = users.setdefault(key, {"rnti": [], "csi": []})
                rec["rnti"].append(u.get("rnti", -1))
                rec["csi"].append(csi)
    return users

def to_db(a):
    return 10.0 * np.log10(np.maximum(np.asarray(a, dtype=float), EPS))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("jsonl")
    ap.add_argument("-o", "--out", default="csi_plots.png")
    ap.add_argument("--keep-dups", action="store_true")
    args = ap.parse_args()

    users = load(args.jsonl, args.keep_dups)
    if not users:
        sys.exit("no usable lines found in " + args.jsonl)

    n_users = len(users)
    fig, axes = plt.subplots(n_users, 3, figsize=(16, 4.6 * n_users), squeeze=False)
    fig.suptitle("SRS-derived per-RU CSI — " + args.jsonl, fontsize=13, y=0.995)

    for row, (ukey, rec) in enumerate(sorted(users.items())):
        M = to_db(rec["csi"])                     # [t x R] in dB
        t = np.arange(M.shape[0])
        R = M.shape[1]
        rntis = sorted(set(rec["rnti"]))

        # 1) time series
        ax = axes[row][0]
        for r in range(R):
            ax.plot(t, M[:, r], lw=1.4, label=f"RU{r} (RB {12*r}-{12*r+11})")
        ax.set_title(f"user {ukey} (rnti {', '.join(hex(x) for x in rntis)}) — per-RU power vs time")
        ax.set_xlabel("sample (one per swap period with new SRS)")
        ax.set_ylabel("CSI power (dB)")
        ax.grid(alpha=0.3)
        ax.legend(fontsize=8, ncol=2)

        # 2) heatmap RU x time
        ax = axes[row][1]
        im = ax.imshow(M.T, aspect="auto", origin="lower", cmap="viridis",
                       extent=[0, M.shape[0], -0.5, R - 0.5])
        ax.set_title("channel matrix  [RU x time]  (dB)")
        ax.set_xlabel("sample")
        ax.set_ylabel("RU index (freq ->)")
        ax.set_yticks(range(R))
        fig.colorbar(im, ax=ax, shrink=0.85, label="dB")

        # 3) CCDF per RU
        ax = axes[row][2]
        for r in range(R):
            v = np.sort(M[:, r])
            ccdf = 1.0 - np.arange(1, len(v) + 1) / len(v)
            ax.semilogy(v, np.maximum(ccdf, 1.0 / len(v)), lw=1.4, label=f"RU{r}")
        ax.set_title("CCDF of per-RU CSI (outage view)")
        ax.set_xlabel("CSI power (dB)")
        ax.set_ylabel("P(CSI > x)")
        ax.grid(alpha=0.3, which="both")
        ax.legend(fontsize=8)

    fig.tight_layout(rect=[0, 0, 1, 0.97])
    fig.savefig(args.out, dpi=140)
    print("wrote", args.out,
          "| users:", n_users,
          "| samples/user:", {k: len(v["csi"]) for k, v in users.items()})

if __name__ == "__main__":
    main()
