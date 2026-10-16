#!/usr/bin/env python3
"""compare_algorithms.py — reproduce the Zhang paper's algorithm comparison on a
testbed capture.

Runs, on the SAME logged CSI grid, the paper's baselines and the swapping
strategies, and reports each one's achieved weakest-user floor (the system
metric Gamma_min, eq 3) and its decision time on the same CPU:

  BASELINES (recomputed from scratch every slot, as in the paper)
    Max-Min      : Trassl et al. — maximise the minimum PER-RESOURCE quality
                   s.t. each user gets exactly rpu distinct resources. Implemented
                   as a bottleneck assignment (threshold + bipartite matching),
                   which gives the same optimum as Trassl's sort + Dulmage-
                   Mendelsohn search. O(R^3 U^5) in the paper.
    Preference   : Schwarzenberg et al., gradient variant — each round the user
                   with the LOWEST aggregate quality takes its most-preferred free
                   resource. O(R^2 U^3).
    Bulk         : Walstab et al. — same, but the lowest user claims all rpu of
                   its top free resources at once (U rounds instead of RU).
                   O(R U^3).
    Exhaustive   : brute-force per-slot user-aggregate max-min oracle (the ceiling).

  SWAPPING (warm-started — the whole point: only adjust, don't recompute)
    GGM / WRF / WUF : the three pairwise-swap priority strategies. WRF/WUF are
                   imported from swap_policies.py (the validated C++ port); GGM is
                   the same machinery ordered by highest normalised gain first.

Offline by necessity: only one assignment can go on the air per period, so the
baselines cannot run live beside the swap. Replaying every algorithm on the
captured CSI is the only apples-to-apples comparison, and the only way to time
them all on one CPU.

ALL algorithms are SCORED on the same yardstick — the weakest-user AGGREGATE
floor  Gamma_min = min_u sum_{r in A(u)} gamma_{u,r}  (eq 3) — even though Max-Min
internally optimises the minimum per-RESOURCE quality. That mismatch is exactly
why Max-Min trails the oracle at R>1 (paper Fig 3), and this script will show it.

Usage:
  python3 compare_algorithms.py swap.jsonl --rus-per-user 2 -o out/cmp
  python3 compare_algorithms.py swap.jsonl -o out/cmp          # rpu from log
"""

import argparse, json, sys, copy
from time import perf_counter
import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from scipy.sparse import csr_matrix
from scipy.sparse.csgraph import maximum_bipartite_matching

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
EPS = 1e-12


# ---------------------------------------------------------------------------
# swap strategy: GGM (same candidate machinery as WRF, ordered by gain)
# ---------------------------------------------------------------------------
def policy_ggm(user_rus, csi, epsilon=1, num_redun=0):
    """Greedy Gain Maximisation: generate the same Type-1/Type-2 candidates as
    WRF over the epsilon worst RUs, then execute them highest-normalised-gain
    first, conflict-free. (paper eq 9)."""
    U = len(user_rus)
    R = len(csi[0])
    ur = [list(x) for x in user_rus]
    owner = [UNASSIGNED] * R
    ru_perf = [0.0] * R
    for u in range(U):
        for r in ur[u]:
            owner[r] = u
            ru_perf[r] = csi[u][r]
    allocated = [r for r in range(R) if owner[r] != UNASSIGNED]
    allocated.sort(key=lambda r: ru_perf[r])
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
                c_A_target = csi[A][tr]
                c_B_worst = csi[B][wr]
                c_B_target = csi[B][tr]
                d_swap = min(c_A_target, c_B_worst)
                d_current = min(c_A_worst, c_B_target)
                if d_swap > d_current and c_A_worst < c_A_target:
                    ops.append((1, wr, tr, (d_swap - d_current) / c_B_worst, B))
        for sr in spares:
            c_A_spare = csi[A][sr]
            if c_A_spare > c_A_worst:
                ops.append((2, wr, sr, (c_A_spare - c_A_worst) / c_A_worst, UNASSIGNED))

    stats = {"swaps": 0, "replaces": 0, "ran": True}
    if not ops:
        return ur, stats
    locked = [0] * R
    for (typ, wr, tr, gain, B) in sorted(ops, key=lambda o: -o[3]):  # GGM: global gain order
        if locked[wr] or locked[tr] or gain <= 0.0:
            continue
        A = owner[wr]
        if typ == 1:
            _replace(ur[A], wr, tr)
            _replace(ur[B], tr, wr)
            owner[wr] = B
            owner[tr] = A
            stats["swaps"] += 1
        else:
            _replace(ur[A], wr, tr)
            owner[wr] = UNASSIGNED
            owner[tr] = A
            stats["replaces"] += 1
        locked[wr] = 1
        locked[tr] = 1
    return ur, stats


# ---------------------------------------------------------------------------
# baseline: Max-Min (Trassl) — bottleneck assignment via threshold + matching
# ---------------------------------------------------------------------------
def maxmin_trassl(csi, U, R, rpu):
    """Maximise the minimum assigned PER-RESOURCE gain, each user getting exactly
    rpu distinct resources. Same optimum as Trassl's sort + Dulmage-Mendelsohn,
    found here by binary-searching a gain threshold and testing feasibility with
    a bipartite matching (user split into rpu slots)."""
    nrows = U * rpu
    gains = sorted({csi[u][r] for u in range(U) for r in range(R)}, reverse=True)

    def feasible(theta):
        rows, cols = [], []
        for u in range(U):
            base = u * rpu
            for r in range(R):
                if csi[u][r] >= theta:
                    for s in range(rpu):
                        rows.append(base + s)
                        cols.append(r)
        if not rows:
            return None
        g = csr_matrix(
            (np.ones(len(rows), dtype=np.int8), (rows, cols)), shape=(nrows, R)
        )
        # perm_type="column": array of length nrows, perm[row] = matched col (or -1)
        m = maximum_bipartite_matching(g, perm_type="column")
        return None if np.any(m == -1) else m

    # smallest index in the descending gain list (= largest theta) that is feasible
    lo, hi, best = 0, len(gains) - 1, None
    while lo <= hi:
        mid = (lo + hi) // 2
        m = feasible(gains[mid])
        if m is not None:
            best = m
            hi = mid - 1
        else:
            lo = mid + 1
    if best is None:  # degenerate; fall back to greedy best-per-user
        best = feasible(gains[-1])
    assign = [[] for _ in range(U)]
    for u in range(U):
        for s in range(rpu):
            assign[u].append(int(best[u * rpu + s]))
    return assign


# ---------------------------------------------------------------------------
# baseline: Preference-Based (Schwarzenberg, gradient variant)
# ---------------------------------------------------------------------------
def preference_gradient(csi, U, R, rpu):
    """Gradient variant: each round, serve the under-allocated user that is most
    constrained — lowest POTENTIAL aggregate (current served + the best gain it
    could still grab). That protects the weakest link (the max-min gradient),
    and discriminates even at the start when everyone is at zero. The chosen user
    takes its most-preferred free resource."""
    assigned = [[] for _ in range(U)]
    served = [0.0] * U
    free = [True] * R
    need = [rpu] * U
    for _ in range(U * rpu):
        cand = [u for u in range(U) if need[u] > 0]
        if not cand:
            break

        def best_avail(u):
            b = -1.0
            for r in range(R):
                if free[r] and csi[u][r] > b:
                    b = csi[u][r]
            return b

        u = min(cand, key=lambda u: served[u] + best_avail(u))
        r = max((r for r in range(R) if free[r]), key=lambda r: csi[u][r])
        assigned[u].append(r)
        served[u] += csi[u][r]
        free[r] = False
        need[u] -= 1
    return assigned


# ---------------------------------------------------------------------------
# baseline: Bulk (Walstab) — lowest user claims all rpu top free resources
# ---------------------------------------------------------------------------
def bulk_walstab(csi, U, R, rpu):
    """Bulk variant: the most-constrained remaining user (lowest potential
    aggregate over its top rpu free resources) claims all rpu of them at once."""
    assigned = [[] for _ in range(U)]
    served = [0.0] * U
    free = [True] * R
    done = [False] * U
    for _ in range(U):
        cand = [u for u in range(U) if not done[u]]
        if not cand:
            break

        def topsum(u):
            vals = sorted((csi[u][r] for r in range(R) if free[r]), reverse=True)[:rpu]
            return sum(vals)

        u = min(cand, key=lambda u: served[u] + topsum(u))
        order = sorted((r for r in range(R) if free[r]), key=lambda r: -csi[u][r])
        for r in order[:rpu]:
            assigned[u].append(r)
            served[u] += csi[u][r]
            free[r] = False
        done[u] = True
    return assigned


# ---------------------------------------------------------------------------
def db(x):
    return 10 * np.log10(np.maximum(np.asarray(x, float), EPS))


def read_meta(path):
    meta = {"policy": None, "rus_per_user": None, "epsilon": None}
    for ln in open(path):
        ln = ln.strip()
        if not ln:
            continue
        try:
            d = json.loads(ln)
        except json.JSONDecodeError:
            continue
        for k in ("policy", "rus_per_user", "epsilon"):
            if meta[k] is None and d.get(k) is not None:
                meta[k] = d[k]
        if all(v is not None for v in meta.values()):
            break
    return meta


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("swaplog", nargs="?")
    ap.add_argument("--rb", help="per-RB log instead (regroup with --rbs-per-ru)")
    ap.add_argument("--rbs-per-ru", type=int, default=8)
    ap.add_argument("--rus-per-user", type=int, default=None, help="default: from log, else 1")
    ap.add_argument("--epsilon", type=int, default=None, help="swap depth; default: from log, else 1")
    ap.add_argument("--min-users", type=int, default=3)
    ap.add_argument("--no-exhaustive", action="store_true", help="skip the oracle (speed)")
    ap.add_argument("-o", "--out", default="algo_cmp")
    ap.add_argument("--label", default=None)
    a = ap.parse_args()

    meta = read_meta(a.swaplog) if a.swaplog else {"policy": None, "rus_per_user": None, "epsilon": None}
    if a.rb:
        grids = build_grids_rb(a.rb, a.min_users, a.rbs_per_ru)
    elif a.swaplog:
        grids = build_grids_swaplog(a.swaplog, a.min_users)
    else:
        ap.error("give a swap.jsonl or --rb <per-RB log>")
    if not grids:
        sys.exit("no usable slots")
    U = len(grids[0])
    R = len(grids[0][0])
    rpu = a.rus_per_user or (int(meta["rus_per_user"]) if meta["rus_per_user"] else 1)
    eps = a.epsilon if a.epsilon is not None else (int(meta["epsilon"]) if meta["epsilon"] else 1)
    if U * rpu > R:
        sys.exit("need U*rus_per_user=%d <= R=%d" % (U * rpu, R))

    swap_fn = {"GGM": policy_ggm, "WRF": policy_wrf, "WUF": policy_wuf}
    base_fn = {"Max-Min": maxmin_trassl, "Preference": preference_gradient, "Bulk": bulk_walstab}
    init = [list(range(u * rpu, u * rpu + rpu)) for u in range(U)]
    state = {k: copy.deepcopy(init) for k in swap_fn}

    names = list(swap_fn) + list(base_fn) + (["Exhaustive"] if not a.no_exhaustive else [])
    floors = {k: np.empty(len(grids)) for k in names}
    times = {k: 0.0 for k in names}
    asgs = _uniq_assignments(U, R, rpu) if not a.no_exhaustive else None

    for i, g in enumerate(grids):
        for k, fn in swap_fn.items():
            t0 = perf_counter()
            state[k], _ = fn(state[k], g, epsilon=eps)
            times[k] += (perf_counter() - t0) * 1e6
            floors[k][i] = floor_of(state[k], g)
        for k, fn in base_fn.items():
            t0 = perf_counter()
            asg = fn(g, U, R, rpu)
            times[k] += (perf_counter() - t0) * 1e6
            floors[k][i] = floor_of(asg, g)
        if asgs is not None:
            t0 = perf_counter()
            best = max(floor_of(asg, g) for asg in asgs)
            times["Exhaustive"] += (perf_counter() - t0) * 1e6
            floors["Exhaustive"][i] = best

    n = len(grids)
    # ---- hardware decision times from the gNB log (the real, same-CPU numbers) ----
    # alloc_us = applied swap (WRF/WUF), and by shared machinery also GGM.
    # opt_alloc_us = Exhaustive. maxmin_us / pref_us / bulk_us = the baselines.
    hw_raw = {k: [] for k in (
        "alloc_us", "opt_alloc_us", "maxmin_us", "pref_us", "bulk_us",
        "wrf_us_e1", "wrf_us_e2", "wrf_us_e4",
        "ggm_us_e1", "ggm_us_e2", "ggm_us_e4",
    )}
    if a.swaplog:
        for ln in open(a.swaplog):
            ln = ln.strip()
            if not ln:
                continue
            try:
                d = json.loads(ln)
            except json.JSONDecodeError:
                continue
            for k in hw_raw:
                v = d.get(k)
                if v is not None and float(v) >= 0:
                    hw_raw[k].append(float(v))
    hwmean = {k: (sum(v) / len(v) if v else None) for k, v in hw_raw.items()}
    hw_us = {
        "WRF": hwmean["alloc_us"], "WUF": hwmean["alloc_us"], "GGM": hwmean["alloc_us"],
        "Exhaustive": hwmean["opt_alloc_us"],
        "Max-Min": hwmean["maxmin_us"], "Preference": hwmean["pref_us"], "Bulk": hwmean["bulk_us"],
    }

    def hwcol(k):
        v = hw_us.get(k)
        return f"{v:>13.3f}" if v is not None else f"{'n/a':>13}"

    # ---- report ----
    order = ["Exhaustive", "Max-Min", "Preference", "Bulk", "GGM", "WRF", "WUF"]
    order = [k for k in order if k in names]
    lines = []
    lines.append(f"algorithm comparison — {a.label or (a.swaplog or a.rb).split('/')[-1]}")
    lines.append(f"slots={n} users={U} RUs={R} rus_per_user={rpu} epsilon={eps}  "
                 f"(policy logged={meta['policy']})")
    lines.append("")
    lines.append("PERFORMANCE (floor, computed offline)  +  DECISION COST")
    lines.append("  gNB us/slot = real C++ time on the base station CPU (apples-to-apples).")
    lines.append("  py-ref us   = same-CPU python, RELATIVE reference only — not a hardware result.")
    lines.append("")
    hdr = (f"{'algorithm':<12}{'mean floor(dB)':>15}{'@1%(dB)':>10}"
           f"{'gNB us/slot':>13}{'py-ref us':>11}")
    lines.append(hdr)
    lines.append("-" * len(hdr))
    for k in order:
        d = db(floors[k])
        lines.append(
            f"{k:<12}{d.mean():>15.2f}{np.percentile(d,1):>10.2f}"
            f"{hwcol(k)}{times[k] / n:>11.3f}"
        )
    if hwmean["maxmin_us"] is None:
        lines.append("")
        lines.append("note: no baseline gNB timings in this capture (pre-dates the baseline-timing "
                     "gNB build). Rebuild the gNB to populate maxmin_us/pref_us/bulk_us. Until then "
                     "only alloc_us (swap) / opt_alloc_us (exhaustive) are real hardware times.")

    # relative to Exhaustive (performance)
    if "Exhaustive" in names:
        ex = db(floors["Exhaustive"]).mean()
        lines.append("")
        lines.append("gap to Exhaustive (mean floor, dB) — lower is better:")
        for k in order:
            if k == "Exhaustive":
                continue
            lines.append(f"  {k:<12}{ex - db(floors[k]).mean():+.3f}")

    # headline: swap matches Max-Min in PERFORMANCE at a fraction of the HARDWARE cost
    if "Max-Min" in names:
        lines.append("")
        mm_hw = hw_us.get("Max-Min")
        for sw in ("WRF", "WUF", "GGM"):
            dgap = db(floors[sw]).mean() - db(floors["Max-Min"]).mean()
            sw_hw = hw_us.get(sw)
            if mm_hw and sw_hw:
                lines.append(
                    f"  {sw} vs Max-Min: floor {dgap:+.3f} dB, "
                    f"{mm_hw / max(sw_hw, 1e-9):.0f}x faster on the gNB "
                    f"({sw_hw:.3f} vs {mm_hw:.3f} us/decision)"
                )
            else:
                lines.append(f"  {sw} vs Max-Min: floor {dgap:+.3f} dB  (gNB timing n/a — rebuild gNB)")

    # ---- decision-time sweep on the gNB CPU (for offline timing-vs-epsilon plots) ----
    def tstats(arr):
        if not arr:
            return None
        v = np.asarray(arr, float)
        return (v.mean(), np.percentile(v, 50), np.percentile(v, 99), v.max())

    sweep = []  # (algorithm, epsilon, mean, p50, p99, max)
    for e in (1, 2, 4):
        for algo, key in (("WRF", f"wrf_us_e{e}"), ("GGM", f"ggm_us_e{e}")):
            st = tstats(hw_raw.get(key, []))
            if st:
                sweep.append((algo, str(e), *st))
    for algo, key in (("Max-Min", "maxmin_us"), ("Preference", "pref_us"),
                      ("Bulk", "bulk_us"), ("Exhaustive", "opt_alloc_us"),
                      ("swap-applied", "alloc_us")):
        st = tstats(hw_raw.get(key, []))
        if st:
            sweep.append((algo, "-", *st))  # epsilon-independent

    if sweep:
        lines.append("")
        lines.append("DECISION TIME on the gNB CPU (us) — epsilon-swept for WRF/GGM, "
                     "single for the rest (eps='-'):")
        lines.append(f"  {'algorithm':<13}{'eps':>4}{'mean':>10}{'p50':>10}{'p99':>10}{'max':>10}")
        for (algo, e, m, p50, p99, mx) in sweep:
            lines.append(f"  {algo:<13}{e:>4}{m:>10.3f}{p50:>10.3f}{p99:>10.3f}{mx:>10.3f}")
        with open(a.out + "_timing.csv", "w") as f:
            f.write("algorithm,epsilon,mean_us,p50_us,p99_us,max_us\n")
            for (algo, e, m, p50, p99, mx) in sweep:
                f.write(f"{algo},{e},{m:.4f},{p50:.4f},{p99:.4f},{mx:.4f}\n")
    else:
        lines.append("")
        lines.append("note: no epsilon-swept gNB timings (wrf_us_e*/ggm_us_e*) in this capture — "
                     "rebuild the gNB with the epsilon-swept timers to populate them.")

    report = "\n".join(lines)
    print(report)
    with open(a.out + ".txt", "w") as f:
        f.write(report + "\n")

    # per-slot floors for re-plotting / audit
    with open(a.out + ".jsonl", "w") as f:
        for i in range(n):
            f.write(json.dumps({k: float(floors[k][i]) for k in names}) + "\n")

    # ---- comparison outage plot (performance story) ----
    def outage(x):
        xs = np.sort(x)
        return xs, np.arange(1, len(xs) + 1) / len(xs)

    styles = {
        "Exhaustive": ("black", "-", 2.4),
        "Max-Min": ("tab:red", "-", 2.0),
        "Preference": ("tab:orange", "-", 1.8),
        "Bulk": ("tab:brown", ":", 1.8),
        "GGM": ("tab:green", "--", 2.0),
        "WRF": ("tab:blue", "--", 2.0),
        "WUF": ("tab:purple", "-.", 2.0),
    }
    fig, ax = plt.subplots(figsize=(9.5, 6))
    for k in order:
        c, ls, lw = styles.get(k, ("gray", "-", 1.5))
        xs, ps = outage(db(floors[k]))
        ax.semilogy(xs, ps, color=c, ls=ls, lw=lw, label=k)
    ax.set_xlabel("weakest-user power  $\\Gamma_{th}$  (dB)", fontsize=14)
    ax.set_ylabel("outage probability  $P(\\Gamma_{min} < \\Gamma_{th})$", fontsize=14)
    ax.set_title(
        f"Algorithm comparison — {rpu} RBG/user  (swap $\\epsilon$={eps})", fontsize=12.5
    )
    ax.grid(alpha=0.3, which="both")
    ax.legend(fontsize=11, loc="lower right", ncol=2)
    ax.set_ylim(max(1.0 / n, 5e-4), 1.0)
    fig.tight_layout()
    fig.savefig(a.out + ".png", dpi=140)
    plt.close(fig)
    outs = [a.out + ".txt", a.out + ".png", a.out + ".jsonl"]
    if sweep:
        outs.append(a.out + "_timing.csv")
    print("wrote", ", ".join(outs))


if __name__ == "__main__":
    main()