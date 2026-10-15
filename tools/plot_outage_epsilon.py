#!/usr/bin/env python3
"""plot_outage_epsilon.py — weakest-user OUTAGE vs the swap depth epsilon.

ONE capture, several epsilons. epsilon in this scheme is the number of worst
RUs (WRF) / weakest users (WUF) reconsidered for a swap each period, NOT a dB
threshold. The gNB runs a single epsilon per capture, but the swap decision is
a pure function of (CSI snapshot, current assignment), and the per-RU CSI grid
is logged — so every epsilon can be REPLAYED offline from the same data, each
evolving its own warm-started assignment. The WRF/WUF port is imported from
swap_policies.py (the validated single source of truth), not re-derived here.

The offline epsilon that matches the capture's own epsilon should reproduce the
gNB's logged weakest-user floor; the other epsilons show how search depth trades
off against the achieved outage. Baselines (best-fixed, per-slot optimum) are
identical across epsilon and drawn once.

Power only: 10*log10 of the measured CSI. No rate/throughput modelling.

Usage:
  python3 plot_outage_epsilon.py swap.jsonl -o outage_eps.png
  python3 plot_outage_epsilon.py swap.jsonl -o outage_eps.png --eps-list 1,2,3,4
"""

import argparse, json, sys, copy
import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

# the validated WRF/WUF port + loaders live in swap_policies.py (same folder)
try:
    from swap_policies import (
        policy_wrf,
        policy_wuf,
        build_grids_swaplog,
        _uniq_assignments,
        floor_of,
    )
except ImportError:
    sys.exit(
        "cannot import swap_policies.py — keep it in the same folder as this script"
    )

EPS = 1e-12


def read_meta(path):
    """Scrape policy / rus_per_user / epsilon from the log (gNB-written fields)."""
    meta = {"policy": None, "rus_per_user": None, "epsilon": None}
    for ln in open(path):
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
                meta["epsilon"] = int(ev)
        if all(meta[k] is not None for k in meta):
            break
    return meta


def logged_floor(path, min_users, U):
    """The gNB's own achieved weakest-user floor (served), for the trust check."""
    ach = []
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
            ach.append(min(u["served"] for u in real[:U]))
    return np.array(ach)


def replay(grids, rpu, epsilon, policy_fn):
    """Warm-started trajectory of one policy at one epsilon -> per-slot floor."""
    U = len(grids[0])
    init = [list(range(u * rpu, u * rpu + rpu)) for u in range(U)]
    state = copy.deepcopy(init)
    floors = np.empty(len(grids))
    for i, g in enumerate(grids):
        state, _ = policy_fn(state, g, epsilon=epsilon)
        floors[i] = floor_of(state, g)
    return floors


def all_floors(C, asgs, U):
    """Weakest-user floor for EVERY assignment, vectorised over all slots."""
    out = np.empty((len(asgs), C.shape[0]))
    for i, asg in enumerate(asgs):
        served = np.stack([C[:, u, asg[u]].sum(axis=1) for u in range(U)], axis=1)
        out[i] = served.min(axis=1)
    return out


def outage(x):
    xs = np.sort(x)
    return xs, np.arange(1, len(xs) + 1) / len(xs)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("swaplog")
    ap.add_argument("-o", "--out", default="outage_eps.png")
    ap.add_argument("--min-users", type=int, default=3)
    ap.add_argument(
        "--rus-per-user", type=int, default=None, help="default: read from log, else 1"
    )
    ap.add_argument(
        "--policy", default=None, help="wrf|wuf; default: read from log, else wrf"
    )
    ap.add_argument("--eps-list", default="1,2,3,4", help="comma list of epsilons")
    ap.add_argument("--label", default=None)
    a = ap.parse_args()

    meta = read_meta(a.swaplog)
    grids = build_grids_swaplog(a.swaplog, a.min_users)
    if not grids:
        sys.exit("no usable slots")
    U = len(grids[0])
    R = len(grids[0][0])

    rpu = a.rus_per_user or meta["rus_per_user"] or 1
    if U * rpu > R:
        sys.exit("need U*rus_per_user=%d <= R=%d RUs" % (U * rpu, R))
    pol_name = (a.policy or meta["policy"] or "wrf").lower()
    policy_fn = policy_wuf if pol_name == "wuf" else policy_wrf
    eps_list = [int(x) for x in a.eps_list.split(",") if x.strip()]

    # baselines (identical across epsilon)
    C = np.asarray(grids, float)  # (n_slots, U, R)
    asgs = _uniq_assignments(U, R, rpu)
    af = all_floors(C, asgs, U)
    fix = af[int(af.mean(axis=1).argmax())]
    opt = af.max(axis=0)
    fx_db = 10 * np.log10(np.maximum(fix, EPS))
    op_db = 10 * np.log10(np.maximum(opt, EPS))

    # swap replay per epsilon
    sw = {}
    for e in eps_list:
        sw[e] = 10 * np.log10(np.maximum(replay(grids, rpu, e, policy_fn), EPS))

    # trust check: offline floor at the capture's own epsilon vs the logged floor
    cap_eps = meta["epsilon"]  # the epsilon the gNB actually ran (if logged)
    logf = logged_floor(a.swaplog, a.min_users, U)
    logf_db = 10 * np.log10(np.maximum(logf[: len(grids)], EPS))

    # ---- plot ----
    fig, ax = plt.subplots(figsize=(9.5, 6))
    xf, pf = outage(fx_db)
    xo, po = outage(op_db)
    ax.semilogy(xf, pf, color="tab:red", lw=2.2, label="no swap  (best fixed)")
    # epsilon curves: light -> dark blue for increasing depth
    shades = plt.cm.Blues(np.linspace(0.45, 0.95, len(eps_list)))
    for e, c in zip(eps_list, shades):
        xe, pe = outage(sw[e])
        ax.semilogy(
            xe, pe, color=c, lw=2.2,
            label=f"swap  {pol_name.upper()}  $\\epsilon$={e}",
        )
    ax.semilogy(
        xo, po, color="tab:green", lw=2.0, ls="--", label="optimal  (per-slot max-min)"
    )
    ax.set_xlabel("weakest-user power  $\\Gamma_{th}$  (dB)", fontsize=14)
    ax.set_ylabel(
        "outage probability  $P(\\,\\Gamma_{min} < \\Gamma_{th}\\,)$", fontsize=14
    )
    ax.set_title(
        f"Weakest-user outage  —  {rpu} RBG/user  ({pol_name.upper()} $\\epsilon$-sweep)",
        fontsize=12.5,
    )
    ax.grid(alpha=0.3, which="both")
    ax.legend(fontsize=10.5, loc="lower right")
    ax.set_ylim(max(1.0 / len(grids), 5e-4), 1.0)
    fig.tight_layout()
    fig.savefig(a.out, dpi=140)
    plt.close(fig)

    # ---- text ----
    txt = a.out.rsplit(".", 1)[0] + ".txt"
    with open(txt, "w") as f:
        lbl = a.label or a.swaplog.split("/")[-1]
        f.write(
            f"weakest-user outage — epsilon sweep — {lbl}\n"
            f"slots={len(grids)} users={U} RUs={R} rus_per_user={rpu} "
            f"policy={pol_name} (from {'log' if meta['policy'] else 'default/cli'})\n\n"
        )
        f.write(
            f"{'curve':<14}{'mean(dB)':>10}{'@1%(dB)':>10}{'@5%(dB)':>10}"
            f"{'gain/fixed':>12}{'opt gap':>9}\n"
        )
        f.write("-" * 65 + "\n")

        def row(name, d):
            g = d.mean() - fx_db.mean()
            gap = op_db.mean() - d.mean()
            f.write(
                f"{name:<14}{d.mean():>10.2f}{np.percentile(d,1):>10.2f}"
                f"{np.percentile(d,5):>10.2f}{g:>+12.2f}{gap:>9.2f}\n"
            )

        row("best-fixed", fx_db)
        for e in eps_list:
            row(f"{pol_name} eps={e}", sw[e])
        row("optimal", op_db)
        cap_txt = f"eps={cap_eps}" if cap_eps is not None else "unknown (not logged)"
        f.write(
            f"\nTRUST CHECK: gNB logged floor (its own epsilon: {cap_txt}): "
            f"mean={logf_db.mean():.2f} dB, @1%={np.percentile(logf_db,1):.2f} dB\n"
            f"  -> compare to the offline {cap_txt} row above; they should match "
            f"within a few hundredths dB (warm-start washes out over the run).\n"
        )
    for e in eps_list:
        print(
            f"{pol_name} eps={e}: mean={sw[e].mean():.2f} dB  "
            f"@1%={np.percentile(sw[e],1):.2f} dB  "
            f"gain/fixed={sw[e].mean()-fx_db.mean():+.2f} dB"
        )
    print(
        f"gNB logged (own eps): mean={logf_db.mean():.2f} dB  "
        f"@1%={np.percentile(logf_db,1):.2f} dB   [trust-check vs offline eps=4]"
    )
    print("wrote", a.out, "and", txt)


if __name__ == "__main__":
    main()
