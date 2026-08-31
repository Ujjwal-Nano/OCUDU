#!/usr/bin/env python3
"""
plot_csi.py v2 — analysis for swap_metrics.jsonl
- uses wall-clock "t" field when present (real time axis, gaps visible)
- marks re-attach events (rnti changes) with vertical lines
- per-segment statistics: printed AND written to <out>_stats.txt
Usage: python3 plot_csi.py data.jsonl [-o out.png]
"""

import argparse, json, math, sys
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

EPS = 1e-12


def load(path):
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
                if prev.get(key) == csi:
                    continue
                prev[key] = csi
                rec = users.setdefault(key, {"rnti": [], "csi": [], "t": []})
                rec["rnti"].append(u.get("rnti", -1))
                rec["csi"].append(csi)
                rec["t"].append(d.get("t"))
    return users


def to_db(a):
    return 10.0 * np.log10(np.maximum(np.asarray(a, dtype=float), EPS))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("jsonl")
    ap.add_argument("-o", "--out", default="csi_plots.png")
    args = ap.parse_args()
    users = load(args.jsonl)
    if not users:
        sys.exit("no usable lines in " + args.jsonl)

    stats_lines = ["dataset: " + args.jsonl]
    n = len(users)
    fig, axes = plt.subplots(n, 3, figsize=(17, 4.8 * n), squeeze=False)
    fig.suptitle("SRS-derived per-RU CSI — " + args.jsonl, fontsize=13, y=0.995)

    for row, (ukey, rec) in enumerate(sorted(users.items())):
        M = to_db(rec["csi"])
        R = M.shape[1]
        rn = np.array(rec["rnti"])
        have_t = all(v is not None for v in rec["t"])
        if have_t:
            t0 = rec["t"][0]
            x = (np.array(rec["t"], dtype=float) - t0) / 60000.0
            xlabel = "time (min since start)"
        else:
            x = np.arange(M.shape[0])
            xlabel = "sample index (no t field)"

        cuts = [0] + [i for i in range(1, len(rn)) if rn[i] != rn[i - 1]] + [len(rn)]
        ax = axes[row, 0]
        for r in range(R):
            ax.plot(x, M[:, r], lw=1.0, label=f"RBG{r} (RB {12*r}-{12*r+11})")
        for i, c in enumerate(cuts[1:-1]):  # mark re-attach events (rnti change)
            xc = x[c]
            ax.axvline(
                xc,
                color="k",
                ls="--",
                lw=1,
                alpha=0.6,
                label="reconnection" if i == 0 else None,
            )
        ax.set_title(
            f"user {ukey} — per Resource Block Group(RBG) Power vs {'time' if have_t else 'sample'}"
        )
        ax.set_xlabel(xlabel)
        ax.set_ylabel("CSI power (dB)")
        ax.grid(alpha=0.3)
        ax.legend(fontsize=8, ncol=2)

        stats_lines.append(
            f"\nuser {ukey}: {len(rn)} samples, {len(cuts)-1} segment(s)"
        )
        hdr = "  seg rnti    n      " + "  ".join(
            f"RBG{r}:mean/10%out" for r in range(R)
        )
        stats_lines.append(hdr)
        for si in range(len(cuts) - 1):
            a, b = cuts[si], cuts[si + 1]
            seg = M[a:b]
            cols = []
            for r in range(R):
                v = np.sort(seg[:, r])
                out10 = v[max(0, int(0.9 * len(v)) - 1)]
                cols.append(f"{seg[:,r].mean():6.1f}/{out10:6.1f}")
            stats_lines.append(
                f"  {si:>3} {hex(rn[a]):>6} {b-a:>5}  " + "  ".join(cols)
            )

    fig.tight_layout(rect=[0, 0, 1, 0.97])
    fig.savefig(args.out, dpi=140)
    stxt = args.out.rsplit(".", 1)[0] + "_stats.txt"
    open(stxt, "w").write("\n".join(stats_lines) + "\n")
    print("\n".join(stats_lines))
    print("\nwrote", args.out, "and", stxt)


if __name__ == "__main__":
    main()
