#!/usr/bin/env python3
"""swap_analysis.py — multi-user swap benefit (Option C).

Reads the scheduler's swap_metrics.jsonl (per-slot, per-user CSI + achieved
assignment) and reports, in the SAME units (b/s/Hz):

  FIXED     : best single STATIC assignment held all run (fair no-adaptation)
  ACHIEVED  : what your swap actually delivered to the weakest user (served)
  ACHIEVABLE: true max-min optimum each slot (brute force) -> a valid ceiling

  realized gain  = ACHIEVED - FIXED      (what the swap won vs best static split)
  gap-to-optimal = ACHIEVABLE - ACHIEVED (what the swap left on the table)

Plus per-RU cross-user correlation (low = swap opportunity).

No time-alignment needed: the swap logs all users on one slot boundary.

Usage:
  python3 swap_analysis.py /tmp/swap_metrics.jsonl -o out.png [--snr0 10] [--rus-per-user 1]
"""
import argparse, json, sys, itertools
from collections import defaultdict
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

EPS = 1e-12


def load(path):
    slots = []
    R = None
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                d = json.loads(line)
            except json.JSONDecodeError:
                continue
            us = d.get("users", [])
            if not us:
                continue
            recs, ok = [], True
            for u in us:
                csi = u.get("csi")
                if not csi:
                    ok = False; break
                if R is None:
                    R = len(csi)
                if len(csi) != R or max(csi) <= 1e-8:
                    ok = False; break
                recs.append({"u": u.get("u"), "csi": np.asarray(csi, float),
                             "served": u.get("served", 0.0),
                             "rus": list(u.get("rus", []))})
            if ok and recs:
                slots.append({"t": d.get("t"), "users": recs})
    return slots, (R or 0)


def rate_of_rus(csi, rus, scale):
    if not len(rus):
        return 0.0
    return float(np.sum(np.log2(1.0 + np.maximum(scale * csi[list(rus)], 0.0))))


def weakest_rate(csis, assignment, scale):
    rates = [rate_of_rus(csis[u], assignment[u], scale) for u in range(len(csis))]
    active = [r for r, a in zip(rates, assignment) if len(a)]
    return min(active) if active else 0.0


def optimal_maxmin(csis, scale):
    """True max-min optimum this slot by brute force (one distinct RU per user)."""
    U, R = len(csis), len(csis[0])
    best_min, best = -1.0, None
    for perm in itertools.permutations(range(R), U):
        m = min(rate_of_rus(csis[u], [perm[u]], scale) for u in range(U))
        if m > best_min:
            best_min, best = m, perm
    return [[r] for r in best]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("swaplog")
    ap.add_argument("-o", "--out", default="swap_analysis.png")
    ap.add_argument("--snr0", type=float, default=10.0)
    ap.add_argument("--rus-per-user", type=int, default=1)
    ap.add_argument("--smooth", type=int, default=50)
    a = ap.parse_args()

    slots, R = load(a.swaplog)
    if not slots:
        sys.exit("no usable slots in " + a.swaplog)
    U = max(len(s["users"]) for s in slots)

    allc = np.concatenate([u["csi"] for s in slots for u in s["users"]])
    med = np.median(allc[allc > 0]) if np.any(allc > 0) else 1.0
    scale = (10 ** (a.snr0 / 10.0)) / max(med, EPS)

    ach, ceil, ts = [], [], []
    slot_csis = []
    ru_series = defaultdict(lambda: defaultdict(list))
    for s in slots:
        us = s["users"]
        csis = [u["csi"] for u in us]
        slot_csis.append(csis)
        a_slot = min([rate_of_rus(u["csi"], u["rus"], scale)
                      for u in us if len(u["rus"])] or [0.0])
        ach.append(a_slot)
        c_opt = weakest_rate(csis, optimal_maxmin(csis, scale), scale)
        ceil.append(max(c_opt, a_slot))     # clamp float noise
        ts.append(s["t"])
        for u in us:
            for r in range(R):
                ru_series[r][u["u"]].append(u["csi"][r])

    # best fixed static split over the whole run
    U_full = max(len(c) for c in slot_csis)
    best_perm, best_mean = None, -1.0
    for perm in itertools.permutations(range(R), U_full):
        per = []
        for csis in slot_csis:
            if len(csis) != U_full:
                continue
            per.append(min(rate_of_rus(csis[u], [perm[u]], scale) for u in range(U_full)))
        if per and np.mean(per) > best_mean:
            best_mean, best_perm = np.mean(per), perm
    fix = []
    for csis in slot_csis:
        if len(csis) == U_full and best_perm is not None:
            fix.append(min(rate_of_rus(csis[u], [best_perm[u]], scale) for u in range(U_full)))
        else:
            fix.append(np.nan)

    ach, fix, ceil = np.array(ach), np.array(fix), np.array(ceil)

    def sm(x, w):
        x = np.asarray(x, float)
        return x if (w <= 1 or len(x) < w) else np.convolve(x, np.ones(w)/w, mode="same")

    # per-RU cross-user correlation (log CSI)
    uids = sorted({u["u"] for s in slots for u in s["users"]})
    percorr = {}
    for r in range(R):
        series = {u: np.array(ru_series[r][u]) for u in uids if len(ru_series[r][u]) > 5}
        vals = []
        for ui, uj in itertools.combinations(series, 2):
            n = min(len(series[ui]), len(series[uj]))
            aa = np.log10(np.maximum(series[ui][:n], EPS))
            bb = np.log10(np.maximum(series[uj][:n], EPS))
            if aa.std() > 1e-9 and bb.std() > 1e-9:
                vals.append(np.corrcoef(aa, bb)[0, 1])
        percorr[r] = float(np.mean(vals)) if vals else float("nan")

    fig = plt.figure(figsize=(15, 9))
    gs = fig.add_gridspec(2, 2, height_ratios=[1.4, 1])
    axT = fig.add_subplot(gs[0, :])
    x = (np.array(ts, float) - ts[0]) / 1000.0 if all(t is not None for t in ts) else np.arange(len(ts))
    axT.plot(x, sm(fix, a.smooth), label="fixed (best static split)", color="tab:red", lw=1.3)
    axT.plot(x, sm(ach, a.smooth), label="achieved (your swap)", color="tab:blue", lw=1.6)
    axT.plot(x, sm(ceil, a.smooth), label="achievable (max-min ceiling)", color="tab:green", lw=1.3, ls="--")
    axT.set_xlabel("time (s)"); axT.set_ylabel("weakest-user rate (b/s/Hz)")
    axT.set_title(f"Weakest-user rate: fixed vs achieved vs ceiling (snr0={a.snr0:.0f} dB)")
    axT.legend(); axT.grid(alpha=0.3)

    axG = fig.add_subplot(gs[1, 0])
    axG.bar(["fixed", "achieved", "ceiling"], [fix.mean(), ach.mean(), ceil.mean()],
            color=["tab:red", "tab:blue", "tab:green"], alpha=0.8)
    realized, gap = ach.mean() - fix.mean(), ceil.mean() - ach.mean()
    axG.set_ylabel("mean weakest-user rate (b/s/Hz)")
    axG.set_title(f"realized gain = {realized:+.3f}  |  gap-to-optimal = {gap:+.3f}")
    axG.grid(alpha=0.3, axis="y")

    axC = fig.add_subplot(gs[1, 1])
    rs = list(range(R)); cv = [percorr[r] for r in rs]
    cols = ["tab:green" if (not np.isnan(v) and v < 0.2) else
            ("tab:red" if (not np.isnan(v) and v > 0.5) else "tab:orange") for v in cv]
    axC.bar(rs, [0 if np.isnan(v) else v for v in cv], color=cols)
    axC.axhline(0.5, color="k", ls="--", lw=0.7, alpha=0.5)
    axC.set_ylim(-1, 1); axC.set_xticks(rs); axC.set_xticklabels([f"RU{r}" for r in rs])
    axC.set_ylabel("mean cross-user corr")
    axC.set_title("Per-RU cross-user correlation\n(low=opportunity, high=contention)")
    axC.grid(alpha=0.3, axis="y")

    fig.suptitle("Multi-user swap benefit — " + a.swaplog.split("/")[-1], fontsize=13)
    fig.tight_layout(rect=[0, 0, 1, 0.97])
    fig.savefig(a.out, dpi=140)
    plt.close(fig)

    txt = a.out.rsplit(".", 1)[0] + ".txt"
    with open(txt, "w") as f:
        f.write(f"swap_analysis — {a.swaplog}\nslots={len(slots)} users={U} RUs={R} snr0={a.snr0}\n")
        f.write(f"best static split (fixed): user->RU {best_perm}\n\n")
        f.write("WEAKEST-USER RATE (b/s/Hz), mean:\n")
        f.write(f"  fixed    : {fix.mean():.4f}\n  achieved : {ach.mean():.4f}\n  ceiling  : {ceil.mean():.4f}\n")
        f.write(f"  realized gain  : {realized:+.4f} ({100*realized/max(fix.mean(),EPS):+.1f}%)\n")
        f.write(f"  gap-to-optimal : {gap:+.4f}\n")
        f.write("per-RU cross-user corr: " + ", ".join(f"RU{r}={percorr[r]:+.2f}" for r in range(R)) + "\n")

    rel = 100 * realized / max(fix.mean(), EPS)
    cap = 100 * realized / max(ceil.mean() - fix.mean(), EPS)
    print(f"slots={len(slots)} users={U} RUs={R}")
    print(f"weakest-user rate  fixed={fix.mean():.4f}  achieved={ach.mean():.4f}  ceiling={ceil.mean():.4f} b/s/Hz")
    print(f"realized gain={realized:+.4f} b/s/Hz ({rel:+.1f}%)  gap-to-optimal={gap:+.4f}  capture={cap:.0f}% of available")
    print(f"per-RU cross-user corr: " + ", ".join(f"RU{r}={percorr[r]:+.2f}" for r in range(R)))
    print("wrote", a.out, "and", txt)


if __name__ == "__main__":
    main()
