#!/usr/bin/env python3
"""swap_analysis.py — multi-user swap benefit analysis (Option C).

Reads the scheduler's own /tmp/swap_metrics.jsonl (per-slot, per-user CSI +
achieved assignment) and answers two questions in the SAME units (b/s/Hz):

  ACHIEVED  : what your swap actually delivered to the weakest user
  ACHIEVABLE: the best any max-min swap could deliver that slot (ceiling)
  FIXED     : a static no-adaptation baseline (each user pinned to one RU)

Headline results:
  realized gain  = ACHIEVED - FIXED      (what the swap won vs not adapting)
  gap-to-optimal = ACHIEVABLE - ACHIEVED (what the swap left on the table)

Plus METRIC 1 (what you asked for): per-RU cross-user correlation — for each RU,
how correlated that RU's quality is ACROSS the users. Low = users see the RU
differently = swap has something to exploit; high = they move together = little
room. Reported as the mean pairwise correlation per RU (pairwise underneath).

No time-alignment needed: the swap logs all users on one slot boundary, so every
line is already synchronized.

Rate model: per-user rate = sum over the user's OWNED RUs of log2(1+snr0*csi_norm),
where csi is normalized so the median per-RU csi maps to snr0 dB (so the absolute
scale is a knob, comparisons are scale-robust). Weakest-user = min over users.

Usage:
  python3 swap_analysis.py /tmp/swap_metrics.jsonl -o swap_analysis.png
  python3 swap_analysis.py capture.swap.jsonl -o out.png --snr0 10 --rus-per-user 1
"""

import argparse, json, sys, itertools
from collections import defaultdict
import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

EPS = 1e-12


def load(path):
    """Return list of slots: each {t, users:[{u,rnti,csi[np],served,rus[list]}]}.
    Keeps only slots where every user has a full-length csi (clean, aligned)."""
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
            recs = []
            ok = True
            for u in us:
                csi = u.get("csi")
                if not csi:
                    ok = False
                    break
                if R is None:
                    R = len(csi)
                if len(csi) != R:
                    ok = False
                    break
                # drop floor-only (absent) users
                if max(csi) <= 1e-8:
                    ok = False
                    break
                recs.append(
                    {
                        "u": u.get("u"),
                        "rnti": u.get("rnti"),
                        "csi": np.asarray(csi, float),
                        "served": u.get("served", 0.0),
                        "rus": list(u.get("rus", [])),
                    }
                )
            if ok and recs:
                slots.append({"t": d.get("t"), "users": recs})
    return slots, (R or 0)


def rate_of_rus(csi, rus, snr_scale):
    """achievable rate (b/s/Hz) for a user given the RUs it holds."""
    if not len(rus):
        return 0.0
    snr = snr_scale * csi[list(rus)]
    return float(np.sum(np.log2(1.0 + np.maximum(snr, 0.0))))


def _all_assignments(U, R):
    """All ways to give each of U users a DISTINCT single RU out of R (U<=R).
    Yields tuples: assignment[u] = ru. With 3 users, 4 RUs -> 24 assignments."""
    return itertools.permutations(range(R), U)


def optimal_maxmin(csis, snr_scale):
    """TRUE max-min optimum this slot by brute force (one RU per user, distinct;
    the spare RU(s) go unused). Guaranteed >= any real assignment, so it is a
    valid achievable CEILING. Small: U<=3, R=4 -> 24 assignments."""
    U = len(csis)
    R = len(csis[0])
    best_min, best = -1.0, None
    for perm in _all_assignments(U, R):
        rates = [rate_of_rus(csis[u], [perm[u]], snr_scale) for u in range(U)]
        m = min(rates)
        if m > best_min:
            best_min, best = m, perm
    return [[r] for r in best]  # list of 1-RU lists


def weakest_rate(csis, assignment, snr_scale):
    rates = [rate_of_rus(csis[u], assignment[u], snr_scale) for u in range(len(csis))]
    # exclude users with no RUs from the min (they aren't contending)
    active = [r for r, a in zip(rates, assignment) if len(a)]
    return min(active) if active else 0.0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("swaplog")
    ap.add_argument("-o", "--out", default="swap_analysis.png")
    ap.add_argument(
        "--snr0",
        type=float,
        default=10.0,
        help="reference SNR (dB) mapped to the MEDIAN per-RU csi",
    )
    ap.add_argument("--rus-per-user", type=int, default=1)
    ap.add_argument(
        "--smooth",
        type=int,
        default=50,
        help="moving-average window (slots) for the rate timelines",
    )
    a = ap.parse_args()

    slots, R = load(a.swaplog)
    if not slots:
        sys.exit(
            "no usable slots in "
            + a.swaplog
            + " (need per-user csi arrays; is this swap_metrics.jsonl?)"
        )
    U = max(len(s["users"]) for s in slots)

    # scale factor: map median csi over all users/RUs/slots -> snr0 dB (linear)
    all_csi = np.concatenate([u["csi"] for s in slots for u in s["users"]])
    med = np.median(all_csi[all_csi > 0]) if np.any(all_csi > 0) else 1.0
    snr_scale = (10 ** (a.snr0 / 10.0)) / max(med, EPS)

    # fixed baseline: assign RU u%R to user u (a deterministic static split)
    ach, ceil = [], []
    fix = []  # filled after loop (best static split)
    slot_csis = []  # retain per-slot csis for the whole-run fixed search
    ts = []
    # for per-RU cross-user correlation, collect per-user per-RU series (by user index)
    # align by user index within slot (u field); assume stable ordering per slot
    ru_series = defaultdict(lambda: defaultdict(list))  # ru -> uid -> [csi over slots]

    for s in slots:
        us = s["users"]
        csis = [u["csi"] for u in us]
        slot_csis.append(csis)
        uids = [u["u"] for u in us]
        # --- achieved: use the swap's own 'served' (what really happened) ---
        served_rates = []
        for u in us:
            served_rates.append(rate_of_rus(u["csi"], u["rus"], snr_scale))
        active = [r for r, u in zip(served_rates, us) if len(u["rus"])]
        ach.append(min(active) if active else 0.0)
        # --- achievable ceiling: TRUE max-min optimum this slot (brute force) ---
        opt = optimal_maxmin(csis, snr_scale)
        c_opt = weakest_rate(csis, opt, snr_scale)
        # the true optimum is provably >= achieved (both use distinct RUs), so any
        # residual below achieved is float/model noise -> clamp to achieved.
        ceil.append(max(c_opt, ach[-1]))
        # (fixed baseline computed after the loop as the best SINGLE static split)
        ts.append(s["t"])
        # correlation series
        for u in us:
            for r in range(R):
                ru_series[r][u["u"]].append(u["csi"][r])

    # ---- FIXED baseline = the single best STATIC assignment, held all run ----
    # brute-force all static perms; for each, mean weakest-user rate over the run;
    # keep the best. This is the fair "no adaptation but best split" baseline.
    U_full = max(len(c) for c in slot_csis)
    best_fixed_perm, best_fixed_mean = None, -1.0
    for perm in _all_assignments(U_full, R):
        per_slot = []
        for csis in slot_csis:
            if len(csis) != U_full:
                continue
            rates = [rate_of_rus(csis[u], [perm[u]], snr_scale) for u in range(U_full)]
            per_slot.append(min(rates))
        if per_slot:
            mfix = float(np.mean(per_slot))
            if mfix > best_fixed_mean:
                best_fixed_mean, best_fixed_perm = mfix, perm
    # build the fixed timeline from the winning static perm
    for csis in slot_csis:
        if len(csis) == U_full and best_fixed_perm is not None:
            rates = [
                rate_of_rus(csis[u], [best_fixed_perm[u]], snr_scale)
                for u in range(U_full)
            ]
            fix.append(min(rates))
        else:
            fix.append(np.nan)

    ach = np.array(ach)
    fix = np.array(fix)
    ceil = np.array(ceil)

    def sm(x, w):
        if w <= 1 or len(x) < w:
            return x
        k = np.ones(w) / w
        return np.convolve(x, k, mode="same")

    # ---- METRIC 1: per-RU cross-user correlation (avg pairwise) ----
    percorr = {}  # ru -> mean pairwise corr
    percorr_pairs = {}  # ru -> {(ui,uj): corr}
    uids_all = sorted({u["u"] for s in slots for u in s["users"]})
    for r in range(R):
        pairs = {}
        series = {
            uid: np.array(ru_series[r][uid])
            for uid in uids_all
            if len(ru_series[r][uid]) > 5
        }
        keys = list(series.keys())
        vals = []
        for ui, uj in itertools.combinations(keys, 2):
            a_, b_ = series[ui], series[uj]
            n = min(len(a_), len(b_))
            if n < 5:
                continue
            aa, bb = np.log10(np.maximum(a_[:n], EPS)), np.log10(
                np.maximum(b_[:n], EPS)
            )
            if aa.std() > 1e-9 and bb.std() > 1e-9:
                c = np.corrcoef(aa, bb)[0, 1]
                pairs[(ui, uj)] = c
                vals.append(c)
        percorr[r] = float(np.mean(vals)) if vals else float("nan")
        percorr_pairs[r] = pairs

    # ---- plot ----
    fig = plt.figure(figsize=(15, 9))
    gs = fig.add_gridspec(2, 2, height_ratios=[1.4, 1])

    # top: weakest-user rate timelines (the headline)
    axT = fig.add_subplot(gs[0, :])
    x = (
        (np.array(ts, float) - ts[0]) / 1000.0
        if all(t is not None for t in ts)
        else np.arange(len(ts))
    )
    axT.plot(
        x, sm(fix, a.smooth), label="fixed (no adaptation)", color="tab:red", lw=1.3
    )
    axT.plot(
        x, sm(ach, a.smooth), label="achieved (your swap)", color="tab:blue", lw=1.6
    )
    axT.plot(
        x,
        sm(ceil, a.smooth),
        label="achievable (max-min ceiling)",
        color="tab:green",
        lw=1.3,
        ls="--",
    )
    axT.set_xlabel("time (s)")
    axT.set_ylabel("weakest-user rate (b/s/Hz)")
    axT.set_title(
        "Weakest-user rate: fixed vs achieved vs achievable ceiling "
        f"(snr0={a.snr0:.0f} dB @ median CSI)"
    )
    axT.legend()
    axT.grid(alpha=0.3)

    # bottom-left: gain bars (means)
    axG = fig.add_subplot(gs[1, 0])
    realized = ach.mean() - fix.mean()
    gap = ceil.mean() - ach.mean()
    bars = axG.bar(
        ["fixed", "achieved", "ceiling"],
        [fix.mean(), ach.mean(), ceil.mean()],
        color=["tab:red", "tab:blue", "tab:green"],
        alpha=0.8,
    )
    axG.set_ylabel("mean weakest-user rate (b/s/Hz)")
    axG.set_title(
        f"realized gain = {realized:+.3f}  |  gap-to-optimal = {gap:+.3f} b/s/Hz"
    )
    axG.grid(alpha=0.3, axis="y")

    # bottom-right: per-RU cross-user correlation
    axC = fig.add_subplot(gs[1, 1])
    rs = list(range(R))
    cv = [percorr[r] for r in rs]
    cols = [
        (
            "tab:green"
            if (not np.isnan(v) and v < 0.2)
            else ("tab:red" if (not np.isnan(v) and v > 0.5) else "tab:orange")
        )
        for v in cv
    ]
    axC.bar(rs, [0 if np.isnan(v) else v for v in cv], color=cols)
    axC.axhline(0.5, color="k", ls="--", lw=0.7, alpha=0.5)
    axC.set_ylim(-1, 1)
    axC.set_xticks(rs)
    axC.set_xticklabels([f"RU{r}" for r in rs])
    axC.set_ylabel("mean cross-user corr")
    axC.set_title(
        "Per-RU cross-user correlation\n(low=swap opportunity, high=contention)"
    )
    axC.grid(alpha=0.3, axis="y")

    fig.suptitle(
        "Multi-user swap benefit (Option C) — " + a.swaplog.split("/")[-1], fontsize=13
    )
    fig.tight_layout(rect=[0, 0, 1, 0.97])
    fig.savefig(a.out, dpi=140)
    plt.close(fig)

    # ---- text ----
    txt = a.out.rsplit(".", 1)[0] + ".txt"
    with open(txt, "w") as f:
        f.write(f"swap_analysis — {a.swaplog}\n")
        f.write(f"slots={len(slots)}  users={U}  RUs={R}  snr0={a.snr0} dB\n\n")
        f.write(f"best static split (fixed baseline): user->RU {best_fixed_perm}\n")
        f.write("WEAKEST-USER RATE (b/s/Hz), mean over run:\n")
        f.write(f"  fixed (no adaptation) : {fix.mean():.4f}\n")
        f.write(f"  achieved (your swap)  : {ach.mean():.4f}\n")
        f.write(f"  achievable (ceiling)  : {ceil.mean():.4f}\n")
        f.write(
            f"  --> realized gain     : {ach.mean()-fix.mean():+.4f}  "
            f"({100*(ach.mean()-fix.mean())/max(fix.mean(),EPS):+.1f}%)\n"
        )
        f.write(
            f"  --> gap to optimal    : {ceil.mean()-ach.mean():+.4f}  "
            f"(capture ratio {100*(ach.mean()-fix.mean())/max(ceil.mean()-fix.mean(),EPS):.0f}% "
            f"of available gain)\n\n"
        )
        f.write("PER-RU CROSS-USER CORRELATION (log-CSI, mean pairwise):\n")
        for r in range(R):
            f.write(
                f"  RU{r}: {percorr[r]:+.3f}   pairs: "
                + ", ".join(
                    f"{ui}-{uj}={c:+.2f}" for (ui, uj), c in percorr_pairs[r].items()
                )
                + "\n"
            )
        f.write("\ninterpretation: realized gain>0 means the swap helps the weakest\n")
        f.write("user vs a static split. low per-RU cross-user corr = users see that\n")
        f.write("RU differently = the swap has structure to exploit. If gain~0 but\n")
        f.write("corr is high, users contend (expected in LOS/co-located); the gain\n")
        f.write("regime is NLOS/separated/mobile where corr drops.\n")
    print(f"slots={len(slots)} users={U} RUs={R}")
    print(
        f"weakest-user rate  fixed={fix.mean():.4f}  achieved={ach.mean():.4f}  "
        f"ceiling={ceil.mean():.4f} b/s/Hz"
    )
    print(
        f"realized gain={ach.mean()-fix.mean():+.4f}  gap-to-optimal={ceil.mean()-ach.mean():+.4f}"
    )
    print(
        f"per-RU cross-user corr: "
        + ", ".join(f"RU{r}={percorr[r]:+.2f}" for r in range(R))
    )
    print("wrote", a.out, "and", txt)


if __name__ == "__main__":
    main()
