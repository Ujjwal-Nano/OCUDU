#!/usr/bin/env python3
"""swap_decision_quality.py — how GOOD and how FAST each swap decision is.

Separates the SCHEDULER'S quality from the channel's limits. From swap.jsonl:

  OPTIMALITY : per slot, compare the swap's achieved weakest-user floor to the
               TRUE max-min optimum (brute force over all user->RU permutations).
               gap_to_optimum = 10log10(optimum / achieved)  (dB, >= 0)
               -> % of slots that are exactly optimal, and the gap distribution.
  CAPTURED   : of the gain available between the best FIXED split and the
               per-slot optimum, what fraction did the swap take.
  TIMING     : alloc_us per decision (the 'lightweight' claim, on real HW).

Two-panel figure: (left) gap-to-optimum CDF, (right) alloc-time CDF.
Power only; no rate modelling.

Usage:
  python3 swap_decision_quality.py swap.jsonl -o quality.png [--min-users 3]
"""
import argparse, json, sys, itertools
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

EPS = 1e-12


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("swaplog")
    ap.add_argument("-o", "--out", default="quality.png")
    ap.add_argument("--min-users", type=int, default=3)
    ap.add_argument("--label", default=None)
    a = ap.parse_args()

    slots, alloc = [], []
    for ln in open(a.swaplog):
        ln = ln.strip()
        if not ln:
            continue
        try:
            d = json.loads(ln)
        except json.JSONDecodeError:
            continue
        if "alloc_us" in d:
            alloc.append(float(d["alloc_us"]))
        real = [u for u in d.get("users", [])
                if u.get("rnti", 0) != 0 and max(u.get("csi", [0])) > 1e-7]
        if len(real) >= a.min_users:
            slots.append(real)
    if not slots:
        sys.exit("no usable slots")
    U = min(len(s) for s in slots)
    R = len(slots[0][0]["csi"])

    ach, csis = [], []
    for us in slots:
        us = us[:U]
        ach.append(min(u["served"] for u in us))
        csis.append([np.asarray(u["csi"], float) for u in us])
    ach = np.array(ach)

    perms = list(itertools.permutations(range(R), U))
    ceil = np.array([max(min(c[u][p[u]] for u in range(U)) for p in perms) for c in csis])
    gap_db = 10 * np.log10(np.maximum(ceil, EPS) / np.maximum(ach, EPS))
    gap_db = np.maximum(gap_db, 0.0)
    opt_pct = 100.0 * np.mean(gap_db < 1e-3)

    # weakest-on-own-best-RU
    onbest = 0
    for us in slots:
        us = us[:U]
        w = min(us, key=lambda u: u["served"])
        if int(np.argmax(w["csi"])) == w["rus"][0]:
            onbest += 1
    onbest_pct = 100.0 * onbest / len(slots)

    # captured-gain ratio vs best fixed
    best_perm = max(perms, key=lambda p: np.mean([min(c[u][p[u]] for u in range(U)) for c in csis]))
    fix = np.array([min(c[u][best_perm[u]] for u in range(U)) for c in csis])
    fm, am, cm = (10*np.log10(np.maximum(x, EPS)).mean() for x in (fix, ach, ceil))
    captured = (am - fm) / max(cm - fm, 1e-9) * 100.0

    alloc = np.array(alloc) if alloc else np.array([np.nan])

    def cdf(x):
        xs = np.sort(x); return xs, np.arange(1, len(xs)+1)/len(xs)

    fig, ax = plt.subplots(1, 2, figsize=(15, 5.5))

    # LEFT: gap-to-optimum CDF
    gx, gy = cdf(gap_db)
    ax[0].plot(gx, gy, color="tab:green", lw=2.4)
    ax[0].axvline(0, color="k", ls=":", lw=0.8, alpha=0.5)
    ax[0].set_xlabel("gap to per-slot max-min optimum (dB)", fontsize=13)
    ax[0].set_ylabel("CDF — fraction of slots", fontsize=13)
    ax[0].set_title(f"Decision optimality\n{opt_pct:.0f}% of slots are exactly optimal · "
                    f"mean gap {gap_db.mean():.3f} dB", fontsize=13)
    ax[0].grid(alpha=0.3)
    ax[0].set_xlim(-0.1, max(0.6, np.percentile(gap_db, 99)))
    ax[0].annotate(f"{opt_pct:.0f}% optimal\n(gap = 0)", xy=(0.02, opt_pct/100),
                   xytext=(0.18, 0.55), fontsize=12, color="tab:green", fontweight="bold",
                   arrowprops=dict(arrowstyle="->", color="tab:green"))

    # RIGHT: alloc-time CDF
    if np.isfinite(alloc).any():
        ax_, ay = cdf(alloc[np.isfinite(alloc)])
        ax[1].plot(ax_, ay, color="tab:blue", lw=2.4)
        mean_us, p99 = np.nanmean(alloc), np.nanpercentile(alloc, 99)
        ax[1].axvline(mean_us, color="k", ls="--", lw=1, label=f"mean {mean_us:.1f} µs")
        ax[1].axvline(p99, color="tab:red", ls=":", lw=1.2, label=f"p99 {p99:.1f} µs")
        ax[1].set_xlabel("allocation time per decision (µs)", fontsize=13)
        ax[1].set_ylabel("CDF — fraction of decisions", fontsize=13)
        ax[1].set_title(f"Decision cost (lightweight)\nmean {mean_us:.1f} µs · p99 {p99:.1f} µs — "
                        f"on real hardware", fontsize=13)
        ax[1].grid(alpha=0.3); ax[1].legend(fontsize=12, loc="lower right")
        ax[1].set_xlim(0, p99 * 1.4)
    else:
        ax[1].text(0.5, 0.5, "no alloc_us in log", ha="center", va="center")
        ax[1].axis("off")

    fig.suptitle("Swap decision quality — near-optimal AND lightweight (power only)", fontsize=14)
    fig.tight_layout(rect=[0, 0, 1, 0.95])
    fig.savefig(a.out, dpi=140); plt.close(fig)

    txt = a.out.rsplit(".", 1)[0] + ".txt"
    with open(txt, "w") as f:
        lbl = a.label or a.swaplog.split("/")[-1]
        f.write(f"swap decision quality — {lbl}\nslots={len(slots)} users={U} RUs={R}\n\n")
        f.write(f"OPTIMALITY: {opt_pct:.1f}% of slots hit the true max-min optimum\n")
        f.write(f"  gap-to-optimum: mean {gap_db.mean():.3f} dB, p90 {np.percentile(gap_db,90):.2f} dB, "
                f"max {gap_db.max():.2f} dB\n")
        f.write(f"WEAKEST ON OWN BEST RU: {onbest_pct:.1f}% of slots\n")
        f.write(f"CAPTURED-GAIN RATIO: {captured:.0f}% "
                f"(best-fixed {fm:.2f} -> swap {am:.2f} -> ceiling {cm:.2f} dB)\n")
        if np.isfinite(alloc).any():
            f.write(f"ALLOC TIME: mean {np.nanmean(alloc):.2f} us, p99 {np.nanpercentile(alloc,99):.2f} us, "
                    f"max {np.nanmax(alloc):.2f} us\n")
    print(f"optimality={opt_pct:.1f}%  weakest-on-best={onbest_pct:.1f}%  captured={captured:.0f}%  "
          f"mean-gap={gap_db.mean():.3f}dB  alloc_mean={np.nanmean(alloc):.2f}us")
    print("wrote", a.out, "and", txt)


if __name__ == "__main__":
    main()
