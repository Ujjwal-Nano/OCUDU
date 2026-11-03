#!/usr/bin/env python3
"""swap_policies.py — run WRF and WUF (and best-fixed + per-slot optimum) on the
SAME capture, warm-started, for any rus_per_user. Offline, reproducible,
apples-to-apples. This is the generalisable testbed comparator.

ADDED (purely additive, existing code below is unchanged):
    policy_ggm        Greedy Gain Maximisation (swap policy, same signature as WRF/WUF)
    max_min_leximin   Trassl leximin max-min [6]            \
    pref_gradient     PrefGradient, Schwarzenberg [7]        > stateless baselines,
    bulk_gradient     Bulk, Walstab [8]                      > fn(G, rpu[, l_pref])
    exhaustive_maxmin exact per-slot optimum (branch&bound)  /  G = numpy U x R
    synth_grids       synthetic CSI loader
    POLICIES / BASELINES registries for the compare/plot script to import.

WHY OFFLINE INSTEAD OF TWO LIVE SCHEDULERS
------------------------------------------
Only one assignment can be written to the air each period, so you cannot APPLY
two policies at once. But the swap decision is a pure function of
(CSI snapshot, current assignment), and every per-RB CSI sample is logged — so
each policy can be replayed here from the same data, each evolving its OWN
warm-started assignment (a true trajectory comparison, not a one-step shadow).

WRF (Weakest Resource First)  — faithful port of swap_allocator.cpp::step():
    key on the worst RU (min owner-CSI-on-RU); pairwise Type-1 swaps + Type-2
    spares; min-of-pair improvement; epsilon worst RUs; highest-gain-first;
    lock touched RUs.

WUF (Weakest User First) — the same lightweight pairwise machine, but keyed on
    the weakest USER (min served = sum of CSI over that user's RUs). It swaps one
    of the weakest user's RUs with another user's RU when that raises
    min(served_A, served_B). This is the policy aligned with the max-min
    objective; WRF is a proxy that is EXACT only at rus_per_user == 1 (where the
    worst RU is, by definition, the weakest user's only RU -> WRF == WUF).

Baselines:
    BEST-FIXED : single static user->RU-set assignment maximising the mean
                 weakest-user floor over the run (brute force).
    OPTIMAL    : per-slot max-min over all assignments (the oracle ceiling).

INPUT
-----
A) swap.jsonl (default): per-user "csi" is already the per-RU grid. Use as-is
   at the rus_per_user the capture ran with (usually 1).
B) --rb <per-RB log> --rbs-per-ru N : build the per-RU grid by summing power over
   groups of N consecutive RBs, so you can REGROUP one capture into more RUs and
   test rus_per_user >= 2 without re-running the gNB. (Finalise the field name to
   match your rb.jsonl; see build_grids_rb.)

USAGE
-----
  python3 swap_policies.py swap.jsonl --rus-per-user 1 -o out/cmp
  python3 swap_policies.py --rb cap.rb.jsonl --rbs-per-ru 8 --rus-per-user 2 -o out/cmp
"""

import argparse, json, sys, itertools, copy, time
import numpy as np

UNASSIGNED = -1
EPS = 1e-12


# -----------------------------------------------------------------------------
# policies — csi is a list-of-lists csi[user][ru]; user_rus is list[list[int]]
# both return (new_user_rus, stats_dict) and NEVER mutate the input.
# -----------------------------------------------------------------------------
def _served(user_rus, csi):
    return [sum(csi[u][r] for r in rus) for u, rus in enumerate(user_rus)]


def policy_wrf(user_rus, csi, epsilon=1, num_redun=0):
    """Faithful port of swap_allocator.cpp::step() (decision part only)."""
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
        for B in range(U):  # Type 1: swap with user B's RU
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
        for sr in spares:  # Type 2: take a better spare
            c_A_spare = csi[A][sr]
            if c_A_spare > c_A_worst:
                ops.append((2, wr, sr, (c_A_spare - c_A_worst) / c_A_worst, UNASSIGNED))

    stats = {"swaps": 0, "replaces": 0, "ran": True}
    if not ops:
        return ur, stats

    locked = [0] * R
    order = sorted(range(len(worst)), key=lambda i: csi[owner[worst[i]]][worst[i]])
    for idx in order:
        wr = worst[idx]
        if locked[wr]:
            continue
        ru_ops = sorted([o for o in ops if o[1] == wr], key=lambda o: -o[3])
        for typ, _wr, tr, gain, B in ru_ops:
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
            break
    return ur, stats


def policy_wuf(user_rus, csi, epsilon=1, num_redun=0):
    """Weakest-USER-first, FAITHFUL to Zhang et al. (eq 4, 11, 12).

    Uses the SAME candidate set as WRF/GGM: R_worst = the `epsilon` globally
    worst-performing RUs (lowest owner-CSI). The ONLY thing that differs from
    WRF is the order in which those worst RUs are processed: a two-level
    lexicographic priority — primary pi^(1) = -Gamma_owner (eq 11: the worst
    RU whose OWNER has the lowest aggregate quality goes first), secondary
    pi^(2) = -gamma_ru (eq 12: within the same owner, the worst RU first). So
    epsilon counts WORST RESOURCES (not users), exactly as in the paper; at
    rpu=1 this is identical to WRF."""
    U = len(user_rus)
    R = len(csi[0])
    ur = [list(x) for x in user_rus]
    owner = [UNASSIGNED] * R
    ru_perf = [0.0] * R
    served = [0.0] * U
    for u in range(U):
        for r in ur[u]:
            owner[r] = u
            ru_perf[r] = csi[u][r]
            served[u] += csi[u][r]
    spares = [r for r in range(R) if owner[r] == UNASSIGNED]

    # R_worst = the epsilon globally-worst allocated RUs (SAME set as WRF).
    allocated = [r for r in range(R) if owner[r] != UNASSIGNED]
    allocated.sort(key=lambda r: ru_perf[r])
    worst = allocated[: min(epsilon, len(allocated))]

    # ---- candidate generation: identical to WRF ----
    ops = []  # (type, worst_ru, target_ru, gain, target_user)
    for wr in worst:
        A = owner[wr]
        c_A_worst = csi[A][wr]
        for B in range(U):  # Type 1: swap with user B's RU
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
        for sr in spares:  # Type 2: take a better spare
            c_A_spare = csi[A][sr]
            if c_A_spare > c_A_worst:
                ops.append((2, wr, sr, (c_A_spare - c_A_worst) / c_A_worst, UNASSIGNED))

    stats = {"swaps": 0, "replaces": 0, "ran": True}
    if not ops:
        return ur, stats

    # ---- execution: one best swap per worst RU (by gain), but visit the worst
    #      RUs in WUF's two-level priority order (eq 11 then eq 12): weakest
    #      OWNER first (served ascending), then worst RU within owner (ru_perf
    #      ascending). This is the ONLY difference from WRF. ----
    locked = [0] * R
    worst_order = sorted(worst, key=lambda r: (served[owner[r]], ru_perf[r]))
    for wr in worst_order:
        if locked[wr]:
            continue
        ru_ops = sorted([o for o in ops if o[1] == wr], key=lambda o: -o[3])
        for typ, _wr, tr, gain, B in ru_ops:
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
            break
    return ur, stats


def _replace(v, frm, to):
    for i, x in enumerate(v):
        if x == frm:
            v[i] = to
            return


# -----------------------------------------------------------------------------
# ADDED: GGM swap policy (same signature / format as WRF and WUF above)
# -----------------------------------------------------------------------------
def policy_ggm(user_rus, csi, epsilon=1, num_redun=0):
    """Greedy Gain Maximisation (paper eq 9).

    Builds the same candidates as WRF over the `epsilon` worst allocated RUs
    (Type 1: pairwise swap with another user, Type 2: replace by a spare RU),
    then executes ALL of them pooled together in descending normalised gain,
    conflict-free (an RU touched by an executed op is locked)."""
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

    # ---- candidate generation: identical to WRF ----
    ops = []  # (type, worst_ru, target_ru, gain, target_user)
    for wr in worst:
        A = owner[wr]
        c_A_worst = csi[A][wr]
        for B in range(U):  # Type 1: swap with user B's RU
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
        for sr in spares:  # Type 2: take a better spare
            c_A_spare = csi[A][sr]
            if c_A_spare > c_A_worst:
                ops.append((2, wr, sr, (c_A_spare - c_A_worst) / c_A_worst, UNASSIGNED))

    stats = {"swaps": 0, "replaces": 0, "ran": True}
    if not ops:
        return ur, stats

    # ---- execution: global highest-gain-first over the pooled candidates ----
    locked = [0] * R
    for typ, wr, tr, gain, B in sorted(ops, key=lambda o: -o[3]):
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


# -----------------------------------------------------------------------------
# ADDED: stateless baseline allocators — rebuilt from scratch each slot, no warm
# start. G is a numpy U x R array; they return user_rus (list[list[int]]).
# -----------------------------------------------------------------------------
def pref_gradient(G, rpu, l_pref=3.0):
    """PrefGradient (Schwarzenberg [7]). Repeatedly: score each unfinished user by
    Q = best_gain * gain_at_lookahead_window, give the lowest-Q user its single
    best free RU. l_pref must equal PREF_LOOKAHEAD in scheduler_swap.cpp."""
    U, R = G.shape
    main = [[] for _ in range(U)]
    need = [rpu] * U
    free = list(range(R))
    while any(n > 0 for n in need) and free:
        Q = np.full(U, np.inf)
        best = [-1] * U
        for u in range(U):
            if need[u] <= 0:
                continue
            gf = G[u, free]
            idx = np.argsort(-gf, kind="stable")
            gs = gf[idx]
            W = min(max(1, int(np.ceil(l_pref * need[u]))), len(gs))
            Q[u] = gs[0] * gs[W - 1]
            best[u] = free[idx[0]]
        uw = int(np.argmin(Q))
        main[uw].append(int(best[uw]))
        need[uw] -= 1
        free.remove(int(best[uw]))
    return main


def bulk_gradient(G, rpu, l_pref=3.0):
    """Bulk (Walstab [8]). Same scoring as PrefGradient, but the lowest-Q user
    takes ALL its remaining RUs (its top `need` free RUs) in one go."""
    U, R = G.shape
    main = [[] for _ in range(U)]
    need = [rpu] * U
    free = list(range(R))
    while any(n > 0 for n in need) and free:
        Q = np.full(U, np.inf)
        order = [None] * U
        for u in range(U):
            if need[u] <= 0:
                continue
            gf = G[u, free]
            idx = np.argsort(-gf, kind="stable")
            gs = gf[idx]
            W = min(max(1, int(np.ceil(l_pref * need[u]))), len(gs))
            Q[u] = gs[0] * gs[W - 1]
            order[u] = [free[i] for i in idx]
        uw = int(np.argmin(Q))
        take = order[uw][: need[uw]]
        main[uw].extend(int(x) for x in take)
        need[uw] = 0
        for x in take:
            free.remove(int(x))
    return main


def _kuhn(adj, nL, nR):
    """Maximum bipartite matching size (Kuhn's augmenting paths)."""
    matchR = [-1] * nR

    def try_(v, seen):
        for r in adj[v]:
            if not seen[r]:
                seen[r] = True
                if matchR[r] == -1 or try_(matchR[r], seen):
                    matchR[r] = v
                    return True
        return False

    return sum(1 for v in range(nL) if try_(v, [False] * nR))


def max_min_leximin(G, rpu):
    """Leximin max-min (Trassl [6]). Treat each user as `rpu` virtual users; sweep
    (gain, vuser, ru) pairs from best to worst, and fix the bottleneck pair each
    time a perfect matching of the remaining virtual users first becomes possible."""
    U, R = G.shape
    V = U * rpu
    pairs = sorted(
        ((G[v // rpu, r], v, r) for v in range(V) for r in range(R)),
        key=lambda p: -p[0],
    )
    vleft = list(range(V))
    rleft = list(range(R))
    alloc = [-1] * V
    while vleft:
        nL = len(vleft)
        vidx = {v: i for i, v in enumerate(vleft)}
        ridx = {r: i for i, r in enumerate(rleft)}
        for omega in range(nL, len(pairs) + 1):
            adj = [[] for _ in range(nL)]
            for g, v, r in pairs[:omega]:
                if v in vidx and r in ridx:
                    adj[vidx[v]].append(ridx[r])
            if _kuhn(adj, nL, len(rleft)) == nL:
                g, v, r = pairs[omega - 1]
                alloc[v] = r
                vleft.remove(v)
                rleft.remove(r)
                pairs = [(gg, vv, rr) for (gg, vv, rr) in pairs if vv != v and rr != r]
                break
    main = [[] for _ in range(U)]
    for v in range(V):
        main[v // rpu].append(int(alloc[v]))
    return main


def exhaustive_maxmin(G, rpu, seed_alloc):
    """Exact per-slot max-min of the weakest-USER sum, via DFS branch-and-bound
    (same method as scheduler_swap.cpp::exhaustive_maxmin). Warm-seeded from
    seed_alloc (the previous slot's optimum), so it prunes heavily when the channel
    is coherent across slots - this is the cheap way to keep Optimal feasible at
    rpu>=2 without enumerating all assignments. Returns the optimal assignment."""
    U, K = G.shape
    best = {
        "val": min(sum(G[u, r] for r in seed_alloc[u]) for u in range(U)),
        "alloc": [list(x) for x in seed_alloc],
    }
    order = list(
        np.argsort(
            [sum(sorted(G[u], reverse=True)[:rpu]) for u in range(U)], kind="stable"
        )
    )

    def dfs(depth, used, cur_min, alloc):
        if depth >= U:
            if cur_min > best["val"]:
                best["val"] = cur_min
                best["alloc"] = [list(x) for x in alloc]
            return
        free = [r for r in range(K) if not used[r]]
        bound = cur_min
        for j in order[depth:]:
            bound = min(bound, sum(sorted(G[j, free], reverse=True)[:rpu]))
        if bound <= best["val"]:
            return  # prune: can't beat incumbent
        u = order[depth]
        combos = (
            [tuple(free)]
            if len(free) == rpu
            else list(itertools.combinations(free, rpu))
        )
        for val, c in sorted(
            ((sum(G[u, list(cc)]), cc) for cc in combos), key=lambda x: -x[0]
        ):
            nm = min(cur_min, val)
            if nm <= best["val"]:
                break  # remaining combos are weaker
            alloc[u] = list(c)
            used2 = used.copy()
            for r in c:
                used2[r] = True
            dfs(depth + 1, used2, nm, alloc)

    dfs(0, [False] * K, float("inf"), [list(x) for x in best["alloc"]])
    return best["alloc"]


# ADDED: registries for the compare/plot script (run() below keeps its own dict)
POLICIES = {"WRF": policy_wrf, "WUF": policy_wuf, "GGM": policy_ggm}
BASELINES = {"MAXMIN": max_min_leximin, "PREF": pref_gradient, "BULK": bulk_gradient}


# -----------------------------------------------------------------------------
# baselines
# -----------------------------------------------------------------------------
def _assignments(U, R, rpu):
    """All ways to give each of U users a distinct set of rpu RUs out of R."""
    rus = range(R)
    for combo in itertools.permutations(rus, U * rpu):
        grp = [tuple(sorted(combo[i * rpu : (i + 1) * rpu])) for i in range(U)]
        # dedupe within-group order by only yielding sorted groups once
        if all(grp[i] == tuple(sorted(grp[i])) for i in range(U)):
            yield [list(g) for g in grp]


def _uniq_assignments(U, R, rpu):
    seen = set()
    out = []
    for a in _assignments(U, R, rpu):
        key = tuple(tuple(g) for g in a)
        if key not in seen:
            seen.add(key)
            out.append(a)
    return out


def floor_of(user_rus, csi):
    s = _served(user_rus, csi)
    return min(s) if s else 0.0


# -----------------------------------------------------------------------------
# loaders
# -----------------------------------------------------------------------------
def build_grids_swaplog(path, min_users):
    """swap.jsonl -> list of csi[U][R] grids (per-RU, as captured)."""
    grids = []
    for ln in open(path):
        ln = ln.strip()
        if not ln:
            continue
        try:
            d = json.loads(ln)
        except json.JSONDecodeError:
            continue
        real = [
            u
            for u in d.get("users", [])
            if u.get("rnti", 0) != 0 and max(u.get("csi", [0])) > 1e-7
        ]
        if len(real) >= min_users:
            grids.append([list(map(float, u["csi"])) for u in real])
    # square up to a common (U,R)
    if not grids:
        return []
    U = min(len(g) for g in grids)
    R = min(len(g[0]) for g in grids)
    return [[row[:R] for row in g[:U]] for g in grids]


def build_grids_rb(path, min_users, rbs_per_ru):
    """per-RB log -> per-RU grid by summing power over groups of rbs_per_ru RBs.
    EXPECTED per-line schema (adjust field name to your rb.jsonl):
        {"slot":..,"users":[{"u":..,"rnti":..,"rb_csi":[p0,p1,...perRB...]}]}
    """
    grids = []
    for ln in open(path):
        ln = ln.strip()
        if not ln:
            continue
        try:
            d = json.loads(ln)
        except json.JSONDecodeError:
            continue
        us = [
            u
            for u in d.get("users", [])
            if u.get("rnti", 0) != 0 and sum(u.get("rb_csi", [0])) > 1e-9
        ]
        if len(us) < min_users:
            continue
        grid = []
        for u in us:
            rb = list(map(float, u["rb_csi"]))
            nru = len(rb) // rbs_per_ru
            grid.append(
                [sum(rb[k * rbs_per_ru : (k + 1) * rbs_per_ru]) for k in range(nru)]
            )
        grids.append(grid)
    if not grids:
        return []
    U = min(len(g) for g in grids)
    R = min(len(g[0]) for g in grids)
    return [[row[:R] for row in g[:U]] for g in grids]


def synth_grids(U, rpu, slots, seed=42, alpha=0.95):
    """ADDED: synthetic temporally-correlated fading CSI (AR(1) on |N(0,1)|)."""
    rng = np.random.default_rng(seed)
    R = U * rpu
    grids = []
    pl = 0.5 + 0.5 * rng.random(U)
    H = np.array([pl[u] * np.abs(rng.standard_normal(R)) for u in range(U)])
    for t in range(slots):
        if t > 0:
            H = alpha * H + (1 - alpha) * (
                pl[:, None] * np.abs(rng.standard_normal((U, R)))
            )
        grids.append([list(H[u]) for u in range(U)])
    return grids


# -----------------------------------------------------------------------------
def run(grids, rpu, epsilon, do_optimal):
    U = len(grids[0])
    R = len(grids[0][0])
    assert U * rpu <= R, f"need U*rpu={U*rpu} <= R={R} RUs"

    # common warm-start init: first rpu RUs to user 0, next rpu to user 1, ...
    init = [list(range(u * rpu, u * rpu + rpu)) for u in range(U)]

    pol = {"WRF": policy_wrf, "WUF": policy_wuf}
    state = {k: copy.deepcopy(init) for k in pol}
    floors = {k: [] for k in pol}
    nops = {k: 0 for k in pol}
    tus = {k: 0.0 for k in pol}

    # best-fixed (brute force over run, on mean floor)
    asgs = _uniq_assignments(U, R, rpu)
    bf_mean = [np.mean([floor_of(a, g) for g in grids]) for a in asgs]
    best_fixed = asgs[int(np.argmax(bf_mean))]
    fix_floor = np.array([floor_of(best_fixed, g) for g in grids])

    opt_floor = None
    if do_optimal:
        opt_floor = np.array([max(floor_of(a, g) for a in asgs) for g in grids])

    for g in grids:
        for k, fn in pol.items():
            t0 = time.perf_counter()
            state[k], st = fn(state[k], g, epsilon=epsilon)
            tus[k] += (time.perf_counter() - t0) * 1e6
            nops[k] += st["swaps"] + st["replaces"]
            floors[k].append(floor_of(state[k], g))

    out = {k: np.array(v) for k, v in floors.items()}
    out["BEST_FIXED"] = fix_floor
    if opt_floor is not None:
        out["OPTIMAL"] = opt_floor
    meta = {
        "U": U,
        "R": R,
        "rpu": rpu,
        "slots": len(grids),
        "best_fixed": best_fixed,
        "nops": nops,
        "alloc_us_mean": {k: tus[k] / len(grids) for k in pol},
        "n_assignments": len(asgs),
    }
    return out, meta


def db(x):
    return 10 * np.log10(np.maximum(x, EPS))


def summarize(out, meta):
    lines = []
    lines.append(
        f"slots={meta['slots']} U={meta['U']} R={meta['R']} "
        f"rus_per_user={meta['rpu']} assignments={meta['n_assignments']}"
    )
    lines.append(f"best-fixed user->RUs: {meta['best_fixed']}")
    lines.append("")
    ref = "BEST_FIXED"
    order = [k for k in ["WRF", "WUF", "OPTIMAL", "BEST_FIXED"] if k in out]
    fm = db(out[ref]).mean()
    hdr = f"{'policy':<11} {'meanfloor(dB)':>13} {'@1%out(dB)':>11} {'gain/fixed':>11} {'swaps':>8}"
    lines.append(hdr)
    lines.append("-" * len(hdr))
    for k in order:
        d = db(out[k])
        p1 = np.percentile(d, 1)
        gain = d.mean() - fm
        nops = meta["nops"].get(k, 0)
        lines.append(f"{k:<11} {d.mean():>13.2f} {p1:>11.2f} {gain:>+11.2f} {nops:>8}")
    lines.append("")
    if "WRF" in out and "WUF" in out:
        same = np.allclose(out["WRF"], out["WUF"])
        diff = db(out["WUF"]).mean() - db(out["WRF"]).mean()
        lines.append(f"WUF vs WRF: identical={same}  mean-floor diff={diff:+.3f} dB")
        lines.append(
            f"alloc_us/decision (python, NOT the gNB CPU): "
            f"WRF={meta['alloc_us_mean']['WRF']:.2f}  "
            f"WUF={meta['alloc_us_mean']['WUF']:.2f}"
        )
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("swaplog", nargs="?", help="swap.jsonl (per-RU csi)")
    ap.add_argument("--rb", help="per-RB log instead (regroup with --rbs-per-ru)")
    ap.add_argument("--rbs-per-ru", type=int, default=8)
    ap.add_argument("--rus-per-user", type=int, default=1)
    ap.add_argument(
        "--epsilon",
        type=int,
        default=1,
        help="how many worst RUs (WRF) / weakest users (WUF) per period",
    )
    ap.add_argument("--min-users", type=int, default=3)
    ap.add_argument(
        "--no-optimal",
        action="store_true",
        help="skip per-slot optimum (brute force) for speed",
    )
    ap.add_argument("-o", "--out", default="policy_cmp")
    a = ap.parse_args()

    if a.rb:
        grids = build_grids_rb(a.rb, a.min_users, a.rbs_per_ru)
    elif a.swaplog:
        grids = build_grids_swaplog(a.swaplog, a.min_users)
    else:
        ap.error("give a swap.jsonl or --rb <per-RB log>")
    if not grids:
        sys.exit("no usable slots")

    out, meta = run(grids, a.rus_per_user, a.epsilon, do_optimal=not a.no_optimal)
    report = summarize(out, meta)
    print(report)

    with open(a.out + ".txt", "w") as f:
        f.write(report + "\n")
    # per-slot weakest floors for every policy (so you can re-plot / audit)
    with open(a.out + ".jsonl", "w") as f:
        keys = list(out.keys())
        for i in range(meta["slots"]):
            f.write(json.dumps({k: float(out[k][i]) for k in keys}) + "\n")
    print("wrote", a.out + ".txt", "and", a.out + ".jsonl")


if __name__ == "__main__":
    main()
