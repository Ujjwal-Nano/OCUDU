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


def assignments(U, R, rpu):
    """All ways to give each of U users a distinct set of `rpu` RUs out of R.
    For rpu==1 this reduces to permutations(range(R), U)."""
    seen, out = set(), []
    for combo in itertools.permutations(range(R), U * rpu):
        grp = tuple(tuple(sorted(combo[i * rpu : (i + 1) * rpu])) for i in range(U))
        if grp in seen:
            continue
        seen.add(grp)
        out.append([list(g) for g in grp])
    return out


def floor_of(asg, c, U):
    """Weakest-user floor for one slot: min over users of sum-of-CSI on their RUs."""
    return min(sum(c[u][r] for r in asg[u]) for u in range(U))


def floor_series_C(C, asg, U):
    """Weakest-user floor over all slots for ONE assignment -> (n_slots,)."""
    served = np.stack([C[:, u, asg[u]].sum(axis=1) for u in range(U)], axis=1)
    return served.min(axis=1)


def stream_best_opt(C, asgs, U):
    """Memory-safe pass: per-assignment mean floor + per-slot optimum.

    Replaces the full (n_assignments x n_slots) float64 matrix, which is
    ~1.1 GB at U=4 / R=8 and OOM-killed the process. Keeps only the running
    per-slot optimum and the per-assignment means (peak memory O(n_slots)); the
    best-fixed series is recomputed afterwards for the single winning
    assignment.
    """
    n_slots = C.shape[0]
    opt = np.full(n_slots, -np.inf)
    means = np.empty(len(asgs))
    for i, asg in enumerate(asgs):
        fl = floor_series_C(C, asg, U)
        means[i] = fl.mean()
        np.maximum(opt, fl, out=opt)
    return means, opt


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("swaplog")
    ap.add_argument("-o", "--out", default="quality.png")
    ap.add_argument("--min-users", type=int, default=3)
    ap.add_argument(
        "--rus-per-user",
        type=int,
        default=None,
        help="RUs owned per user; default: read from log, else 1 "
        "(R>=2 brute-forces group assignments)",
    )
    ap.add_argument(
        "--epsilon",
        type=float,
        default=None,
        help="swap ε (worst RUs reconsidered each period) shown in the title; "
        "default: read from log if present",
    )
    ap.add_argument("--label", default=None)
    ap.add_argument(
        "--alloc-min", type=float, default=None, help="alloc-time panel x-axis min (us)"
    )
    ap.add_argument(
        "--alloc-max",
        type=float,
        default=15.0,
        help="alloc-time panel x-axis max floor (us)",
    )
    a = ap.parse_args()

    slots, alloc, opt_alloc = [], [], []
    meta = {"policy": None, "rus_per_user": None, "epsilon": None}
    for ln in open(a.swaplog):
        ln = ln.strip()
        if not ln:
            continue
        try:
            d = json.loads(ln)
        except json.JSONDecodeError:
            continue
        if meta["policy"] is None and d.get("policy") is not None:
            meta["policy"] = str(d["policy"])
        if meta["rus_per_user"] is None and d.get("rus_per_user") is not None:
            meta["rus_per_user"] = int(d["rus_per_user"])
        if meta["epsilon"] is None:
            ev = d.get("epsilon", d.get("eps"))
            if ev is not None:
                meta["epsilon"] = float(ev)
        if "alloc_us" in d:
            alloc.append(float(d["alloc_us"]))
        # optimal (exhaustive) decision time, logged live when present (>=0);
        # -1 or absent means it was skipped / an old capture -> ignored.
        if "opt_alloc_us" in d and float(d["opt_alloc_us"]) >= 0:
            opt_alloc.append(float(d["opt_alloc_us"]))
        real = [
            u
            for u in d.get("users", [])
            if u.get("rnti", 0) != 0 and max(u.get("csi", [0])) > 1e-7
        ]
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

    # RU granularity: CLI wins, else what the gNB logged, else 1 (old captures)
    if a.rus_per_user is not None:
        rpu = a.rus_per_user
    elif meta["rus_per_user"]:
        rpu = meta["rus_per_user"]
    else:
        rpu = 1
    eps = a.epsilon if a.epsilon is not None else meta["epsilon"]
    pol = meta["policy"]
    if U * rpu > R:
        sys.exit("need U*rus_per_user=%d <= R=%d RUs" % (U * rpu, R))
    asgs = assignments(U, R, rpu)
    C = np.asarray(csis, float)  # (n_slots, U, R)
    means, ceil = stream_best_opt(C, asgs, U)  # memory-safe; ceil=per-slot optimum
    gap_db = 10 * np.log10(np.maximum(ceil, EPS) / np.maximum(ach, EPS))
    gap_db = np.maximum(gap_db, 0.0)
    opt_pct = 100.0 * np.mean(gap_db < 1e-3)

    # weakest-on-own-best-RU  (R>=2: weakest user holds its own best rpu RUs)
    onbest = 0
    for us in slots:
        us = us[:U]
        w = min(us, key=lambda u: u["served"])
        best_rus = set(int(i) for i in np.argsort(w["csi"])[::-1][:rpu])
        if set(int(r) for r in w["rus"]) == best_rus:
            onbest += 1
    onbest_pct = 100.0 * onbest / len(slots)

    # captured-gain ratio vs best fixed
    fix = floor_series_C(C, asgs[int(means.argmax())], U)
    fm, am, cm = (10 * np.log10(np.maximum(x, EPS)).mean() for x in (fix, ach, ceil))
    captured = (am - fm) / max(cm - fm, 1e-9) * 100.0

    alloc = np.array(alloc) if alloc else np.array([np.nan])
    opt_alloc = np.array(opt_alloc) if opt_alloc else np.array([])

    def cdf(x):
        xs = np.sort(x)
        return xs, np.arange(1, len(xs) + 1) / len(xs)

    fig, ax = plt.subplots(1, 2, figsize=(15, 5.5))

    # LEFT: gap-to-optimum CDF
    gx, gy = cdf(gap_db)
    ax[0].plot(gx, gy, color="tab:green", lw=2.4)
    ax[0].axvline(0, color="k", ls=":", lw=0.8, alpha=0.5)
    ax[0].set_xlabel("gap to per-slot max-min optimum (dB)", fontsize=13)
    ax[0].set_ylabel("CDF : fraction of slots", fontsize=13)
    ax[0].set_title(f"Decision optimality", fontsize=13)
    ax[0].grid(alpha=0.3)
    ax[0].set_xlim(-0.001, max(0.6, np.percentile(gap_db, 99)))
    ax[0].set_ylim(0, 1.0)
    ax[0].annotate(
        f"{opt_pct:.0f}% optimal\n(gap = 0)",
        xy=(0.02, opt_pct / 100),
        xytext=(0.18, 0.55),
        fontsize=12,
        color="tab:green",
        fontweight="bold",
        arrowprops=dict(arrowstyle="->", color="tab:green"),
    )

    # RIGHT: decision-cost box-and-whisker (swap vs optimal), mean labelled in box.
    # box = IQR (p25-p75), line = median, diamond = mean, whiskers = p1..p99.
    if np.isfinite(alloc).any():
        groups = [alloc[np.isfinite(alloc)]]
        names = ["swap"]
        colors = ["tab:blue"]
        if opt_alloc.size:
            groups.append(opt_alloc)
            names.append("optimal")
            colors.append("tab:orange")
        bp = ax[1].boxplot(
            groups,
            showmeans=True,
            whis=(1, 99),
            showfliers=False,
            widths=0.5,
            patch_artist=True,
            medianprops=dict(color="black", lw=1.5),
            meanprops=dict(
                marker="D",
                markerfacecolor="white",
                markeredgecolor="black",
                markersize=7,
            ),
        )
        for patch, c in zip(bp["boxes"], colors):
            patch.set_facecolor(c)
            patch.set_alpha(0.35)
        # mean value labelled inside each box
        for i, g in enumerate(groups, start=1):
            m = float(np.mean(g))
            ax[1].annotate(
                f"mean {m:.1f} µs",
                xy=(i, m),
                ha="center",
                va="bottom",
                fontsize=11,
                fontweight="bold",
                bbox=dict(boxstyle="round,pad=0.25", fc="white", ec="0.6", alpha=0.9),
            )
        ax[1].set_xticks(range(1, len(names) + 1))
        ax[1].set_xticklabels(names, fontsize=12)
        ax[1].set_ylabel("allocation time per decision (µs)", fontsize=13)
        ax[1].set_title(f"Decision cost on real hardware", fontsize=13)
        ax[1].grid(alpha=0.3, axis="y")
        ax[1].set_ylim(bottom=0)
    else:
        ax[1].text(0.5, 0.5, "no alloc_us in log", ha="center", va="center")
        ax[1].axis("off")

    # self-describing title from the log: policy, RBG/user, epsilon
    bits = []
    if pol:
        bits.append(pol.upper())
    bits.append(f"{rpu} RBG/user")
    if eps is not None:
        bits.append(f"$\\epsilon$={eps:g}")
    fig.suptitle("Swap decision quality  —  " + ", ".join(bits), fontsize=18)
    # NOTE: don't call tight_layout here — it overrides wspace. Set margins + gap manually.
    fig.subplots_adjust(left=0.07, right=0.97, top=0.88, bottom=0.12, wspace=0.30)
    fig.savefig(a.out, dpi=140)
    plt.close(fig)

    txt = a.out.rsplit(".", 1)[0] + ".txt"
    with open(txt, "w") as f:
        lbl = a.label or a.swaplog.split("/")[-1]
        f.write(
            f"swap decision quality — {lbl}\nslots={len(slots)} users={U} RUs={R}\n\n"
        )
        f.write(f"OPTIMALITY: {opt_pct:.1f}% of slots hit the true max-min optimum\n")
        f.write(
            f"  gap-to-optimum: mean {gap_db.mean():.3f} dB, p90 {np.percentile(gap_db,90):.2f} dB, "
            f"max {gap_db.max():.2f} dB\n"
        )
        f.write(f"WEAKEST ON OWN BEST RU: {onbest_pct:.1f}% of slots\n")
        f.write(
            f"CAPTURED-GAIN RATIO: {captured:.0f}% "
            f"(best-fixed {fm:.2f} -> swap {am:.2f} -> ceiling {cm:.2f} dB)\n"
        )
        if np.isfinite(alloc).any():
            f.write(
                f"ALLOC TIME (swap): mean {np.nanmean(alloc):.2f} us, p99 {np.nanpercentile(alloc,99):.2f} us, "
                f"max {np.nanmax(alloc):.2f} us\n"
            )
        if opt_alloc.size:
            f.write(
                f"ALLOC TIME (optimal, exhaustive, same CPU): mean {opt_alloc.mean():.2f} us, "
                f"p99 {np.percentile(opt_alloc,99):.2f} us, max {opt_alloc.max():.2f} us "
                f"(optimal/swap = {opt_alloc.mean()/max(np.nanmean(alloc),1e-9):.1f}x)\n"
            )
    print(
        f"optimality={opt_pct:.1f}%  weakest-on-best={onbest_pct:.1f}%  captured={captured:.0f}%  "
        f"mean-gap={gap_db.mean():.3f}dB  alloc_mean={np.nanmean(alloc):.2f}us"
    )
    print("wrote", a.out, "and", txt)


if __name__ == "__main__":
    main()
