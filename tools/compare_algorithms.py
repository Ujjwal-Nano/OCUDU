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
import os
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

# Exact per-slot optimum (max-min of SUMS) via MILP, for rpu where brute-force
# enumeration of assignments is intractable (e.g. U4/R12/rpu3 -> 369,600;
# the old permutations() path tries 12! = 4.8e8 and hangs). MILP returns the
# PROVEN optimum without enumerating. Needs scipy>=1.9 (ships HiGHS).
try:
    from scipy.optimize import milp, LinearConstraint, Bounds

    _HAVE_MILP = True
except ImportError:
    _HAVE_MILP = False

BRUTE_MAX = 100000  # <= this many assignments: enumerate; above: use MILP


def _assignment_count(U, R, rpu):
    """Number of distinct disjoint assignments = prod_u C(R-u*rpu, rpu)."""
    from math import comb

    c = 1
    for u in range(U):
        avail = R - u * rpu
        if avail < rpu:
            return 0
        c *= comb(avail, rpu)
    return c


def optimal_maxmin_milp(csi, U, R, rpu):
    """Exact max-min of SUMS: maximize t s.t. each user's RU-sum >= t, each user
    gets exactly rpu distinct RUs, each RU to <=1 user. Returns the optimal
    weakest-user aggregate power (linear units)."""
    import io, contextlib

    # Scale CSI so coefficients are O(1e4). With raw ~1e-4 magnitudes, HiGHS's
    # default MIP gap/tolerances can return a slightly SUB-optimal t (it may even
    # fall below the heuristic on deep-fade slots, which is impossible for a true
    # optimum). Scaling + mip_rel_gap=0 forces the exact integer optimum; t is
    # linear in the CSI, so we divide the scale back out of the result.
    mx = max((csi[u][r] for u in range(U) for r in range(R)), default=1.0)
    s = (1e4 / mx) if mx > 0 else 1.0
    n = U * R + 1  # x[u,r] flattened (U*R) + t (last)
    c = np.zeros(n)
    c[-1] = -1.0  # maximize t
    integ = np.ones(n)
    integ[-1] = 0  # x integer, t continuous
    lb = np.zeros(n)
    ub = np.ones(n)
    ub[-1] = np.inf
    A1 = np.zeros((U, n))  # sum_r csi[u,r] x[u,r] - t >= 0
    for u in range(U):
        for r in range(R):
            A1[u, u * R + r] = csi[u][r] * s
        A1[u, -1] = -1.0
    A2 = np.zeros((U, n))  # sum_r x[u,r] = rpu
    for u in range(U):
        for r in range(R):
            A2[u, u * R + r] = 1.0
    A3 = np.zeros((R, n))  # sum_u x[u,r] <= 1
    for r in range(R):
        for u in range(U):
            A3[r, u * R + r] = 1.0
    cons = [
        LinearConstraint(A1, 0, np.inf),
        LinearConstraint(A2, rpu, rpu),
        LinearConstraint(A3, 0, 1),
    ]
    with contextlib.redirect_stderr(io.StringIO()):  # silence HiGHS chatter
        res = milp(
            c=c,
            constraints=cons,
            integrality=integ,
            bounds=Bounds(lb, ub),
            options={"mip_rel_gap": 0.0},
        )  # force exact optimum
    return float(-res.fun) / s


def _opt_worker(args):
    """Picklable wrapper so the per-slot MILP can run in a process pool."""
    grid, U, R, rpu = args
    return optimal_maxmin_milp(grid, U, R, rpu)


def optimal_series(grids, idx, U, R, rpu, jobs):
    """Exact per-slot optimum over the given slot indices, parallelised over
    `jobs` processes (each MILP is single-threaded, so this scales ~linearly)."""
    tasks = [(grids[i], U, R, rpu) for i in idx]
    if jobs and jobs > 1 and len(tasks) > 50:
        import multiprocessing as mp

        with mp.Pool(jobs) as pool:
            vals = pool.map(
                _opt_worker, tasks, chunksize=max(1, len(tasks) // (jobs * 8))
            )
    else:
        vals = [optimal_maxmin_milp(g, U, R, rpu) for (g, U, R, rpu) in tasks]
    return np.array(vals)


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
    """All policies share the epsilon-worst-resource set (Zhang eq 4), so each
    is swept over epsilon and labelled with it."""
    return f"{pol}  $\\epsilon$={e}"


def make_plot(curves, opt_db, fixed_db, n_slots, n_users, rpu, out_png, pmax=0.1):
    """Single plot: Gamma_th (dB) vs outage probability, one legend entry per curve.

    Colour encodes the POLICY (WRF blue / GGM green / WUF orange / optimal
    black), identical in every plot. Marker encodes the EPSILON VALUE and is
    identical across policies, so a given epsilon looks the same everywhere
    (point (1): "same marker for same epsilon"). By number of sides where one
    exists: 3 -> triangle, 4 -> square; 1 -> circle; 2 has no 2-sided polygon
    so it uses a cross (edit EPS_MARKER[2] to change). WUF markers are filled,
    WRF/GGM open, so same-epsilon curves of different policies stay distinct.
    """
    color = {"WRF": "tab:blue", "GGM": "tab:green", "WUF": "tab:orange"}
    ls = {"WRF": "-", "GGM": "--", "WUF": "-."}
    # marker by epsilon value, shared across all policies (point (1))
    EPS_MARKER = {1: "o", 2: "X", 3: "^", 4: "s"}
    DEFAULT_MARKER = "P"

    pmin = max(1.0 / n_slots, 5e-4)
    pg = np.logspace(np.log10(pmin), np.log10(pmax), 60)
    thr = lambda series: np.quantile(series, pg)

    fig, ax = plt.subplots(figsize=(9.5, 6))

    if fixed_db is not None:
        ax.semilogx(
            pg, thr(fixed_db), color="tab:red", lw=2.2, label="no swap  (best fixed)"
        )

    for j, (pol, e, series) in enumerate(curves):
        mk = EPS_MARKER.get(e, DEFAULT_MARKER)  # marker = epsilon value (point 1)
        ax.semilogx(
            pg,
            thr(series),
            color=color[pol],
            ls=ls[pol],
            lw=2.0,
            marker=mk,
            ms=8,
            mfc="white",
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
        "--ggm-eps",
        default="1,2,4",
        help="epsilons for GGM, comma list (default 1,2,4)",
    )
    ap.add_argument(
        "--wuf-eps",
        default="4",
        help="epsilons for WUF, comma list (default 4). can be set to 1,2,4",
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
        help="also draw the best-fixed (no-swap) reference curve "
        "(brute-force only; omitted automatically when the optimum uses MILP)",
    )
    ap.add_argument(
        "--opt-method",
        choices=["auto", "milp", "brute"],
        default="auto",
        help="optimal baseline: 'brute' enumerates all assignments (only for "
        "small rpu), 'milp' uses the exact solver (any rpu), 'auto' picks milp "
        "above %d assignments (default)" % BRUTE_MAX,
    )
    ap.add_argument(
        "--opt-subsample",
        type=int,
        default=0,
        help="slots used for the MILP optimum CCDF. 0 = ALL slots (default), for "
        "a fair full-resolution comparison against the heuristics; a positive N "
        "uses N evenly-spaced slots for a faster preview. With --opt-jobs, "
        "all-slots stays practical.",
    )
    ap.add_argument(
        "--opt-jobs",
        type=int,
        default=0,
        help="parallel processes for the MILP optimum (0 = all CPU cores). "
        "Each MILP is single-threaded, so this scales ~linearly.",
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
        + [("WUF", e) for e in parse_eps(a.wuf_eps)]
    )

    # ---- baselines: per-slot optimum (MILP for large rpu, brute otherwise) ----
    C = np.asarray(grids, float)  # (n_slots, U, R)
    n_assign = _assignment_count(U, R, rpu)
    use_milp = (a.opt_method == "milp") or (
        a.opt_method == "auto" and n_assign > BRUTE_MAX
    )
    opt_subsampled = False
    if use_milp:
        if not _HAVE_MILP:
            sys.exit(
                "optimal needs scipy>=1.9 for MILP at this rpu "
                "(%d assignments). Install: pip install -U scipy" % n_assign
            )
        # per-slot optimum is independent across slots, so an evenly-spaced
        # subsample gives an unbiased CCDF; brute force (n_assign) is intractable.
        k = len(grids) if a.opt_subsample <= 0 else min(a.opt_subsample, len(grids))
        idx = np.linspace(0, len(grids) - 1, k).astype(int)
        jobs = a.opt_jobs if a.opt_jobs else (os.cpu_count() or 1)
        print(
            "optimal: MILP on %d of %d slots, %d parallel jobs "
            "(brute force would need %d assignments)" % (k, len(grids), jobs, n_assign)
        )
        opt_db = db(optimal_series(grids, idx, U, R, rpu, jobs))
        opt_subsampled = k != len(grids)
        opt_idx = idx  # the slots the optimum was evaluated on
        fixed_db = (
            None  # best-fixed brute force intractable here; omitted (see --fixed)
        )
    else:
        asgs = _uniq_assignments(U, R, rpu)
        means, opt = stream_best_opt(C, asgs, U)
        opt_db = db(opt)
        opt_idx = None
        fixed_db = db(floor_series(C, asgs[int(means.argmax())], U))

    # ---- swap replays ----
    curves, times = [], {}
    for pol, e in runs:
        fl, t_us = replay(grids, rpu, e, POLICIES[pol])
        curves.append((pol, e, db(fl)))
        times[(pol, e)] = t_us / n

    # ---- guard: optimum >= every heuristic on each evaluated slot ----
    # The true per-slot optimum is always >= any heuristic's floor. On a handful
    # of extreme deep-fade slots HiGHS can still return a hair below optimal even
    # after scaling; the best heuristic floor is a valid lower bound on the true
    # optimum, so clamping up to it repairs those without ever overshooting.
    if opt_idx is not None and curves:
        best_heur_db = np.max([d for (_, _, d) in curves], axis=0)[opt_idx]
        opt_db = np.maximum(opt_db, best_heur_db)

    # ---- plot ----
    # When the optimum is a subsample, draw EVERY curve on the SAME slots so the
    # CCDFs are comparable and the optimum can't dip below a heuristic in the
    # noisy deep tail (opt >= heuristic per slot => opt >= heuristic per quantile
    # only when evaluated on identical slots).
    if opt_subsampled:
        plot_curves = [(pol, e, d[opt_idx]) for pol, e, d in curves]
        n_grid = len(opt_idx)
    else:
        plot_curves = curves
        n_grid = n
    make_plot(
        plot_curves,
        opt_db,
        fixed_db if a.fixed else None,
        n_grid,
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

    fixed_mean = fixed_db.mean() if fixed_db is not None else None

    def row(name, d, t=None):
        t_s = f"{t:>11.3f}" if t is not None else f"{'-':>11}"
        gain = (
            f"{d.mean() - fixed_mean:>+12.2f}"
            if fixed_mean is not None
            else f"{'-':>12}"
        )
        return (
            f"{name:<14}{d.mean():>10.2f}{np.percentile(d, 1):>10.2f}"
            f"{np.percentile(d, 5):>10.2f}{gain}"
            f"{opt_db.mean() - d.mean():>9.2f}{t_s}"
        )

    if fixed_db is not None:
        lines.append(row("best-fixed", fixed_db))
    for pol, e, d in curves:
        lines.append(row(f"{pol} eps={e}", d, times[(pol, e)]))
    lines.append(row("optimal" + ("*" if opt_subsampled else ""), opt_db))
    lines += [
        "",
        "py-ref us = python decision time per slot on this CPU; a RELATIVE "
        "reference only, not a gNB hardware result.",
        "opt gap   = mean(optimal) - mean(curve) in dB; lower is better.",
    ]
    if opt_subsampled:
        lines.append(
            "optimal*  = exact MILP optimum on %d evenly-spaced slots "
            "(per-slot independent -> unbiased CCDF)." % len(opt_db)
        )
    report = "\n".join(lines)
    print(report)
    with open(a.out + ".txt", "w") as f:
        f.write(report + "\n")

    # per-slot floors (dB) for re-plotting / audit
    with open(a.out + ".jsonl", "w") as f:
        for i in range(n):
            rec = {}
            if not opt_subsampled:  # aligned per-slot optimum (brute path)
                rec["optimal"] = float(opt_db[i])
            if fixed_db is not None:
                rec["best_fixed"] = float(fixed_db[i])
            for pol, e, d in curves:
                rec[f"{pol}_eps{e}"] = float(d[i])
            f.write(json.dumps(rec) + "\n")
    # subsampled MILP optimum: write its own CCDF sample (not per-slot aligned)
    if opt_subsampled:
        with open(a.out + ".optimal.jsonl", "w") as f:
            for v in opt_db:
                f.write(json.dumps({"optimal_db": float(v)}) + "\n")

    print("wrote", ", ".join(a.out + s for s in (".png", ".txt", ".jsonl")))


if __name__ == "__main__":
    main()
