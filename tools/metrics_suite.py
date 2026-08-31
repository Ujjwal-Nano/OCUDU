#!/usr/bin/env python3
"""
metrics_suite.py — full P1 metric set from a per-RB SRS capture.

Usage:
    python3 metrics_suite.py CAPTURE.rb.jsonl[.gz] [-o out.png] [--K 12]
                             [--trim-start 1.5] [--trim-end 1.0] [--budget 0.5]

Produces a 6-panel figure and a text summary:
  A regret vs re-decision period T  -> T* (largest T with regret <= budget)
  B decision-lifetime CCDF for eps = 0.5/1/2/3 dB
  C frequency correlation -> coherence bandwidth Bc(0.5)
  D granularity sweep: opportunity G and separation vs RU size K
  E temporal autocorrelation -> coherence time Tc(0.5)
  F per-RU time series (context)

Input: lines of {"t":ms,"rnti":n,"ports":p,"rb":[...]} written by the PHY logger.
"""

import argparse, gzip, json, sys
import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

RBBW = 0.36e6  # 12 subcarriers x 30 kHz
LAMBDA = 3e8 / 3.75e9  # ~0.08 m


def load(path, trim_start, trim_end):
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
    RN = np.array(RN)
    P = P[:, 1:]  # drop CRB0 (never sounded)
    mins = (T - T[0]) / 60000.0
    k = (mins >= trim_start) & (mins <= mins[-1] - trim_end)
    if k.sum() < 100:
        k = np.ones(len(mins), bool)  # short capture: keep all
    T, P, mins, RN = T[k], P[k], mins[k], RN[k]
    fs = 1000.0 / np.median(np.diff(T))  # samples per second
    return T, P, mins, fs, RN


def to_ru_db(P, K):
    R = P.shape[1] // K  # number of RUs (floor)
    return 10 * np.log10(
        np.maximum(
            np.stack([P[:, r * K : (r + 1) * K].sum(1) for r in range(R)], 1), 1e-15
        )
    )  # sum RBs in each RU, then dB


def regret_vs_T(M, Ts, fs):
    best = M.max(1)
    out = []
    for T in Ts:
        step = max(1, int(round(T * fs)))
        pick = np.repeat(M[::step].argmax(1), step)[: len(M)]
        out.append(float((best - M[np.arange(len(M)), pick]).mean()))
    return np.array(out)


def lifetimes(M, eps, fs):
    best = M.max(1)
    out = []
    i = 0
    n = len(M)
    while i < n - 1:
        p = int(M[i].argmax())
        j = i + 1
        while j < n and M[j, p] >= best[j] - eps:
            j += 1
        out.append((j - i) / fs)
        i = j
    return np.array(out) if out else np.array([0.0])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cap")
    ap.add_argument("-o", "--out", default="metrics.png")
    ap.add_argument("--K", type=int, default=12)
    ap.add_argument("--trim-start", type=float, default=1.5)
    ap.add_argument("--trim-end", type=float, default=1.0)
    ap.add_argument(
        "--budget", type=float, default=0.5, help="regret budget for T* (dB)"
    )
    ap.add_argument(
        "--mobile",
        action="store_true",
        help="short averaging window for moving UE (default 60 ms)",
    )
    ap.add_argument(
        "--avg-win",
        type=float,
        default=None,
        help="averaging window in ms (overrides --mobile default)",
    )
    ap.add_argument(
        "--mobile-csv",
        default=None,
        help="append Tc/T*/regret row to this CSV (mobility sweep table)",
    )
    ap.add_argument(
        "--label",
        default=None,
        help="name to record in the mobility CSV file column (defaults to the capture path)",
    )
    ap.add_argument(
        "--reattach-guard",
        type=float,
        default=10.0,
        help="seconds of data to drop before AND after each re-attach (rnti change); 0 disables",
    )
    ap.add_argument(
        "--speed", type=float, default=None, help="UE speed m/s, for the Doppler line"
    )
    a = ap.parse_args()

    T, P, mins, fs, RN = load(a.cap, a.trim_start, a.trim_end)
    # drop a guard window around every re-attach (rnti change): power control resets there
    if a.reattach_guard > 0 and len(RN) > 1:
        g = int(round(a.reattach_guard * fs))
        keep = np.ones(len(RN), bool)
        cuts = [i for i in range(1, len(RN)) if RN[i] != RN[i - 1]]
        for c in cuts:
            keep[max(0, c - g) : min(len(RN), c + g)] = False
        dropped = (~keep).sum()
        if keep.sum() > 100:  # only apply if enough data survives
            P, mins, RN = P[keep], mins[keep], RN[keep]
            _ra_note = f"re-attach guard: {a.reattach_guard:.0f}s x2 around {len(cuts)} event(s), dropped {dropped} samples ({100*dropped/len(keep):.1f}%)"
        else:
            _ra_note = (
                f"re-attach guard SKIPPED: would drop too much ({len(cuts)} events)"
            )
    else:
        _ra_note = "re-attach guard: none applied"
    NRB = P.shape[1]
    # averaging window: 1 s static (noise suppression) vs short for mobility (preserve fast fading)
    if a.avg_win is not None:
        win_s = a.avg_win / 1000.0
    elif a.mobile:
        win_s = 0.060
    else:
        win_s = 1.0
    W = max(1, int(round(win_s * fs)))
    _winnote = f"avg window   : {win_s*1000:.0f} ms ({W} samples)" + (
        "  [MOBILE]" if (a.mobile or a.avg_win) else ""
    )
    M = to_ru_db(P, a.K)
    R = M.shape[1]
    L = []
    L.append(_winnote)
    L.append(_ra_note)
    L.append(f"file        : {a.cap}")
    L.append(
        f"samples     : {len(M)}   duration {mins[-1]-mins[0]:.1f} min   rate {fs:.1f}/s"
    )
    L.append(f"RBs sounded : {NRB}   RU size K={a.K} -> {R} RUs")
    # SRS SNR estimate from per-RB data: slow(signal) vs fast(noise) split over 1 s
    w = max(1, int(round(fs)))
    kern = np.ones(w) / w
    sig_pow, noi_pow = [], []
    for rb in range(NRB):
        col = P[:, rb]
        slow = np.convolve(col, kern, mode="same")
        fast = col - slow
        sig_pow.append(slow.mean() ** 2 if False else (slow**2).mean())
        noi_pow.append(fast.var())
    snr_lin = np.array(sig_pow) / np.maximum(np.array(noi_pow), 1e-20)
    snr_db = 10 * np.log10(np.maximum(snr_lin, 1e-12))
    L.append(
        f"SRS SNR est : median {np.median(snr_db):.1f} dB  p10 {np.percentile(snr_db,10):.1f} dB  "
        f"(per-RB, slow/fast split; lower bound if UE moving)"
    )

    fig, ax = plt.subplots(2, 3, figsize=(16.5, 9))

    # A regret vs T
    Ts = np.logspace(
        np.log10(2 / fs), np.log10(max(2.1 / fs, (mins[-1] - mins[0]) * 60 / 4)), 40
    )
    r = regret_vs_T(M, Ts, fs)
    ax[0, 0].semilogx(Ts, r, lw=2, color="#1f77b4")
    ax[0, 0].axhline(a.budget, ls="--", c="k", lw=1)
    ok = np.where(r <= a.budget)[0]
    Tstar = Ts[ok[-1]] if len(ok) else np.nan
    if len(ok):
        ax[0, 0].plot(Tstar, r[ok[-1]], "o", ms=10, mfc="w", mew=2, color="#1f77b4")
        ax[0, 0].text(Tstar, r[ok[-1]] + 0.05, f" T*={Tstar:.2f}s", fontsize=9)
    ax[0, 0].set_xlabel("re-decision period T(s)")
    ax[0, 0].set_ylabel("Mean Power loss (dB)")
    ax[0, 0].set_title(f"Mean Power Loss (dB) vs Update rate")
    ax[0, 0].grid(alpha=0.3, which="both")
    L.append(f"T*          : {Tstar:.3f} s at {a.budget} dB budget")
    L.append(f"regret(fixed choice) : {r[-1]:.2f} dB")

    # B lifetime CCDF
    for eps, c in [
        (0.5, "#4c72b0"),
        (1.0, "#55a868"),
        (2.0, "#c44e52"),
        (3.0, "#8172b2"),
    ]:
        li = np.sort(lifetimes(M, eps, fs))
        cc = 1 - np.arange(1, len(li) + 1) / len(li)
        ax[0, 1].loglog(
            np.maximum(li, 1 / fs),
            np.maximum(cc, 1 / len(li)),
            lw=1.8,
            color=c,
            label=f"ε={eps} dB (med {np.median(li):.2f}s)",
        )
        L.append(
            f"lifetime eps={eps:>3} dB : median {np.median(li):8.3f} s   p10 {np.percentile(li,10):8.3f} s"
        )
    ax[0, 1].set_xlabel("Decision Lifetime (s)")
    ax[0, 1].set_ylabel("P(lifetime > x)")
    ax[0, 1].set_title("Decision lifetime CCDF")
    ax[0, 1].grid(alpha=0.3, which="both")
    ax[0, 1].legend(fontsize=8)

    # C frequency correlation -> Bc  (time-average 1 s to suppress per-occasion noise floor)
    blk = W
    B = P[: len(P) // blk * blk].reshape(-1, blk, NRB).mean(1) if len(P) >= blk else P
    x = B / np.maximum(B.mean(0), 1e-15) - 1.0
    cf = np.array([(x[:, : NRB - d] * x[:, d:]).mean() for d in range(NRB)])
    cf /= cf[0]
    L.append(
        f"freq-corr lag1 (1s-avg): {cf[1]:.2f}  (low value => noise-dominated raw data)"
    )
    df = np.arange(NRB) * RBBW / 1e6
    ax[1, 1].plot(df, cf, lw=2, color="#1f77b4")
    ax[1, 1].axhline(0.5, ls="--", c="k", lw=1)
    Bc = df[np.argmax(cf < 0.5)] if (cf < 0.5).any() else np.nan
    if not np.isnan(Bc):
        ax[1, 1].axvline(Bc, ls=":", c="r", lw=2)
        ax[1, 1].text(Bc * 1.05, 0.8, f"$B_c$≈{Bc:.1f} MHz", color="r", fontsize=9)
    for K, st in [(4, ":"), (12, "-."), (24, "--")]:
        ax[1, 1].axvline(K * RBBW / 1e6, color="gray", ls=st, lw=1)
        ax[1, 1].text(
            K * RBBW / 1e6, 0.05, f"K={K}", rotation=90, fontsize=7, color="gray"
        )
    ax[1, 1].set_xlabel("frequency separation (MHz)")
    ax[1, 1].set_ylabel("correlation")
    ax[1, 1].set_title("C  frequency correlation -> $B_c$")
    ax[1, 1].grid(alpha=0.3)
    L.append(f"Bc(0.5)     : {Bc:.2f} MHz  (RU width K={a.K} = {a.K*RBBW/1e6:.2f} MHz)")

    # D granularity sweep
    Ks = [k for k in (4, 6, 8, 12, 16, 24) if NRB // k >= 2]
    G = []
    S = []
    for K in Ks:
        MK = to_ru_db(P, K)
        G.append(float((MK.max(1) - MK.mean(1)).mean()))
        S.append(float((MK.max(1) - MK.min(1)).mean() / MK.std(0).mean()))
        L.append(f"  K={K:>2} ({NRB//K} RUs): G={G[-1]:5.2f} dB  sep={S[-1]:5.2f}")
    b = ax[1, 0]
    b.plot(Ks, G, "o-", lw=2, color="#1f77b4")
    b.set_ylabel("opportunity G (dB)", color="#1f77b4")
    b2 = b.twinx()
    b2.plot(Ks, S, "s--", lw=2, color="#d62728")
    b2.set_ylabel("separation gap/σ", color="#d62728")
    b.set_xlabel("RU size K (RBs)")
    b.set_xticks(Ks)
    b.set_title("D  granularity sweep")
    b.grid(alpha=0.3)

    # E temporal autocorrelation -> Tc
    # smooth 1 s to remove the per-occasion noise floor before temporal correlation
    w = W
    ys = np.convolve(M[:, 0], np.ones(w) / w, mode="valid")
    y = ys - ys.mean()
    maxlag = min(len(y) - 2, int(60 * fs))
    ac = np.array([(y[: len(y) - d] * y[d:]).mean() for d in range(maxlag)])
    ac /= ac[0]
    lag = np.arange(maxlag) / fs
    ax[0, 2].plot(lag, ac, lw=1.8, color="#ff7f0e")
    ax[0, 2].axhline(0.5, ls="--", c="k", lw=1)
    k = np.where(ac < 0.5)[0]
    if len(k) and k[0] > 0:
        i = k[0]  # first lag below 0.5
        frac = (ac[i - 1] - 0.5) / (
            ac[i - 1] - ac[i]
        )  # linear interp between i-1 (>=0.5) and i (<0.5)
        Tc = lag[i - 1] + frac * (lag[i] - lag[i - 1])
    elif len(k):
        Tc = lag[k[0]]
    else:
        Tc = np.nan
    if not np.isnan(Tc):
        ax[0, 2].plot(Tc, 0.5, "o", ms=9, mfc="w", mew=2, color="#ff7f0e")
        ax[0, 2].text(Tc, 0.55, f" $T_c$={Tc:.2f}s", fontsize=9)
    if a.speed:
        Tth = 0.42 * LAMBDA / a.speed
    ax[0, 2].set_xlabel("lag (s)")
    ax[0, 2].set_ylabel("autocorrelation")
    ax[0, 2].set_title("E  temporal autocorrelation -> $T_c$")
    ax[0, 2].grid(alpha=0.3)
    L.append(f"Tc(0.5)     : {Tc:.3f} s")

    # F context: per-RU traces
    for rr in range(R):
        ax[1, 2].plot(mins, M[:, rr], lw=0.8, label=f"RU{rr}")
    cuts = [i for i in range(1, len(RN)) if RN[i] != RN[i - 1]]
    for c in cuts:
        tc = mins[c]
        ax[1, 2].axvline(tc, color="k", ls="--", lw=1, alpha=0.6)
        ax[1, 2].annotate(
            f"re-attach {tc:.1f}min\n{hex(int(RN[c]))}",
            xy=(tc, ax[1, 2].get_ylim()[1]),
            fontsize=6,
            ha="left",
            va="top",
            color="k",
        )
    L.append(
        f"re-attaches  : {len(cuts)}  at min "
        + ", ".join(f"{mins[c]:.1f}" for c in cuts)
    )
    ax[1, 2].set_xlabel("time (min)")
    ax[1, 2].set_ylabel("per-RU power (dB)")
    ax[1, 2].set_title("F  per-RU time series (context)")
    ax[1, 2].grid(alpha=0.3)
    ax[1, 2].legend(fontsize=7, ncol=2)

    # --- mobility sweep CSV row ---
    if a.mobile_csv:
        import csv, os

        Tc_theory = (0.42 * LAMBDA / a.speed) if a.speed else float("nan")
        row = {
            "file": a.label if a.label else a.cap.split("/")[-1],
            "speed_mps": a.speed if a.speed is not None else "",
            "Tc_meas_s": round(float(Tc), 4) if not np.isnan(Tc) else "",
            "Tc_theory_s": round(Tc_theory, 4) if a.speed else "",
            "Tc_ratio": (
                round(float(Tc) / Tc_theory, 3)
                if (a.speed and not np.isnan(Tc))
                else ""
            ),
            "Tstar_s": round(float(Tstar), 4) if not np.isnan(Tstar) else "",
            "regret_fixed_dB": round(float(r[-1]), 3),
            "Bc_MHz": round(float(Bc), 2) if not np.isnan(Bc) else "",
            "srs_snr_dB": round(float(np.median(snr_db)), 1),
            "avg_win_ms": round(win_s * 1000),
            "n_samples": len(M),
        }
        newf = not os.path.exists(a.mobile_csv)
        with open(a.mobile_csv, "a", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(row.keys()))
            if newf:
                w.writeheader()
            w.writerow(row)
        L.append(f"appended mobility row to {a.mobile_csv}")

    fig.suptitle(f"P1 metric suite — {a.cap.split('/')[-1]}", fontsize=12)
    fig.tight_layout(rect=[0, 0, 1, 0.965])
    fig.savefig(a.out, dpi=130)
    txt = a.out.rsplit(".", 1)[0] + "_metrics.txt"
    open(txt, "w").write("\n".join(L) + "\n")
    print("\n".join(L))
    print("\nwrote", a.out, "and", txt)


if __name__ == "__main__":
    main()
