#!/usr/bin/env python3
"""compare_algorithms.py — swap-policy comparison on ONE testbed capture.

Replays every algorithm on the same logged CSI grid and draws TWO outage plots
(weakest-user power threshold Gamma_th (dB) vs outage probability P_out,sys;
paper Fig. 3 layout, higher is better; Gamma_th at P is the P-quantile of the
per-slot weakest-user power):

  PLOT 1  <out>.png           (unchanged look: same colours / markers / labels)
      WRF  (worst RU first)             swept over epsilon   (default 1,2,4)
      GGM  (greedy gain maximisation)   swept over epsilon   (default 1,2,4)
      WUF  (weakest user first)         (default epsilon 4)
      optimal (per-slot max-min oracle) drawn once
      no swap (best fixed)              red, solid  (worst case)

  PLOT 2  <out>.all_algos.png
      WRF eps=4, WUF eps=4, GGM eps=4  (same colour/marker as plot 1)
      optimal, Max-min (Trassl), PrefGradient, Bulk   (new colours / markers)
      no swap (best fixed)              red, solid  (worst case)

epsilon = number of worst RUs reconsidered for a swap each period (NOT a dB
threshold). WRF/WUF/GGM are warm-started replays: each (policy, epsilon) evolves
its own assignment slot by slot. Max-min / Pref / Bulk are stateless: rebuilt
from the CSI every slot. All curves are scored on the same yardstick, the
weakest-user aggregate power
    Gamma_min = min_u  sum_{r in A(u)} gamma_{u,r}          (10*log10, dB)

OPTIMAL is ONE algorithm: exhaustive_maxmin (exact branch-and-bound, warm-seeded
from the previous slot). NO SWAP is the best STATIC assignment (maximises the mean
weakest-user floor over the run): exact brute force when the search space is
small, otherwise a local search (marked with * in the report).

All algorithms live in swap_policies.py; this file only replays and plots.

Usage (4 users; only rus-per-user is needed):
  python3 compare_algorithms.py swap.jsonl --rus-per-user 2 -o out/cmp
  python3 compare_algorithms.py --rb rb.jsonl --rbs-per-ru 8 --rus-per-user 2 -o out/cmp
"""

import argparse
import itertools
import json
import os
import sys
from math import comb
from time import perf_counter

import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

try:
    from swap_policies import (
        POLICIES,
        max_min_leximin,
        pref_gradient,
        bulk_gradient,
        exhaustive_maxmin,
        build_grids_swaplog,
        build_grids_rb,
        floor_of,
    )
except ImportError:
    sys.exit(
        "keep compare_algorithms.py in the same folder as the UPDATED "
        "swap_policies.py (needs policy_ggm, max_min_leximin, pref_gradient, "
        "bulk_gradient, exhaustive_maxmin)"
    )

EPS = 1e-12  # numerical floor for log10 (NOT the swap epsilon)
PLOT2_EPS = 4  # epsilon used for WRF / WUF / GGM in plot 2
FIXED_EXACT_MAX = 3e8  # n_assignments * n_slots below this -> exact best-fixed

# ---------------------------------------------------------------------------
# plot style. Plot-1 styling is UNCHANGED. Colour = policy, marker = epsilon.
# ---------------------------------------------------------------------------
POL_COLOR = {"WRF": "tab:blue", "GGM": "tab:green", "WUF": "tab:orange"}
POL_LS = {"WRF": "-", "GGM": "--", "WUF": "-."}
EPS_MARKER = {1: "o", 2: "X", 3: "^", 4: "s"}
DEFAULT_MARKER = "P"
# new algorithms (plot 2): colours/markers not used by anything above
NEW_STYLE = {
    "Max-min": dict(label="Max-min", color="tab:purple", ls=":"),
    "Pref": dict(
        label="Preference based", color="tab:brown", ls=(0, (3, 1, 1, 1, 1, 1))
    ),
    "Bulk": dict(label="Bulk", color="tab:cyan", ls=(0, (5, 1))),
}
FIXED_COLOR = "red"  # no swap (best fixed): solid red

# ONE line width / marker size / marker edge for EVERY curve in EVERY plot
LW = 2.2  # line width (all curves, incl. optimal and no-swap)
MS = 7.5  # marker size
MEW = 1.5  # marker edge width
MARKEVERY = 8  # a marker every N points (staggered per curve)
RC = {  # publication look
    "font.family": "STIXGeneral",
    "mathtext.fontset": "stix",
    "axes.linewidth": 1.0,
    "xtick.direction": "in",
    "ytick.direction": "in",
    "xtick.top": True,
    "ytick.right": True,
    "xtick.major.size": 5,
    "ytick.major.size": 5,
    "xtick.minor.size": 3,
    "ytick.minor.size": 3,
    "xtick.labelsize": 12,
    "ytick.labelsize": 12,
}


# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------
def db(x):
    return 10 * np.log10(np.maximum(np.asarray(x, float), EPS))


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


def parse_eps(s):
    return [int(x) for x in s.split(",") if x.strip()]


def floor_series(C, asg, U):
    """Weakest-user aggregate power per slot for ONE fixed assignment."""
    served = np.stack([C[:, u, asg[u]].sum(axis=1) for u in range(U)], axis=1)
    return served.min(axis=1)


def replay(grids, rpu, epsilon, policy_fn):
    """Warm-started trajectory of one swap policy at one epsilon.
    Returns (per-slot floor, total decision time in microseconds)."""
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


def replay_stateless(fn, C, rpu, **kw):
    """Stateless allocator (Max-min / Pref / Bulk): rebuilt from CSI every slot."""
    floors = np.empty(len(C))
    t_us = 0.0
    for i in range(len(C)):
        t0 = perf_counter()
        asg = fn(C[i], rpu, **kw)
        t_us += (perf_counter() - t0) * 1e6
        floors[i] = floor_of(asg, C[i])
    return floors, t_us


def optimal_series(C, rpu):
    """OPTIMAL = exhaustive_maxmin on every slot (exact), warm-seeded by the
    previous slot's optimum. Returns per-slot optimal weakest-user power."""
    U = C.shape[1]
    seed = [list(range(u * rpu, u * rpu + rpu)) for u in range(U)]
    out = np.empty(len(C))
    for i in range(len(C)):
        seed = exhaustive_maxmin(C[i], rpu, seed)
        out[i] = floor_of(seed, C[i])
    return out


# ---------------------------------------------------------------------------
# NO SWAP = best fixed assignment
# ---------------------------------------------------------------------------
def _assignment_count(U, R, rpu):
    """Number of distinct disjoint assignments = prod_u C(R-u*rpu, rpu)."""
    c = 1
    for u in range(U):
        avail = R - u * rpu
        if avail < rpu:
            return 0
        c *= comb(avail, rpu)
    return c


def _iter_assignments(U, R, rpu, avail=None, u=0):
    """Every way to give U users disjoint sets of rpu RUs (no duplicates)."""
    if avail is None:
        avail = tuple(range(R))
    if u == U:
        yield []
        return
    for c in itertools.combinations(avail, rpu):
        rest = tuple(r for r in avail if r not in c)
        for tail in _iter_assignments(U, R, rpu, rest, u + 1):
            yield [list(c)] + tail


def _local_search_fixed(C, U, R, rpu, starts):
    """Steepest-ascent over pairwise RU swaps / spare replacements, maximising the
    MEAN weakest-user floor of one static assignment. Best of several starts."""

    def mean_floor(asg):
        return floor_series(C, asg, U).mean()

    best_asg, best_v = None, -np.inf
    for st in starts:
        asg = [list(x) for x in st]
        v = mean_floor(asg)
        for _ in range(500):
            used = {r for a in asg for r in a}
            spares = [r for r in range(R) if r not in used]
            cand, cv = None, v
            for u in range(U):
                for i in range(rpu):
                    for w in range(u + 1, U):  # swap with another user's RU
                        for j in range(rpu):
                            a2 = [list(x) for x in asg]
                            a2[u][i], a2[w][j] = a2[w][j], a2[u][i]
                            val = mean_floor(a2)
                            if val > cv + 1e-15:
                                cand, cv = a2, val
                    for sp in spares:  # replace by a spare RU
                        a2 = [list(x) for x in asg]
                        a2[u][i] = sp
                        val = mean_floor(a2)
                        if val > cv + 1e-15:
                            cand, cv = a2, val
            if cand is None:
                break
            asg, v = cand, cv
        if v > best_v:
            best_asg, best_v = asg, v
    return best_asg


def best_fixed_assignment(C, U, R, rpu):
    """Best static assignment (max mean weakest-user floor). Returns (asg, exact)."""
    n_assign = _assignment_count(U, R, rpu)
    if n_assign * len(C) <= FIXED_EXACT_MAX:
        best, bv = None, -np.inf
        for asg in _iter_assignments(U, R, rpu):
            v = floor_series(C, asg, U).mean()
            if v > bv:
                best, bv = asg, v
        return best, True
    init = [list(range(u * rpu, u * rpu + rpu)) for u in range(U)]
    mean_seed = exhaustive_maxmin(C.mean(axis=0), rpu, init)  # max-min of avg CSI
    return _local_search_fixed(C, U, R, rpu, [init, mean_seed]), False


# ---------------------------------------------------------------------------
# plotting
# ---------------------------------------------------------------------------
def curve_label(pol, e):
    """All policies share the epsilon-worst-resource set (Zhang eq 4), so each
    is swept over epsilon and labelled with it."""
    return f"{pol}  $\\epsilon$={e}"


def swap_style(pol, e):
    return dict(
        label=curve_label(pol, e),
        color=POL_COLOR[pol],
        ls=POL_LS[pol],
        marker=EPS_MARKER.get(e, DEFAULT_MARKER),
    )


def draw_outage(
    curves, opt_db, fixed_db, n_slots, n_users, rpu, out_png, title, pmax=0.1
):
    """One outage plot. `curves` = list of (style_dict, series_dB).

    Colour encodes the ALGORITHM, marker encodes the EPSILON VALUE for the swap
    policies (same marker for same epsilon in every plot). Every curve uses the
    same line width / marker size. No-swap (best fixed) is solid red; optimal is
    black dashed. Each plot is also saved as a vector PDF."""
    pmin = max(1.0 / n_slots, 5e-4)
    pg = np.logspace(np.log10(pmin), np.log10(pmax), 60)
    thr = lambda series: np.quantile(series, pg)

    with plt.rc_context(RC):
        fig, ax = plt.subplots(figsize=(9.5, 6))

        ax.semilogx(
            pg,
            thr(fixed_db),
            color=FIXED_COLOR,
            ls="-",
            lw=LW,
            label="no swap  (best fixed)",
            zorder=2.5,
        )

        for j, (st, series) in enumerate(curves):
            ax.semilogx(
                pg,
                thr(series),
                color=st["color"],
                ls=st["ls"],
                lw=LW,
                marker=st["marker"],
                ms=MS,
                mfc="white",
                mew=MEW,
                markevery=((j * 3) % MARKEVERY, MARKEVERY),  # staggered markers
                label=st["label"],
                zorder=3 + j,
            )

        ax.semilogx(
            pg,
            thr(opt_db),
            color="black",
            lw=LW,
            ls=(0, (6, 2)),
            label="optimal  (per-slot max-min)",
            zorder=2,
        )

        ax.set_xlabel(
            "outage probability  $P(\\Gamma_{min} < \\Gamma_{th})$", fontsize=14
        )
        ax.set_ylabel("weakest-user power  $\\Gamma_{th}$  (dB)", fontsize=14)
        ax.set_title(title, fontsize=13)
        ax.grid(which="major", alpha=0.35, lw=0.8)
        ax.grid(which="minor", alpha=0.15, lw=0.5)
        ax.set_xlim(pmin, pmax)
        # legend below the axes so it never covers the (closely spaced) curves
        ax.legend(
            fontsize=11.5,
            loc="upper center",
            bbox_to_anchor=(0.5, -0.15),
            ncol=3,
            framealpha=1.0,
            edgecolor="0.3",
            fancybox=False,
            handlelength=3.0,
            columnspacing=1.6,
        )
        fig.tight_layout()
        fig.savefig(out_png, dpi=300, bbox_inches="tight")
        fig.savefig(out_png.rsplit(".", 1)[0] + ".pdf", bbox_inches="tight")  # vector
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
    # plot-1 epsilons are fixed (WRF 1,2,4 / GGM 1,2,4 / WUF 4); the flags
    # still work but are hidden, you normally never need them.
    ap.add_argument("--wrf-eps", default="1,2,4", help=argparse.SUPPRESS)
    ap.add_argument("--ggm-eps", default="1,2,4", help=argparse.SUPPRESS)
    ap.add_argument("--wuf-eps", default="4", help=argparse.SUPPRESS)
    ap.add_argument(
        "--l-pref",
        type=float,
        default=3.0,
        help="Pref/Bulk lookahead (== PREF_LOOKAHEAD in scheduler_swap.cpp)",
    )
    ap.add_argument(
        "--min-users", type=int, default=4, help="users needed per slot (default 4)"
    )
    ap.add_argument(
        "--pmax",
        type=float,
        default=0.1,
        help="largest outage probability on the x-axis (default 0.1)",
    )
    # kept so old command lines still run; behaviour is now fixed:
    ap.add_argument("--fixed", action="store_true", help=argparse.SUPPRESS)
    ap.add_argument("--opt-method", default=None, help=argparse.SUPPRESS)
    ap.add_argument("--opt-subsample", type=int, default=0, help=argparse.SUPPRESS)
    ap.add_argument("--opt-jobs", type=int, default=0, help=argparse.SUPPRESS)
    ap.add_argument(
        "-o",
        "--out",
        default="algo_cmp",
        help="output prefix -> .png / .all_algos.png / .txt / .jsonl",
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
    if os.path.dirname(a.out):
        os.makedirs(os.path.dirname(a.out), exist_ok=True)

    n = len(grids)
    U, R = len(grids[0]), len(grids[0][0])
    rpu = a.rus_per_user or (int(meta["rus_per_user"]) if meta["rus_per_user"] else 1)
    if U * rpu > R:
        sys.exit("need U*rus_per_user=%d <= R=%d RUs" % (U * rpu, R))
    C = np.asarray(grids, float)  # (n_slots, U, R)

    # ---- which swap runs: plot-1 epsilons plus eps=4 for plot 2 ----
    plot1_eps = {
        "WRF": parse_eps(a.wrf_eps),
        "GGM": parse_eps(a.ggm_eps),
        "WUF": parse_eps(a.wuf_eps),
    }
    runs = []
    for pol in ("WRF", "GGM", "WUF"):
        for e in sorted(set(plot1_eps[pol]) | {PLOT2_EPS}):
            runs.append((pol, e))

    # ---- baselines: exact optimum + best fixed (no swap) ----
    print("optimal: exhaustive_maxmin on %d slots ..." % n)
    opt_db = db(optimal_series(C, rpu))
    fixed_asg, fixed_exact = best_fixed_assignment(C, U, R, rpu)
    fixed_db = db(floor_series(C, fixed_asg, U))
    print(
        "no swap (best fixed): %s  [%s]"
        % (fixed_asg, "exact" if fixed_exact else "local search")
    )

    # ---- swap replays + stateless algorithms ----
    res, times = {}, {}  # key -> dB series / us per slot
    for pol, e in runs:
        fl, t_us = replay(grids, rpu, e, POLICIES[pol])
        res[(pol, e)] = db(fl)
        times[(pol, e)] = t_us / n
    for name, fn, kw in (
        ("Max-min", max_min_leximin, {}),
        ("Pref", pref_gradient, {"l_pref": a.l_pref}),
        ("Bulk", bulk_gradient, {"l_pref": a.l_pref}),
    ):
        fl, t_us = replay_stateless(fn, C, rpu, **kw)
        res[name] = db(fl)
        times[name] = t_us / n

    # ---- sanity: the exact optimum can never be below any algorithm ----
    worst_viol = max(float((d - opt_db).max()) for d in res.values())
    if worst_viol > 1e-6:
        print(
            "WARNING: an algorithm exceeded 'optimal' by %.3g dB on some slot "
            "- check exhaustive_maxmin / CSI units" % worst_viol
        )

    title_tail = f"{U} Users, {rpu} RBG/user"

    # ---- PLOT 1: unchanged look (epsilon sweeps + optimal) + best fixed ----
    curves1 = []
    for pol, eps_list in (
        ("WRF", plot1_eps["WRF"]),
        ("GGM", plot1_eps["GGM"]),
        ("WUF", plot1_eps["WUF"]),
    ):
        for e in eps_list:
            curves1.append((swap_style(pol, e), res[(pol, e)]))
    draw_outage(
        curves1,
        opt_db,
        fixed_db,
        n,
        U,
        rpu,
        a.out + ".png",
        f"Algorithm comparison: {title_tail}",
        pmax=a.pmax,
    )

    # ---- PLOT 2: WRF/WUF/GGM at eps=4 + Max-min / Pref / Bulk + optimal ----
    curves2 = [
        (swap_style(p, PLOT2_EPS), res[(p, PLOT2_EPS)]) for p in ("WRF", "WUF", "GGM")
    ]
    for name in ("Max-min", "Pref", "Bulk"):
        curves2.append((NEW_STYLE[name], res[name]))
    draw_outage(
        curves2,
        opt_db,
        fixed_db,
        n,
        U,
        rpu,
        a.out + ".all_algos.png",
        f"Algorithm comparison ($\\epsilon$={PLOT2_EPS}): {title_tail}",
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
    fixed_mean = fixed_db.mean()

    def row(name, d, t=None):
        t_s = f"{t:>11.3f}" if t is not None else f"{'-':>11}"
        return (
            f"{name:<14}{d.mean():>10.2f}{np.percentile(d, 1):>10.2f}"
            f"{np.percentile(d, 5):>10.2f}{d.mean() - fixed_mean:>+12.2f}"
            f"{opt_db.mean() - d.mean():>9.2f}{t_s}"
        )

    lines.append(row("best-fixed" + ("" if fixed_exact else "*"), fixed_db))
    for pol, e in runs:
        lines.append(row(f"{pol} eps={e}", res[(pol, e)], times[(pol, e)]))
    for name in ("Max-min", "Pref", "Bulk"):
        lines.append(row(name, res[name], times[name]))
    lines.append(row("optimal", opt_db))
    lines += [
        "",
        "py-ref us = python decision time per slot on this CPU; a RELATIVE "
        "reference only, not a gNB hardware result.",
        "opt gap   = mean(optimal) - mean(curve) in dB; lower is better.",
        "optimal   = exhaustive_maxmin (exact, branch-and-bound) on every slot.",
    ]
    if not fixed_exact:
        lines.append(
            "best-fixed* = best static assignment found by local search "
            "(exhaustive enumeration too large at this U/R/rpu)."
        )
    report = "\n".join(lines)
    print(report)
    with open(a.out + ".txt", "w") as f:
        f.write(report + "\n")

    # per-slot floors (dB) for re-plotting / audit — all aligned per slot
    with open(a.out + ".jsonl", "w") as f:
        for i in range(n):
            rec = {"optimal": float(opt_db[i]), "best_fixed": float(fixed_db[i])}
            for pol, e in runs:
                rec[f"{pol}_eps{e}"] = float(res[(pol, e)][i])
            for name in ("Max-min", "Pref", "Bulk"):
                rec[name.lower().replace("-", "")] = float(res[name][i])
            f.write(json.dumps(rec) + "\n")

    print(
        "wrote",
        ", ".join(
            a.out + s for s in (".png/.pdf", ".all_algos.png/.pdf", ".txt", ".jsonl")
        ),
    )


if __name__ == "__main__":
    main()
