#!/usr/bin/env python3
"""plot_outage.py — weakest-user OUTAGE curve from the swap log (power only).

Reads swap_metrics.jsonl. Plots the weakest-user power distribution as an
OUTAGE curve (the paper's metric, eq 13):

    outage prob Pout = P( weakest-user power < Gamma_th )

so reading the curve at a given outage probability gives the outage threshold
Gamma_th at that reliability level. Two curves:

    no swap : each user frozen on the BEST FIXED assignment (brute-forced over
              all permutations -> the strongest static baseline, not random)
    swap    : the weakest-user floor the scheduler actually achieved (served)

The swap curve sits at higher power for every outage probability; the gain is
largest in the tail (low Pout) -- exactly where URLLC reliability is set.

Power only: 10*log10 of the measured CSI. No rate/throughput modelling.

Usage:
  python3 plot_outage.py swap.jsonl -o outage.png [--min-users 3]
"""

import argparse, json, sys, itertools
import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

EPS = 1e-12


def assignments(U, R, rpu):
    """All ways to give each of U users a distinct set of `rpu` RUs out of R.
    For rpu==1 this is exactly the old permutations(range(R), U)."""
    seen, out = set(), []
    for combo in itertools.permutations(range(R), U * rpu):
        grp = tuple(tuple(sorted(combo[i * rpu : (i + 1) * rpu])) for i in range(U))
        if grp in seen:
            continue
        seen.add(grp)
        out.append([list(g) for g in grp])
    return out


def floor_series(asg, csis, U):
    """Per-slot weakest-user floor (served = sum of CSI over a user's RUs)."""
    return np.array([min(sum(c[u][r] for r in asg[u]) for u in range(U)) for c in csis])


def all_floors(C, asgs, U):
    """Weakest-user floor for EVERY assignment, vectorised over all slots.
    C is (n_slots, U, R); returns (n_assignments, n_slots). ~100x faster than
    looping floor_series per assignment."""
    out = np.empty((len(asgs), C.shape[0]))
    for i, asg in enumerate(asgs):
        served = np.stack([C[:, u, asg[u]].sum(axis=1) for u in range(U)], axis=1)
        out[i] = served.min(axis=1)
    return out


def load(path, min_users):
    slots = []
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
            slots.append(real)
    return slots


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("swaplog")
    ap.add_argument("-o", "--out", default="outage.png")
    ap.add_argument("--min-users", type=int, default=3)
    ap.add_argument(
        "--rus-per-user",
        type=int,
        default=1,
        help="RUs owned per user (R>=2 brute-forces group assignments)",
    )
    ap.add_argument("--label", default=None)
    a = ap.parse_args()

    slots = load(a.swaplog, a.min_users)
    if not slots:
        sys.exit("no usable >= %d-user slots in %s" % (a.min_users, a.swaplog))
    U = min(len(s) for s in slots)

    # per-slot achieved floor (swap) and the per-user csi matrix
    ach, csis = [], []
    for us in slots:
        us = us[:U]
        ach.append(min(u["served"] for u in us))
        csis.append([np.asarray(u["csi"], float) for u in us])
    ach = np.array(ach)

    # BEST FIXED assignment: brute force all ways to give each user rpu distinct
    # RUs, pick the one with the highest mean weakest-user floor over the run.
    R = len(csis[0][0])
    rpu = a.rus_per_user
    if U * rpu > R:
        sys.exit("need U*rus_per_user=%d <= R=%d RUs" % (U * rpu, R))
    asgs = assignments(U, R, rpu)
    C = np.asarray(csis, float)  # (n_slots, U, R)
    af = all_floors(C, asgs, U)  # (n_assignments, n_slots)
    best_i = int(af.mean(axis=1).argmax())
    best_asg = asgs[best_i]
    fix = af[best_i]
    opt = af.max(axis=0)  # per-slot max-min optimum (oracle)

    sw_db = 10 * np.log10(np.maximum(ach, EPS))
    fx_db = 10 * np.log10(np.maximum(fix, EPS))
    op_db = 10 * np.log10(np.maximum(opt, EPS))

    # OUTAGE curve: Pout(x) = fraction of slots with floor < x  (== empirical CDF)
    def outage(x):
        xs = np.sort(x)
        p = np.arange(1, len(xs) + 1) / len(xs)
        return xs, p

    xf, pf = outage(fx_db)
    xs, ps = outage(sw_db)
    xo, po = outage(op_db)

    fig, ax = plt.subplots(figsize=(9.5, 6))
    ax.semilogy(
        xf, pf, color="tab:red", lw=2.4, label="no swap  (best fixed assignment)"
    )
    ax.semilogy(xs, ps, color="tab:blue", lw=2.4, label="swap  (max-min)")
    ax.semilogy(
        xo, po, color="tab:green", lw=2.0, ls="--", label="optimal  (per-slot max-min)"
    )
    ax.set_xlabel("weakest-user power  $\\Gamma_{th}$  (dB)", fontsize=14)
    ax.set_ylabel(
        "outage probability  $P(\\,\\Gamma_{min} < \\Gamma_{th}\\,)$", fontsize=14
    )
    ax.set_title("Weakest-user outage", fontsize=12.5)
    ax.grid(alpha=0.3, which="both")
    ax.legend(fontsize=12, loc="lower right")
    ax.set_ylim(max(1.0 / len(sw_db), 5e-4), 1.0)

    # annotate the gain at 1% and 5% outage
    lines = []
    for pout in [0.01, 0.05, 0.10]:
        gf = np.percentile(fx_db, pout * 100)
        gs = np.percentile(sw_db, pout * 100)
        lines.append((pout, gf, gs, gs - gf))

    fig.tight_layout()
    fig.savefig(a.out, dpi=140)
    plt.close(fig)

    txt = a.out.rsplit(".", 1)[0] + ".txt"
    with open(txt, "w") as f:
        lbl = a.label or a.swaplog.split("/")[-1]
        f.write(
            f"weakest-user outage — {lbl}\nslots={len(slots)} users={U} RUs={R} "
            f"rus_per_user={rpu}\n"
        )
        f.write(f"best fixed assignment (brute-forced): user->RUs {best_asg}\n\n")
        f.write("outage threshold Gamma_th (dB) at each outage probability:\n")
        for pout, gf, gs, g in lines:
            go = np.percentile(op_db, pout * 100)
            f.write(
                f"  Pout={pout:>5.0%}:  no-swap={gf:7.2f}   swap={gs:7.2f}   "
                f"optimal={go:7.2f}   gain={g:+.2f} dB   (swap->opt gap={go-gs:.2f} dB)\n"
            )
        f.write(
            f"  mean floor:  no-swap={fx_db.mean():.2f}   swap={sw_db.mean():.2f}   "
            f"optimal={op_db.mean():.2f}   gain={sw_db.mean()-fx_db.mean():+.2f} dB   "
            f"(swap->opt gap={op_db.mean()-sw_db.mean():.2f} dB)\n"
        )
    for pout, gf, gs, g in lines:
        print(f"Pout={pout:.0%}: no-swap={gf:.2f} swap={gs:.2f} gain={g:+.2f} dB")
    print(
        f"mean: no-swap={fx_db.mean():.2f} swap={sw_db.mean():.2f} gain={sw_db.mean()-fx_db.mean():+.2f} dB"
    )
    print("wrote", a.out, "and", txt)


if __name__ == "__main__":
    main()
