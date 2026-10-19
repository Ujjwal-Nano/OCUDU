#!/usr/bin/env python3
"""plot_outage.py — weakest-user OUTAGE curve from the swap log (power only).

Reads swap_metrics.jsonl. Plots the weakest-user power distribution as an
OUTAGE curve (the paper's metric, eq 13):

    outage prob Pout = P( weakest-user power < Gamma_th )

so reading the curve at a given outage probability gives the outage threshold
Gamma_th at that reliability level. Curves:

    no swap (best fixed)        : every user frozen on the single best static
                                  assignment (brute-forced -> strongest static
                                  baseline).
    no swap (worst fixed)       : frozen on the single WORST static assignment
                                  (brute-forced -> weakest static baseline).
    no swap (random per slot)   : channel-blind allocator that picks a random
                                  assignment each slot; its floor distribution
                                  is exactly the pool of all assignments over
                                  all slots (ensemble, no Monte-Carlo noise).
    swap (max-min)              : weakest-user floor the scheduler achieved.
    optimal (per-slot max-min)  : oracle best assignment every slot.

The swap curve sits at higher power for every outage probability; the gain is
largest in the tail (low Pout) -- exactly where URLLC reliability is set. The
best/worst fixed and random curves bracket what any static or blind scheme can
do, so the swap gain can be read against all three.

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


def floor_series_C(C, asg, U):
    """Weakest-user floor over all slots for ONE assignment -> (n_slots,)."""
    served = np.stack([C[:, u, asg[u]].sum(axis=1) for u in range(U)], axis=1)
    return served.min(axis=1)


def stream_baselines(C, asgs, U, rnd_cap=2_000_000):
    """Memory-safe single pass over every fixed assignment.

    The old all_floors() built the full (n_assignments, n_slots) float64 matrix
    (2520 x 54485 ~ 1.1 GB at U=4 / R=8, then .ravel() copied it again for the
    random pool -> ~3 GB peak -> the kernel OOM-killed it: 'Killed'). This keeps
    only the per-slot running optimum, the per-assignment mean floor, and a
    bounded sub-sample for the channel-blind 'random' curve, so peak memory is
    O(n_slots) instead of O(n_assignments x n_slots).

    Returns (means, opt, rnd):
      means (n_assignments,) mean weakest-user floor of each fixed assignment
      opt   (n_slots,)       per-slot max over assignments (max-min oracle)
      rnd   (<=rnd_cap,)     pooled floors, systematically sub-sampled, for the
                             random-per-slot CDF (statistically identical pool)
    """
    n_slots = C.shape[0]
    opt = np.full(n_slots, -np.inf)
    means = np.empty(len(asgs))
    stride = max(1, (len(asgs) * n_slots) // max(1, rnd_cap))
    rnd_parts = []
    for i, asg in enumerate(asgs):
        fl = floor_series_C(C, asg, U)
        means[i] = fl.mean()
        np.maximum(opt, fl, out=opt)
        # .copy() is essential: a strided slice is a VIEW that pins the whole
        # fl array alive, so without it all 2520 fl's stay in memory (~1.1 GB,
        # re-creating the very OOM this rewrite removes).
        rnd_parts.append(fl[::stride].copy())
    rnd = np.concatenate(rnd_parts) if rnd_parts else np.empty(0)
    return means, opt, rnd


def load(path, min_users):
    """Return (slots, meta). meta carries the gNB-side policy ('wrf'/'wuf') and
    rus_per_user as logged by the scheduler, so the plot can label the swap
    curve by the yaml policy and brute-force the baselines at the SAME RU
    granularity the gNB actually used (prevents swap appearing to beat the
    oracle when R>=2 is plotted with the wrong rus_per_user)."""
    slots = []
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
                meta["epsilon"] = float(ev)
        real = [
            u
            for u in d.get("users", [])
            if u.get("rnti", 0) != 0 and max(u.get("csi", [0])) > 1e-7
        ]
        if len(real) >= min_users:
            slots.append(real)
    return slots, meta


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("swaplog")
    ap.add_argument("-o", "--out", default="outage.png")
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
        help="swap ε (number of worst RUs reconsidered each period) shown in "
        "the swap legend; default: read from log if present, else omitted",
    )
    ap.add_argument("--label", default=None)
    a = ap.parse_args()

    slots, meta = load(a.swaplog, a.min_users)
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

    # brute force all ways to give each user rpu distinct RUs -> floor per slot
    R = len(csis[0][0])
    # RU granularity: explicit CLI flag wins; otherwise take what the gNB logged
    # (so the baselines are brute-forced at the same granularity the scheduler
    # actually used); fall back to 1 for old captures without the field.
    if a.rus_per_user is not None:
        rpu, rpu_src = a.rus_per_user, "cli"
    elif meta["rus_per_user"]:
        rpu, rpu_src = meta["rus_per_user"], "log"
    else:
        rpu, rpu_src = 1, "default"
    if U * rpu > R:
        sys.exit("need U*rus_per_user=%d <= R=%d RUs" % (U * rpu, R))
    # swap ε (worst RUs reconsidered per period) for the legend:
    # CLI wins, else log, else none
    eps = a.epsilon if a.epsilon is not None else meta["epsilon"]
    asgs = assignments(U, R, rpu)
    C = np.asarray(csis, float)  # (n_slots, U, R)
    # memory-safe streaming pass (see stream_baselines): no full
    # (n_assignments x n_slots) matrix, so U=4/R=8 no longer OOMs.
    means, opt, rnd = stream_baselines(C, asgs, U)
    best_i = int(means.argmax())
    worst_i = int(means.argmin())
    best_asg = asgs[best_i]
    worst_asg = asgs[worst_i]
    fix = floor_series_C(C, best_asg, U)    # no swap, best fixed
    wfix = floor_series_C(C, worst_asg, U)  # no swap, worst fixed
    # opt: per-slot max-min optimum (oracle), from the streaming pass.
    # rnd: no swap, random allocation per slot -> pool of every assignment over
    # every slot, systematically sub-sampled so the CDF is unchanged but the
    # 2520x54485 ravel (~1.1 GB) that caused the OOM is gone.

    sw_db = 10 * np.log10(np.maximum(ach, EPS))
    fx_db = 10 * np.log10(np.maximum(fix, EPS))
    wf_db = 10 * np.log10(np.maximum(wfix, EPS))
    rn_db = 10 * np.log10(np.maximum(rnd, EPS))
    op_db = 10 * np.log10(np.maximum(opt, EPS))

    # OUTAGE curve: Pout(x) = fraction of slots with floor < x  (== empirical CDF)
    def outage(x):
        xs = np.sort(x)
        p = np.arange(1, len(xs) + 1) / len(xs)
        return xs, p

    xf, pf = outage(fx_db)
    xw, pw = outage(wf_db)
    xr, pr = outage(rn_db)
    xs, ps = outage(sw_db)
    xo, po = outage(op_db)

    fig, ax = plt.subplots(figsize=(9.5, 6))
    ax.semilogy(
        xf, pf, color="tab:red", lw=2.4, label="no swap  (best fixed)"
    )
    ax.semilogy(
        xr, pr, color="tab:purple", lw=2.0, ls="-.",
        label="no swap  (randomly allocated per slot)",
    )
    ax.semilogy(
        xw, pw, color="tab:brown", lw=2.0, ls=":", label="no swap  (worst fixed)"
    )
    # swap curve labelled by the gNB policy (from the log); falls back to the
    # generic objective name for captures that predate policy logging.
    pol = meta["policy"]
    base = pol.upper() if pol else "max-min"
    swap_label = (
        f"swap  ({base}, $\\epsilon$={eps:g})" if eps is not None
        else f"swap  ({base})"
    )
    ax.semilogy(xs, ps, color="tab:blue", lw=2.4, label=swap_label)
    ax.semilogy(
        xo, po, color="tab:green", lw=2.0, ls="--", label="optimal  (per-slot max-min)"
    )
    ax.set_xlabel("weakest-user power  $\\Gamma_{th}$  (dB)", fontsize=14)
    ax.set_ylabel(
        "outage probability  $P(\\,\\Gamma_{min} < \\Gamma_{th}\\,)$", fontsize=14
    )
    ax.set_title(
        f"Weakest-user outage  —  {rpu} RBG/user", fontsize=12.5
    )
    ax.grid(alpha=0.3, which="both")
    ax.legend(fontsize=11, loc="lower right")
    ax.set_ylim(max(1.0 / len(sw_db), 5e-4), 1.0)

    # gain of swap over the best-fixed baseline at a few outage probabilities
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
            f"rus_per_user={rpu} (from {rpu_src})   policy={pol or 'n/a'}\n"
        )
        f.write(f"best  fixed assignment (brute-forced): user->RUs {best_asg}\n")
        f.write(f"worst fixed assignment (brute-forced): user->RUs {worst_asg}\n\n")
        f.write("outage threshold Gamma_th (dB) at each outage probability:\n")
        for pout, gf, gs, g in lines:
            gw = np.percentile(wf_db, pout * 100)
            gr = np.percentile(rn_db, pout * 100)
            go = np.percentile(op_db, pout * 100)
            f.write(
                f"  Pout={pout:>5.0%}:  worst-fixed={gw:7.2f}   random={gr:7.2f}   "
                f"best-fixed={gf:7.2f}   swap={gs:7.2f}   optimal={go:7.2f}   "
                f"gain(swap vs best-fixed)={g:+.2f} dB   (swap->opt gap={go-gs:.2f} dB)\n"
            )
        f.write(
            f"  mean floor:  worst-fixed={wf_db.mean():.2f}   random={rn_db.mean():.2f}   "
            f"best-fixed={fx_db.mean():.2f}   swap={sw_db.mean():.2f}   "
            f"optimal={op_db.mean():.2f}   gain(swap vs best-fixed)="
            f"{sw_db.mean()-fx_db.mean():+.2f} dB   "
            f"(swap->opt gap={op_db.mean()-sw_db.mean():.2f} dB)\n"
        )
    for pout, gf, gs, g in lines:
        print(f"Pout={pout:.0%}: best-fixed={gf:.2f} swap={gs:.2f} gain={g:+.2f} dB")
    print(
        f"mean: worst-fixed={wf_db.mean():.2f} random={rn_db.mean():.2f} "
        f"best-fixed={fx_db.mean():.2f} swap={sw_db.mean():.2f} "
        f"gain={sw_db.mean()-fx_db.mean():+.2f} dB"
    )
    print(f"policy={pol or 'n/a'}  rus_per_user={rpu} (from {rpu_src})")
    print("wrote", a.out, "and", txt)


if __name__ == "__main__":
    main()