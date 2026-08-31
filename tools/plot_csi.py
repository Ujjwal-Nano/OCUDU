#!/usr/bin/env python3
import argparse, json, sys
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
EPS = 1e-12

def load(path):
    users = {}; prev = {}
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line: continue
            try: d = json.loads(line)
            except json.JSONDecodeError: continue
            for u in d.get("users", []):
                key = u.get("u", 0); csi = u.get("csi")
                if csi is None: continue
                if prev.get(key) == csi: continue
                prev[key] = csi
                rec = users.setdefault(key, {"rnti": [], "csi": [], "t": []})
                rec["rnti"].append(u.get("rnti", -1)); rec["csi"].append(csi); rec["t"].append(d.get("t"))
    return users

def to_db(a):
    return 10.0 * np.log10(np.maximum(np.asarray(a, dtype=float), EPS))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("jsonl")
    ap.add_argument("-o", "--out", default="csi_plots.png")
    ap.add_argument("--zoom-start", type=float, default=None)
    ap.add_argument("--zoom-dur", type=float, default=20.0)
    ap.add_argument("--settle-guard", type=float, default=2.0)
    args = ap.parse_args()
    users = load(args.jsonl)
    if not users: sys.exit("no usable lines in " + args.jsonl)
    stats_lines = ["dataset: " + args.jsonl]
    n = len(users)
    fig, axes = plt.subplots(n, 2, figsize=(17, 4.8 * n), squeeze=False)
    fig.suptitle("SRS-derived per-RU CSI — " + args.jsonl, fontsize=13, y=0.995)
    for row, (ukey, rec) in enumerate(sorted(users.items())):
        M = to_db(rec["csi"]); R = M.shape[1]; rn = np.array(rec["rnti"])
        have_t = all(v is not None for v in rec["t"])
        if have_t:
            t0 = rec["t"][0]; tmin = (np.array(rec["t"], dtype=float) - t0) / 60000.0
            xlabel = "time (min since start)"
        else:
            tmin = np.arange(M.shape[0]) / 6000.0; xlabel = "sample index"
        cuts = [0] + [i for i in range(1, len(rn)) if rn[i] != rn[i-1]] + [len(rn)]
        best = M.argmax(1)
        if args.zoom_start is None and have_t:
            settle = args.settle_guard
            t_lo, t_hi = tmin[0] + settle, tmin[-1] - settle - args.zoom_dur/60.0
            if t_hi <= t_lo: t_lo, t_hi = tmin[0], tmin[-1] - args.zoom_dur/60.0
            dur_min = args.zoom_dur/60.0; best_start, best_sw = t_lo, -1
            step = max(1, len(tmin)//200)
            for i in range(0, len(tmin), step):
                t_s = tmin[i]
                if t_s < t_lo or t_s > t_hi: continue
                w = (tmin >= t_s) & (tmin < t_s + dur_min)
                if w.sum() < 5: continue
                sw = (np.diff(best[w]) != 0).sum()
                if sw > best_sw: best_sw, best_start = sw, t_s
            zstart = best_start
        else:
            zstart = args.zoom_start if args.zoom_start is not None else tmin[0]
        zend = zstart + args.zoom_dur/60.0
        axL = axes[row, 0]
        for r in range(R):
            axL.plot(tmin, M[:, r], lw=0.8, label=f"RU{r} (RB {12*r}-{12*r+11})")
        for i, c in enumerate(cuts[1:-1]):
            axL.axvline(tmin[c], color="k", ls="--", lw=1, alpha=0.6, label="reconnection" if i == 0 else None)
        axL.axvspan(zstart, zend, color="gray", alpha=0.15, label="zoom window")
        axL.set_title(f"user {ukey} — full capture")
        axL.set_xlabel(xlabel); axL.set_ylabel("CSI power (dB)")
        axL.grid(alpha=0.3); axL.legend(fontsize=8, ncol=2)
        axR = axes[row, 1]
        wz = (tmin >= zstart) & (tmin < zend)
        tz = (tmin[wz] - zstart) * 60.0
        for r in range(R):
            axR.plot(tz, M[wz, r], lw=1.1, marker=".", ms=2, label=f"RU{r}")
        bz = best[wz]
        axR.set_title(f"user {ukey} — zoom {args.zoom_dur:.0f}s @ {zstart:.1f} min ({(np.diff(bz)!=0).sum()} switches)")
        axR.set_xlabel("time within window (s)"); axR.set_ylabel("CSI power (dB)")
        axR.grid(alpha=0.3); axR.legend(fontsize=8, ncol=2)
        stats_lines.append(f"\nuser {ukey}: {len(rn)} samples, {len(cuts)-1} segment(s)")
        frac = {r: 100*(best==r).mean() for r in range(R)}
        stats_lines.append("  best-RU fraction: " + ", ".join(f"RU{r}={frac[r]:.1f}%" for r in range(R)))
        stats_lines.append(f"  total switches: {(np.diff(best)!=0).sum()}   zoom: {zstart:.2f}-{zend:.2f} min")
    fig.tight_layout(rect=[0, 0, 1, 0.97])
    fig.savefig(args.out, dpi=140)
    stxt = args.out.rsplit(".", 1)[0] + "_stats.txt"
    open(stxt, "w").write("\n".join(stats_lines) + "\n")
    print("\n".join(stats_lines)); print("\nwrote", args.out)

if __name__ == "__main__":
    main()
