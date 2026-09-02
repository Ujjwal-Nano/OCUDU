#!/usr/bin/env python3
"""
plot_rbg_power.py — RBG power distribution & swapping-benefit figure.
3 panels:
  1. Per-RBG power distribution (box plot over the whole capture)
  2. Best-RBG fraction (what % of time each RBG is strongest) — "fair distribution"
  3. Gain-from-swapping distribution (best - average power, dB) — swapping benefit
Modular in K (RBs per RBG).
Usage:
  python3 plot_rbg_power.py CAP.rb.jsonl[.gz] [-o out.png] [--K 12]
    [--trim-start 1.0] [--trim-end 1.0] [--reattach-guard 10]
"""
import argparse, gzip, json, sys
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

RBBW = 0.36e6

def load(path, trim_start, trim_end):
    op = gzip.open if path.endswith(".gz") else open
    T, P, RN = [], [], []
    with op(path, "rt") as f:
        for line in f:
            line = line.strip()
            if not line: continue
            try: d = json.loads(line)
            except json.JSONDecodeError: continue
            T.append(d["t"]); P.append(d["rb"]); RN.append(d.get("rnti", -1))
    if not P: sys.exit("no samples in " + path)
    T = np.array(T, float); P = np.array(P, float); RN = np.array(RN)
    P = P[:, 1:]
    mins = (T - T[0]) / 60000.0
    k = (mins >= trim_start) & (mins <= mins[-1] - trim_end)
    if k.sum() < 100: k = np.ones(len(mins), bool)
    P, RN = P[k], RN[k]
    fs = 1000.0 / np.median(np.diff(T[k])) if k.sum() > 1 else 100.0
    return P, fs, RN

def to_ru_db(P, K):
    R = P.shape[1] // K
    return 10*np.log10(np.maximum(
        np.stack([P[:, r*K:(r+1)*K].sum(1) for r in range(R)], 1), 1e-15))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cap"); ap.add_argument("-o","--out", default="rbg_power.png")
    ap.add_argument("--K", type=int, default=12)
    ap.add_argument("--trim-start", type=float, default=1.0)
    ap.add_argument("--trim-end", type=float, default=1.0)
    ap.add_argument("--reattach-guard", type=float, default=10.0)
    ap.add_argument("--label", default=None)
    a = ap.parse_args()

    P, fs, RN = load(a.cap, a.trim_start, a.trim_end)
    # re-attach guard
    if a.reattach_guard > 0 and len(RN) > 1:
        g = int(round(a.reattach_guard*fs))
        keep = np.ones(len(RN), bool)
        for c in [i for i in range(1,len(RN)) if RN[i]!=RN[i-1]]:
            keep[max(0,c-g):min(len(RN),c+g)] = False
        if keep.sum() > 100: P = P[keep]

    M = to_ru_db(P, a.K)          # (time, R) per-RBG power dB
    R = M.shape[1]
    label = a.label if a.label else a.cap.split("/")[-1]

    best = M.argmax(1)            # which RBG is best each sample
    best_frac = np.array([100*(best==r).mean() for r in range(R)])
    gain = M.max(1) - M.mean(1)   # dB gain of best over average, per sample

    fig, ax = plt.subplots(1, 3, figsize=(16, 5))

    # Panel 1: per-RBG power distribution (box plot)
    ax[0].boxplot([M[:,r] for r in range(R)], labels=[f"RBG{r}" for r in range(R)],
                  showfliers=False, patch_artist=True,
                  boxprops=dict(facecolor="#8fb3e0"), medianprops=dict(color="k"))
    ax[0].set_ylabel("RBG power (dB)")
    ax[0].set_title(f"Per-RBG power distribution (K={a.K}, {R} RBGs)")
    ax[0].grid(alpha=0.3, axis="y")

    # Panel 2: best-RBG fraction (bar)
    bars = ax[1].bar([f"RBG{r}" for r in range(R)], best_frac,
                     color="#d62728", alpha=0.8)
    for b, f in zip(bars, best_frac):
        ax[1].text(b.get_x()+b.get_width()/2, f+0.5, f"{f:.0f}%",
                   ha="center", fontsize=9)
    ax[1].axhline(100/R, ls="--", c="k", lw=1, label=f"uniform ({100/R:.0f}%)")
    ax[1].set_ylabel("% of time this RBG is best")
    ax[1].set_title("Best-RBG fraction (swapping picks these)")
    ax[1].set_ylim(0, max(best_frac)*1.2); ax[1].grid(alpha=0.3, axis="y"); ax[1].legend(fontsize=8)

    # Panel 3: gain-from-swapping distribution
    ax[2].hist(gain, bins=50, color="#2ca02c", alpha=0.75, density=True)
    ax[2].axvline(gain.mean(), color="k", ls="--", lw=1.5,
                  label=f"mean gain G={gain.mean():.2f} dB")
    ax[2].axvline(np.percentile(gain,90), color="r", ls=":", lw=1.5,
                  label=f"90th pct={np.percentile(gain,90):.2f} dB")
    ax[2].set_xlabel("best - average RBG power (dB)")
    ax[2].set_ylabel("density")
    ax[2].set_title("Gain from swapping (per instant)")
    ax[2].grid(alpha=0.3); ax[2].legend(fontsize=8)

    fig.suptitle(f"RBG power & swapping benefit — {label}", fontsize=12)
    fig.tight_layout(rect=[0,0,1,0.95])
    fig.savefig(a.out, dpi=120)

    # text summary
    txt = a.out.rsplit(".",1)[0]+"_power.txt"
    L=[f"dataset: {label}   K={a.K} -> {R} RBGs   samples={len(M)}",
       "best-RBG fraction: " + ", ".join(f"RBG{r}={best_frac[r]:.1f}%" for r in range(R)),
       f"gain from swapping: mean={gain.mean():.2f} dB  median={np.median(gain):.2f} dB  "
       f"p90={np.percentile(gain,90):.2f} dB  p10={np.percentile(gain,10):.2f} dB",
       "per-RBG median power (dB): " + ", ".join(f"RBG{r}={np.median(M[:,r]):.1f}" for r in range(R))]
    open(txt,"w").write("\n".join(L)+"\n")
    print("\n".join(L)); print("wrote", a.out, "and", txt)

if __name__ == "__main__":
    main()
