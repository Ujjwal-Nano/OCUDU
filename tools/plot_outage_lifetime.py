#!/usr/bin/env python3
"""
plot_outage_lifetime.py — outage-based decision lifetime (no epsilon tolerance).

A decision (pick the best RBG) "dies" when THAT RBG's power drops below an
absolute outage floor P_out. P_out is set from the measured power distribution:
outage = when the (best-RBG) link power is in its worst (100-avail)% of the time.

Sweeps availability = P90, P95, P97.5, P99:
  P90  -> P_out = 10th percentile of best-RBG power (link in worst 10% = outage)
  P99  -> P_out = 1st  percentile (only the worst 1% counts as outage)
Higher availability -> lower floor -> decisions survive longer.

Panels:
  1. CCDF of decision lifetime, one curve per availability level
  2. median decision lifetime vs availability level (bar)
Also overlays the old eps-based lifetime for comparison.

Usage:
  python3 plot_outage_lifetime.py CAP.rb.jsonl[.gz] [-o out.png] [--K 12]
    [--trim-start 1.0] [--trim-end 1.0] [--reattach-guard 10] [--label NAME]
"""
import argparse, gzip, json, sys
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

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
    P, RN, Tk = P[k], RN[k], T[k]
    fs = 1000.0 / np.median(np.diff(Tk)) if len(Tk) > 1 else 100.0
    return P, fs, RN

def to_ru_db(P, K):
    R = P.shape[1] // K
    return 10*np.log10(np.maximum(
        np.stack([P[:, r*K:(r+1)*K].sum(1) for r in range(R)], 1), 1e-15))

def lifetimes_outage(M, P_out, fs):
    """Pick best RBG; decision dies when THAT RBG drops below P_out. No eps."""
    out=[]; i=0; n=len(M)
    while i < n-1:
        p = int(M[i].argmax())               # commit to best RBG now
        j = i+1
        while j < n and M[j, p] >= P_out:     # alive while chosen stays above floor
            j += 1
        out.append((j-i)/fs); i = j
    return np.array(out) if out else np.array([0.0])

def lifetimes_eps(M, eps, fs):
    """Old relative method: dies when chosen falls eps below current best."""
    best = M.max(1); out=[]; i=0; n=len(M)
    while i < n-1:
        p = int(M[i].argmax()); j = i+1
        while j < n and M[j, p] >= best[j]-eps: j += 1
        out.append((j-i)/fs); i = j
    return np.array(out) if out else np.array([0.0])

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cap"); ap.add_argument("-o","--out", default="outage_lifetime.png")
    ap.add_argument("--K", type=int, default=12)
    ap.add_argument("--trim-start", type=float, default=1.0)
    ap.add_argument("--trim-end", type=float, default=1.0)
    ap.add_argument("--reattach-guard", type=float, default=10.0)
    ap.add_argument("--label", default=None)
    a = ap.parse_args()

    P, fs, RN = load(a.cap, a.trim_start, a.trim_end)
    if a.reattach_guard > 0 and len(RN) > 1:
        g = int(round(a.reattach_guard*fs)); keep = np.ones(len(RN), bool)
        for c in [i for i in range(1,len(RN)) if RN[i]!=RN[i-1]]:
            keep[max(0,c-g):min(len(RN),c+g)] = False
        if keep.sum() > 100: P = P[keep]

    M = to_ru_db(P, a.K); R = M.shape[1]
    label = a.label if a.label else a.cap.split("/")[-1]
    best = M.max(1)   # best-RBG power series -> defines outage floor percentiles

    # availability levels -> percentile of best-RBG power used as outage floor
    levels = [("P90", 10), ("P95", 5), ("P97.5", 2.5), ("P99", 1)]
    colors = ["#1f77b4", "#2ca02c", "#ff7f0e", "#d62728"]

    L = [f"dataset: {label}   K={a.K} -> {R} RBGs   samples={len(M)}   fs={fs:.1f}/s",
         f"best-RBG power: median {np.median(best):.1f}  P10 {np.percentile(best,10):.1f}  "
         f"P1 {np.percentile(best,1):.1f} dB"]

    fig, ax = plt.subplots(1, 2, figsize=(13, 5))

    med_life = []
    for (name, pct), col in zip(levels, colors):
        P_out = np.percentile(best, pct)
        lt = lifetimes_outage(M, P_out, fs)
        med = np.median(lt); med_life.append(med)
        li = np.sort(lt); cc = 1 - np.arange(1, len(li)+1)/len(li)
        ax[0].loglog(np.maximum(li, 1/fs), np.maximum(cc, 1/len(li)), lw=1.8, color=col,
                     label=f"{name} (floor {P_out:.1f} dB, med {med:.3f}s)")
        L.append(f"outage {name:>6} (floor {P_out:6.1f} dB): median lifetime {med:.4f} s  "
                 f"p10 {np.percentile(lt,10):.4f}  p90 {np.percentile(lt,90):.4f} s")

    # overlay old eps=0.5 for comparison
    lt_e = lifetimes_eps(M, 0.5, fs)
    li = np.sort(lt_e); cc = 1 - np.arange(1,len(li)+1)/len(li)
    ax[0].loglog(np.maximum(li,1/fs), np.maximum(cc,1/len(li)), lw=1.5, ls="--",
                 color="gray", label=f"old ε=0.5dB (med {np.median(lt_e):.3f}s)")
    L.append(f"old eps=0.5 dB: median lifetime {np.median(lt_e):.4f} s")

    ax[0].set_xlabel("decision lifetime (s)"); ax[0].set_ylabel("P(lifetime > x)")
    ax[0].set_title("Outage-based decision lifetime CCDF"); ax[0].grid(alpha=0.3, which="both")
    ax[0].legend(fontsize=7.5)

    # panel 2: median lifetime vs availability
    names = [n for n,_ in levels]
    bars = ax[1].bar(names, med_life, color=colors, alpha=0.85)
    for b, m in zip(bars, med_life):
        ax[1].text(b.get_x()+b.get_width()/2, m, f"{m*1000:.0f}ms", ha="center", va="bottom", fontsize=9)
    ax[1].set_ylabel("median decision lifetime (s)")
    ax[1].set_xlabel("availability target")
    ax[1].set_title("Median lifetime vs availability")
    ax[1].grid(alpha=0.3, axis="y")

    fig.suptitle(f"Outage-based decision lifetime — {label}", fontsize=12)
    fig.tight_layout(rect=[0,0,1,0.95]); fig.savefig(a.out, dpi=120)
    txt = a.out.rsplit(".",1)[0]+"_outage.txt"
    open(txt,"w").write("\n".join(L)+"\n")
    print("\n".join(L)); print("wrote", a.out, "and", txt)

if __name__ == "__main__":
    main()
