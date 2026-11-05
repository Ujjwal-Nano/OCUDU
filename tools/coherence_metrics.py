#!/usr/bin/env python3
"""coherence_metrics.py — per-user COHERENCE TIME (T_c) and COHERENCE BANDWIDTH
(B_c) from a per-RB SRS capture. A trimmed cut of metrics_suite.py: only the two
coherence panels are kept (temporal autocorrelation -> T_c, frequency correlation
-> B_c); regret, lifetimes, the RBG×RBG heatmap, the per-pair scatter and the
cross-user panel are removed.

Multi-user: pass --sessions ue_sessions.json (the --group-out from
correlate_rnti_imsi.py) and the raw capture is split by resolved IMSI BEFORE any
metric is computed — each coherence measure is only meaningful over ONE
continuous physical UE's own channel. One PNG + txt per user, plus a combined
summary (and optional CSV) listing T_c and B_c for ALL users. Without
--sessions the whole file is treated as one user.

Usage:
  python3 coherence_metrics.py CAP.rb.jsonl[.gz] -o out.png --K 6 \
      [--sessions ue_sessions.json] [--trim-start 1.5] [--trim-end 1.0]
      [--reattach-guard 10] [--avg-win MS | --mobile] [--speed V]
      [--label NAME] [--csv coherence.csv]
"""

import argparse, gzip, json, os, sys
import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

RBBW = 0.36e6  # one RB's bandwidth (Hz)
LAMBDA = 3e8 / 3.75e9  # wavelength at 3.75 GHz (for T_c theory)


def load_raw(path):
    """Load the full capture, chronological, no trimming/splitting yet."""
    op = gzip.open if path.endswith(".gz") else open
    T, P, RN = [], [], []
    with op(path, "rt") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                d = json.loads(line)
            except json.JSONDecodeError:
                continue
            T.append(d["t"])
            P.append(d["rb"])
            RN.append(d.get("rnti", -1))
    if not P:
        sys.exit("no samples in " + path)
    T = np.array(T, float)
    P = np.array(P, float)
    RN = np.array(RN, int)
    P = P[:, 1:]  # CRB0 never sounded
    return T, P, RN


def load_rnti_to_imsi(sessions_path):
    """Flatten ue_sessions.json (imsi -> [{rnti,...}]) into rnti(int) -> imsi."""
    with open(sessions_path) as f:
        grouped = json.load(f)
    rnti_to_imsi = {}
    for imsi, sessions in grouped.items():
        if imsi == "UNKNOWN_IMSI":
            continue
        for s in sessions:
            rnti_hex = s.get("rnti")
            if not rnti_hex:
                continue
            rnti_to_imsi[int(rnti_hex, 16)] = imsi
    return rnti_to_imsi


def compute_tc(M, fs, W):
    """Coherence time: 0.5-crossing of the per-RBG dB autocorrelation, averaged
    over RBGs. M = per-RBG dB series (time, R). Returns (Tc, lag, ac)."""
    R = M.shape[1]
    ac_all = []
    for rr in range(R):
        ys = np.convolve(M[:, rr], np.ones(W) / W, mode="valid")
        y = ys - ys.mean()
        maxlag = min(len(y) - 2, int(60 * fs))
        if maxlag < 2:
            continue
        ac_r = np.array([(y[: len(y) - d] * y[d:]).mean() for d in range(maxlag)])
        ac_r /= ac_r[0]
        ac_all.append(ac_r)
    if not ac_all:
        return np.nan, np.array([0.0]), np.array([1.0])
    L = min(len(a) for a in ac_all)
    ac = np.mean([a[:L] for a in ac_all], axis=0)
    lag = np.arange(len(ac)) / fs
    kk = np.where(ac < 0.5)[0]
    if len(kk) and kk[0] > 0:
        i = kk[0]
        frac = (ac[i - 1] - 0.5) / (ac[i - 1] - ac[i])
        Tc = lag[i - 1] + frac * (lag[i] - lag[i - 1])
    elif len(kk):
        Tc = lag[kk[0]]
    else:
        Tc = np.nan
    return Tc, lag, ac


def compute_bc(P, W, NRB):
    """Coherence bandwidth: 0.5-crossing of the RB frequency-correlation curve.
    Returns (Bc_MHz, df_MHz, cf)."""
    blk = W
    B = P[: len(P) // blk * blk].reshape(-1, blk, NRB).mean(1) if len(P) >= blk else P
    x = B / np.maximum(B.mean(0), 1e-15) - 1.0
    cf = np.array([(x[:, : NRB - d] * x[:, d:]).mean() for d in range(NRB)])
    cf /= cf[0]
    df = np.arange(NRB) * RBBW / 1e6
    Bc = df[np.argmax(cf < 0.5)] if (cf < 0.5).any() else np.nan
    return Bc, df, cf


def process_user(ukey, T, P, mins, fs, RN, a, txt_path):
    """T_c + B_c for one user's own (trimmed) slice. Writes a per-user txt and
    returns a dict with the scalars AND the curves (for the combined figure that
    main() draws), or None if too little data."""
    if len(T) < 5 or not np.isfinite(fs):
        print(f"user {ukey}: only {len(T)} sample(s) after trim -- skipping")
        return None

    # drop a guard window around each re-attach (RNTI change) — settling transient
    if a.reattach_guard > 0 and len(RN) > 1:
        g = int(round(a.reattach_guard * fs))
        keep = np.ones(len(RN), bool)
        cuts = [i for i in range(1, len(RN)) if RN[i] != RN[i - 1]]
        for c in cuts:
            keep[max(0, c - g) : min(len(RN), c + g)] = False
        if keep.sum() > 100:
            P, mins, RN = P[keep], mins[keep], RN[keep]
            ra = f"re-attach guard: {a.reattach_guard:.0f}s x2 around {len(cuts)} event(s)"
        else:
            ra = f"re-attach guard SKIPPED ({len(cuts)} events)"
    else:
        ra = "re-attach guard: none"

    NRB = P.shape[1]
    win_s = (
        (a.avg_win / 1000.0) if a.avg_win is not None else (0.060 if a.mobile else 1.0)
    )
    W = max(1, int(round(win_s * fs)))
    W = min(W, len(P))
    M = to_ru_db(P, a.K)
    R = M.shape[1]

    Tc, lag, ac = compute_tc(M, fs, W)
    Bc, df, cf = compute_bc(P, W, NRB)
    band = float(df[-1]) if len(df) else float("nan")  # widest measurable separation
    bc_disp = f"{Bc:.2f} MHz" if not np.isnan(Bc) else f">{band:.1f} MHz (flat in band)"
    Tc_theory = (0.42 * LAMBDA / a.speed) if a.speed else float("nan")

    L = [
        f"user         : {ukey}",
        f"file         : {a.cap}",
        f"samples      : {len(M)}   duration {mins[-1]-mins[0]:.1f} min   rate {fs:.1f}/s",
        f"avg window   : {win_s*1000:.0f} ms ({W} samples)",
        ra,
        f"RBs sounded  : {NRB}   RBG size K={a.K} → {R} RBGs",
        f"Tc(0.5)      : {Tc:.3f} s"
        + (f"   (theory {Tc_theory:.3f} s @ {a.speed} m/s)" if a.speed else ""),
        f"Bc(0.5)      : {bc_disp}   (RBG width K={a.K} = {a.K*RBBW/1e6:.2f} MHz)",
    ]
    if txt_path:
        open(txt_path, "w").write("\n".join(L) + "\n")
    print("\n".join(L))
    print()
    return {
        "ukey": ukey,
        "Tc": Tc,
        "Bc": Bc,
        "band": band,
        "Tc_theory": Tc_theory,
        "n": len(M),
        "dur_min": mins[-1] - mins[0],
        "fs": fs,
        "win_s": win_s,
        "lag": lag,
        "ac": ac,
        "df": df,
        "cf": cf,
    }


def draw_tc(ax, d):
    """T_c panel (temporal autocorrelation) for one user into a given axis."""
    lag, ac, Tc = d["lag"], d["ac"], d["Tc"]
    ax.plot(lag, ac, lw=1.8, color="#ff7f0e")
    ax.axhline(0.5, ls="--", c="k", lw=1)
    if not np.isnan(Tc):
        ax.plot(Tc, 0.5, "o", ms=8, mfc="w", mew=2, color="#ff7f0e")
        ax.text(Tc, 0.56, f" $T_c$={Tc:.2f}s", fontsize=9)
        axins = ax.inset_axes([0.55, 0.55, 0.42, 0.40])
        zmax = max(5 * Tc, 0.3)
        mz = lag <= zmax
        axins.plot(lag[mz], ac[mz], lw=1.6, color="#ff7f0e")
        axins.axhline(0.5, ls="--", c="k", lw=1)
        axins.plot(Tc, 0.5, "o", ms=6, mfc="w", mew=2, color="#ff7f0e")
        axins.set_xlim(0, zmax)
        axins.set_ylim(0.3, 1.02)
        axins.set_title(f"zoom $T_c$={Tc:.3f}s", fontsize=7.5)
        axins.tick_params(labelsize=6.5)
        axins.grid(alpha=0.3)
    ax.set_xlim(0, min(60, lag[-1]))
    ax.set_ylim(-0.05, 1.05)
    ax.set_xlabel("lag (s)")
    ax.set_ylabel("autocorrelation")
    ax.set_title(f"user {d['ukey']} — coherence time $T_c$", fontsize=10)
    ax.grid(alpha=0.3)


def draw_bc(ax, d, K):
    """B_c panel (frequency correlation) for one user into a given axis."""
    df, cf, Bc, band = d["df"], d["cf"], d["Bc"], d["band"]
    ax.plot(df, cf, lw=2, color="#1f77b4", zorder=3)
    ax.axhline(0.5, ls="--", c="k", lw=1, zorder=1)
    if not np.isnan(Bc):
        ax.axvline(Bc, ls=":", c="r", lw=2, zorder=2)
        ax.text(Bc + 0.2, 0.44, f"$B_c$≈{Bc:.1f} MHz", color="r", fontsize=9, va="top")
    else:
        ax.text(
            0.97,
            0.9,
            f"$B_c$ > {band:.0f} MHz\n(flat in band)",
            transform=ax.transAxes,
            color="r",
            fontsize=9,
            ha="right",
            va="top",
        )
    fx = K * RBBW / 1e6
    ax.axvline(fx, color="0.4", ls="-.", lw=1.2, zorder=1)
    ax.text(
        fx,
        1.02,
        f"RBG {fx:.2f} MHz",
        color="0.4",
        fontsize=7.5,
        ha="center",
        va="bottom",
    )
    ax.set_xlim(0, max(fx, (Bc if not np.isnan(Bc) else band)) + 1.5)
    ax.set_ylim(0, 1.12)
    ax.set_xlabel("frequency separation (MHz)")
    ax.set_ylabel("correlation")
    ax.set_title(f"user {d['ukey']} — coherence bandwidth $B_c$", fontsize=10)
    ax.grid(alpha=0.3, zorder=0)


def to_ru_db(P, K):
    R = P.shape[1] // K
    return 10 * np.log10(
        np.maximum(
            np.stack([P[:, r * K : (r + 1) * K].sum(1) for r in range(R)], 1), 1e-15
        )
    )


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cap")
    ap.add_argument("-o", "--out", default="coherence.png")
    ap.add_argument("--K", type=int, default=12, help="RBG size in RBs (= rbs_per_ru)")
    ap.add_argument("--trim-start", type=float, default=1.5)
    ap.add_argument("--trim-end", type=float, default=1.0)
    ap.add_argument("--reattach-guard", type=float, default=10.0)
    ap.add_argument("--mobile", action="store_true")
    ap.add_argument("--avg-win", type=float, default=None, help="averaging window (ms)")
    ap.add_argument(
        "--speed", type=float, default=None, help="UE speed (m/s) for Tc theory"
    )
    ap.add_argument("--label", default=None)
    ap.add_argument(
        "--csv", default=None, help="append a per-user Tc/Bc summary row here"
    )
    ap.add_argument(
        "--sessions",
        default=None,
        help="ue_sessions.json; splits the capture by resolved IMSI, one output per user",
    )
    a = ap.parse_args()

    T, P, RN = load_raw(a.cap)
    if a.sessions:
        r2i = load_rnti_to_imsi(a.sessions)
        ukeys = np.array([r2i.get(int(rn), None) for rn in RN], dtype=object)
        n_unmapped = int(sum(u is None for u in ukeys))
        if n_unmapped:
            print(
                f"NOTE: dropping {n_unmapped} record(s) with RNTIs not resolvable to a UE",
                file=sys.stderr,
            )
    else:
        ukeys = np.array(["single"] * len(RN), dtype=object)

    distinct = sorted(u for u in set(ukeys.tolist()) if u is not None)
    base, ext = os.path.splitext(a.out)
    results = []
    for ukey in distinct:
        mask = ukeys == ukey
        Tu, Pu, RNu = T[mask], P[mask], RN[mask]
        if len(Tu) < 2:
            continue
        minsu = (Tu - Tu[0]) / 60000.0
        k = (minsu >= a.trim_start) & (minsu <= minsu[-1] - a.trim_end)
        if k.sum() < 100:
            k = np.ones(len(minsu), bool)
        Tu, Pu, minsu, RNu = Tu[k], Pu[k], minsu[k], RNu[k]
        fsu = 1000.0 / np.median(np.diff(Tu)) if len(Tu) > 1 else float("nan")
        txt_path = f"{base}_{ukey}.txt" if a.sessions else f"{base}.txt"
        s = process_user(ukey, Tu, Pu, minsu, fsu, RNu, a, txt_path)
        if s is not None:
            results.append((ukey, s))

    if not results:
        sys.exit("no user had enough samples after trimming — nothing written")

    # ---- ONE combined figure: one row per user, [ T_c | B_c ] ----
    nr = len(results)
    fig, axes = plt.subplots(nr, 2, figsize=(13, 4.2 * nr), squeeze=False)
    for row, (ukey, s) in enumerate(results):
        draw_tc(axes[row, 0], s)
        draw_bc(axes[row, 1], s, a.K)
    fig.suptitle(
        f"Coherence metrics — {a.label or a.cap.split('/')[-1]} "
        f"({nr} user{'s' if nr != 1 else ''})",
        fontsize=13,
    )
    fig.tight_layout(rect=[0, 0, 1, 1 - 0.02 * (1 if nr > 1 else 2)])
    fig.savefig(a.out, dpi=120)
    plt.close(fig)
    print("wrote", a.out)

    # ---- combined summary across ALL users ----
    summary_txt = f"{base}_summary.txt"
    lines = [
        f"coherence summary — {a.label or a.cap.split('/')[-1]}",
        f"{'user':<18}{'Tc(s)':>9}{'Bc(MHz)':>10}{'dur(min)':>10}{'rate/s':>9}",
        "-" * 56,
    ]
    for ukey, s in results:
        bc = f"{s['Bc']:.2f}" if not np.isnan(s["Bc"]) else f">{s['band']:.0f}"
        lines.append(
            f"{str(ukey):<18}{s['Tc']:>9.3f}{bc:>10}{s['dur_min']:>10.1f}{s['fs']:>9.1f}"
        )
    rep = "\n".join(lines)
    print("\n" + rep)
    open(summary_txt, "w").write(rep + "\n")
    print("\nwrote", summary_txt)

    if a.csv:
        import csv

        newf = not os.path.exists(a.csv)
        with open(a.csv, "a", newline="") as f:
            fn = [
                "file",
                "imsi",
                "Tc_s",
                "Bc_MHz",
                "Tc_theory_s",
                "speed_mps",
                "dur_min",
                "rate_hz",
                "n",
            ]
            wr = csv.DictWriter(f, fieldnames=fn)
            if newf:
                wr.writeheader()
            for ukey, s in results:
                wr.writerow(
                    {
                        "file": a.label or a.cap.split("/")[-1],
                        "imsi": ukey,
                        "Tc_s": (
                            round(float(s["Tc"]), 4) if not np.isnan(s["Tc"]) else ""
                        ),
                        "Bc_MHz": (
                            round(float(s["Bc"]), 2) if not np.isnan(s["Bc"]) else ""
                        ),
                        "Tc_theory_s": round(s["Tc_theory"], 4) if a.speed else "",
                        "speed_mps": a.speed if a.speed is not None else "",
                        "dur_min": round(s["dur_min"], 2),
                        "rate_hz": round(s["fs"], 1),
                        "n": s["n"],
                    }
                )
        print(f"appended {len(results)} row(s) to {a.csv}")


if __name__ == "__main__":
    main()
