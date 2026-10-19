#!/usr/bin/env python3
"""compare_algorithms.py — swap-policy comparison on ONE testbed capture.

Replays, on the same logged CSI grid, the three pairwise-swap strategies and
the per-slot max-min optimum, and draws ONE plot: weakest-user power threshold
Gamma_th (dB) vs outage probability P_out,sys (paper Fig. 3 layout, higher is
better; Gamma_th at P is the P-quantile of the per-slot weakest-user power):

    WRF  (worst RU first)             swept over epsilon   (default 1,2,4)
    GGM  (greedy gain maximisation)   swept over epsilon   (default 1,4)
    WUF  (weakest user first)         one curve, labelled without epsilon
    optimal (per-slot max-min oracle) drawn once, epsilon-independent
    [best fixed assignment]           optional reference   (--fixed)

epsilon = number of worst RUs (WRF/GGM) / weakest users (WUF) reconsidered for
a swap each period. It is NOT a dB threshold. Every curve is a warm-started
replay: each (policy, epsilon) evolves its own assignment slot by slot.

All curves are scored on the same yardstick, the weakest-user aggregate power
    Gamma_min = min_u  sum_{r in A(u)} gamma_{u,r}          (10*log10, dB)

WRF/WUF come from swap_policies.py (the validated port); GGM is defined below
(same candidate machinery as WRF, executed highest normalised gain first).

Usage:
  python3 compare_algorithms.py swap.jsonl -o out/cmp
  python3 compare_algorithms.py swap.jsonl --rus-per-user 2 --wrf-eps 1,2,4 -o out/cmp
  python3 compare_algorithms.py swap.jsonl --ggm-eps 1,4 --wuf-eps 4 --fixed -o out/cmp
  python3 compare_algorithms.py --rb rb.jsonl --rbs-per-ru 8 -o out/cmp
"""

import argparse
import copy
import json
import sys
from time import perf_counter

import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

try:
    from swap_policies import (
        policy_wrf,
        policy_wuf,
        build_grids_swaplog,
        build_grids_rb,
        _uniq_assignments,
        floor_of,
        _replace,
    )
except ImportError:
    sys.exit("keep compare_algorithms.py in the same folder as swap_policies.py")

UNASSIGNED = -1
EPS = 1e-12  # numerical floor for log10 (NOT the swap epsilon)


# ---------------------------------------------------------------------------
# GGM: same Type-1/Type-2 candidates as WRF, executed highest-gain first
# ---------------------------------------------------------------------------
def policy_ggm(user_rus, csi, epsilon=1, num_redun=0):
    """Greedy Gain Maximisation (paper eq 9).

    Builds the same candidates as WRF over the `epsilon` worst allocated RUs
    (Type 1: pairwise swap with another user, Type 2: replace by a spare RU),
    then executes them in descending normalised gain, conflict-free.
    """
    U = len(user_rus)
    R = len(csi[0])
    ur = [list(x) for x in user_rus]
    owner = [UNASSIGNED] * R
    ru_perf = [0.0] * R
    for u in range(U):
        for r in ur[u]:
            owner[r] = u
            ru_perf[r] = csi[u][r]

    allocated = sorted(
        (r for r in range(R) if owner[r] != UNASSIGNED), key=lambda r: ru_perf[r]
    )
    worst = allocated[: min(epsilon, len(allocated))]
    spares = [r for r in range(R) if owner[r] == UNASSIGNED]

    ops = []  # (type, worst_ru, target_ru, gain, target_user)
    for wr in worst:
        A = owner[wr]
        c_A_worst = csi[A][wr]
        for B in range(U):
            if B == A or not ur[B]:
                continue
            for tr in ur[B]:
                d_swap = min(csi[A][tr], csi[B][wr])
                d_current = min(c_A_worst, csi[B][tr])
                if d_swap > d_current and c_A_worst < csi[A][tr]:
                    ops.append((1, wr, tr, (d_swap - d_current) / csi[B][wr], B))
        for sr in spares:
            if csi[A][sr] > c_A_worst:
                ops.append(
                    (2, wr, sr, (csi[A][sr] - c_A_worst) / c_A_worst, UNASSIGNED)
                )

    stats = {"swaps": 0, "replaces": 0, "ran": True}
    if not ops:
        return ur, stats

    locked = [False] * R
    for typ, wr, tr, gain, B in sorted(ops, key=lambda o: -o[3]):
        if locked[wr] or locked[tr] or gain <= 0.0:
            continue
        A = owner[wr]
        if typ == 1:
            _replace(ur[A], wr, tr)
            _replace(ur[B], tr, wr)
            owner[wr], owner[tr] = B, A
            stats["swaps"] += 1
        else:
            _replace(ur[A], wr, tr)
            owner[wr], owner[tr] = UNASSIGNED, A
            stats["replaces"] += 1
        locked[wr] = locked[tr] = True
    return ur, stats


POLICIES = {"GGM": policy_ggm, "WRF": policy_wrf, "WUF": policy_wuf}


# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------
def db(x):
    return 10 * np.log10(np.maximum(np.asarray(x, float), EPS))


def outage(x):
    """Empirical CDF -> (sorted values, P(X <= value))."""
    xs = np.sort(x)
    return xs, np.arange(1, len(xs) + 1) / len(xs)


def read_meta(path):
    """Scrape policy / rus_per_user / epsilon from the gNB-written log fields."""
    meta = {"policy": None, "rus_per_user": None, "epsilon": None}
    with open(path) as fh:
        for ln in fh:
            ln = ln.strip()
            if not ln:
                continue
            try:
                d = json.loads(ln)
            except json.JSONDecodeError:
                continue
            for k in meta:
                if meta[k] is None:
                    v = d.get(k, d.get("eps") if k == "epsilon" else None)
                    if v is not None:
                        meta[k] = v
            if all(v is not None for v in meta.values()):
                break
    return meta


def floor_series(C, asg, U):
    """Weakest-user aggregate power per slot for ONE fixed assignment."""
    served = np.stack([C[:, u, asg[u]].sum(axis=1) for u in range(U)], axis=1)
    return served.min(axis=1)


def stream_best_opt(C, asgs, U):
    """Memory-safe single pass over all assignments.

    Returns (per-assignment mean floor, per-slot optimum). Peak memory is
    O(n_slots) instead of O(n_assignments * n_slots).
    """
    opt = np.full(C.shape[0], -np.inf)
    means = np.empty(len(asgs))
    for i, asg in enumerate(asgs):
        fl = floor_series(C, asg, U)
        means[i] = fl.mean()
        np.maximum(opt, fl, out=opt)
    return means, opt


def replay(grids, rpu, epsilon, policy_fn):
    """Warm-started trajectory of one policy at one epsilon.

    Returns (per-slot floor, total decision time in microseconds).
    """
    U = len(grids[0])
    state = [list(range(u * rpu, u * rpu + rpu)) for u in range(U)]
    floors = np.empty(len(grids))
    t_us = 0.0
    for i, g in enumerate(grids):
        t0 = perf_counter()
        state, _ = policy_fn(state, g, epsilon=epsilon)
        t_us += (perf_counter() - t0) * 1e6
        floors[i] = floor_of(state, g)
    return floors, t_us


def parse_eps(s):
    return [int(x) for x in s.split(",") if x.strip()]


# ---------------------------------------------------------------------------
# plotting
# ---------------------------------------------------------------------------
def curve_label(pol, e):
    """WUF is drawn as ONE curve and shown without an epsilon in the legend."""
    return "WUF" if pol == "WUF" else f"{pol}  $\\epsilon$={e}"


def make_plot(curves, opt_db, fixed_db, n_slots, n_users, rpu, out_png, pmax=0.1):
    """Single plot: Gamma_th (dB) vs outage probability, one legend entry per curve.

    Each policy has its own colour + line style; the epsilons of one policy
    share that colour and differ by marker (WRF: circle / square / triangle,
    GGM: diamond / down-triangle / pentagon). WUF is one curve with no epsilon
    in its label.
    """
    color = {"WRF": "tab:blue", "GGM": "tab:green", "WUF": "tab:orange"}
    ls = {"WRF": "-", "GGM": "--", "WUF": "-."}
    markers = {"WRF": ["o", "s", "^"], "GGM": ["D", "v", "p"], "WUF": ["X"]}

    pmin = max(1.0 / n_slots, 5e-4)
    pg = np.logspace(np.log10(pmin), np.log10(pmax), 60)
    thr = lambda series: np.quantile(series, pg)

    fig, ax = plt.subplots(figsize=(9.5, 6))

    if fixed_db is not None:
        ax.semilogx(
            pg, thr(fixed_db), color="tab:red", lw=2.2, label="no swap  (best fixed)"
        )

    seen = {"WRF": 0, "GGM": 0, "WUF": 0}
    for j, (pol, e, series) in enumerate(curves):
        mk = markers[pol][seen[pol] % len(markers[pol])]
        seen[pol] += 1
        ax.semilogx(
            pg,
            thr(series),
            color=color[pol],
            ls=ls[pol],
            lw=2.0,
            marker=mk,
            ms=8,
            mfc="white" if pol != "WUF" else color[pol],
            mew=1.7,
            markevery=((j * 3) % 8, 8),  # staggered markers
            label=curve_label(pol, e),
            zorder=3 + j,
        )

    ax.semilogx(
        pg,
        thr(opt_db),
        color="black",
        lw=2.6,
        ls=(0, (6, 2)),
        label="optimal  (per-slot max-min)",
        zorder=2,
    )

    # same label / title wording as the original comparison plot

    ax.set_xlabel("outage probability  $P(\\Gamma_{min} < \\Gamma_{th})$", fontsize=14)
    ax.set_ylabel("weakest-user power  $\\Gamma_{th}$  (dB)", fontsize=14)
    ax.set_title(
        f"Algorithm comparison: {n_users} Users, {rpu} RBG/user", fontsize=12.5
    )
    ax.grid(alpha=0.3, which="both")
    ax.set_xlim(pmin, pmax)
    # legend below the axes so it never covers the (closely spaced) curves
    ax.legend(
        fontsize=11,
        loc="upper center",
        bbox_to_anchor=(0.5, -0.15),
        ncol=3,
        framealpha=0.95,
    )
    fig.tight_layout()
    fig.savefig(out_png, dpi=140, bbox_inches="tight")
    plt.close(fig)


# ---------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    ap.add_argument("swaplog", nargs="?", help="swap.jsonl from the gNB")
    ap.add_argument("--rb", help="per-RB log instead (regrouped with --rbs-per-ru)")
    ap.add_argument("--rbs-per-ru", type=int, default=8)
    ap.add_argument(
        "--rus-per-user", type=int, default=None, help="default: from log, else 1"
    )
    ap.add_argument(
        "--wrf-eps",
        default="1,2,4",
        help="epsilons for WRF, comma list (default 1,2,4)",
    )
    ap.add_argument(
        "--ggm-eps", default="1,4", help="epsilons for GGM, comma list (default 1,4)"
    )
    ap.add_argument(
        "--wuf-eps",
        type=int,
        default=4,
        help="depth handed to the WUF policy (default 4, as in the "
        "paper's Fig. 3); WUF is drawn as ONE curve, no epsilon shown",
    )
    ap.add_argument("--min-users", type=int, default=3)
    ap.add_argument(
        "--pmax",
        type=float,
        default=0.1,
        help="largest outage probability on the x-axis (default 0.1)",
    )
    ap.add_argument(
        "--fixed",
        action="store_true",
        help="also draw the best-fixed (no-swap) reference curve",
    )
    ap.add_argument(
        "-o", "--out", default="algo_cmp", help="output prefix -> .png / .txt / .jsonl"
    )
    ap.add_argument("--label", default=None)
    a = ap.parse_args()

    # ---- load ----
    meta = (
        read_meta(a.swaplog)
        if a.swaplog
        else {"policy": None, "rus_per_user": None, "epsilon": None}
    )
    if a.rb:
        grids = build_grids_rb(a.rb, a.min_users, a.rbs_per_ru)
    elif a.swaplog:
        grids = build_grids_swaplog(a.swaplog, a.min_users)
    else:
        ap.error("give a swap.jsonl or --rb <per-RB log>")
    if not grids:
        sys.exit("no usable slots")

    n = len(grids)
    U, R = len(grids[0]), len(grids[0][0])
    rpu = a.rus_per_user or (int(meta["rus_per_user"]) if meta["rus_per_user"] else 1)
    if U * rpu > R:
        sys.exit("need U*rus_per_user=%d <= R=%d RUs" % (U * rpu, R))

    runs = (
        [("WRF", e) for e in parse_eps(a.wrf_eps)]
        + [("GGM", e) for e in parse_eps(a.ggm_eps)]
        + [("WUF", a.wuf_eps)]
    )

    # ---- baselines: per-slot optimum (+ best fixed), one memory-safe pass ----
    C = np.asarray(grids, float)  # (n_slots, U, R)
    asgs = _uniq_assignments(U, R, rpu)
    means, opt = stream_best_opt(C, asgs, U)
    opt_db = db(opt)
    fixed_db = db(floor_series(C, asgs[int(means.argmax())], U))

    # ---- swap replays ----
    curves, times = [], {}
    for pol, e in runs:
        fl, t_us = replay(grids, rpu, e, POLICIES[pol])
        curves.append((pol, e, db(fl)))
        times[(pol, e)] = t_us / n

    # ---- plot ----
    make_plot(
        curves,
        opt_db,
        fixed_db if a.fixed else None,
        n,
        U,
        rpu,
        a.out + ".png",
        pmax=a.pmax,
    )

    # ---- report ----
    lbl = a.label or (a.swaplog or a.rb).split("/")[-1]
    hdr = (
        f"{'curve':<14}{'mean(dB)':>10}{'@1%(dB)':>10}{'@5%(dB)':>10}"
        f"{'gain/fixed':>12}{'opt gap':>9}{'py-ref us':>11}"
    )
    lines = [
        f"swap-policy comparison — {lbl}",
        f"slots={n} users={U} RUs={R} rus_per_user={rpu} "
        f"(gNB logged policy={meta['policy']}, epsilon={meta['epsilon']})",
        "",
        hdr,
        "-" * len(hdr),
    ]

    def row(name, d, t=None):
        t_s = f"{t:>11.3f}" if t is not None else f"{'-':>11}"
        return (
            f"{name:<14}{d.mean():>10.2f}{np.percentile(d, 1):>10.2f}"
            f"{np.percentile(d, 5):>10.2f}{d.mean() - fixed_db.mean():>+12.2f}"
            f"{opt_db.mean() - d.mean():>9.2f}{t_s}"
        )

    lines.append(row("best-fixed", fixed_db))
    for pol, e, d in curves:
        lines.append(
            row("WUF" if pol == "WUF" else f"{pol} eps={e}", d, times[(pol, e)])
        )
    lines.append(row("optimal", opt_db))
    lines += [
        "",
        "py-ref us = python decision time per slot on this CPU; a RELATIVE "
        "reference only, not a gNB hardware result.",
        "opt gap   = mean(optimal) - mean(curve) in dB; lower is better.",
    ]
    report = "\n".join(lines)
    print(report)
    with open(a.out + ".txt", "w") as f:
        f.write(report + "\n")

    # per-slot floors (dB) for re-plotting / audit
    with open(a.out + ".jsonl", "w") as f:
        for i in range(n):
            rec = {"optimal": float(opt_db[i]), "best_fixed": float(fixed_db[i])}
            for pol, e, d in curves:
                rec["WUF" if pol == "WUF" else f"{pol}_eps{e}"] = float(d[i])
            f.write(json.dumps(rec) + "\n")

    print("wrote", ", ".join(a.out + s for s in (".png", ".txt", ".jsonl")))


if __name__ == "__main__":
    main()
