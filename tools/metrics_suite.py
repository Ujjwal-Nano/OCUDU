#!/usr/bin/env python3
"""
metrics_suite.py — P1 metric set from a per-RB SRS capture.
Panels:
  A regret vs re-decision period T -> T*
  B decision-lifetime CCDF (eps = 0.5/1/2/3 dB)
  E temporal autocorrelation -> coherence time Tc(0.5)  [interpolated, multi-RBG]
  C frequency correlation -> coherence bandwidth Bc(0.5), K=4/12/24 markers
  G RBG x RBG frequency-correlation matrix (heatmap), modular in K
  H RBG-pair correlation vs RBG separation (from per-RB cf at RBG-spacing lags)
Usage:
  python3 metrics_suite.py CAP.rb.jsonl[.gz] [-o out.png] [--K 12]
    [--trim-start 1.5] [--trim-end 1.0] [--budget 0.5]
    [--avg-win MS | --mobile] [--speed V] [--mobile-csv PATH] [--label NAME]
"""
import argparse, gzip, json, sys
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

RBBW = 0.36e6
LAMBDA = 3e8 / 3.75e9

def load(path, trim_start, trim_end):
    op = gzip.open if path.endswith(".gz") else open
    T, P, RN = [], [], []
    with op(path, "rt") as f:
        for line in f:
            line = line.strip()
            if not line: continue
            try: d = json.loads(line)
            except json.JSONDecodeError: continue
            T.append(d["t"]); P.append(d["rb"]); RN.append(d.get("rnti", -1))
    if not P: sys.exit("no samples in " + path)
    T = np.array(T, float); P = np.array(P, float); RN = np.array(RN)
    P = P[:, 1:]
    mins = (T - T[0]) / 60000.0
    k = (mins >= trim_start) & (mins <= mins[-1] - trim_end)
    if k.sum() < 100: k = np.ones(len(mins), bool)
    T, P, mins, RN = T[k], P[k], mins[k], RN[k]
    fs = 1000.0 / np.median(np.diff(T))
    return T, P, mins, fs, RN

def to_ru_db(P, K):
    R = P.shape[1] // K
    return 10*np.log10(np.maximum(
        np.stack([P[:, r*K:(r+1)*K].sum(1) for r in range(R)], 1), 1e-15))

def regret_vs_T(M, Ts, fs):
    best = M.max(1); out = []
    for T in Ts:
        step = max(1, int(round(T*fs)))
        pick = np.repeat(M[::step].argmax(1), step)[:len(M)]
        out.append(float((best - M[np.arange(len(M)), pick]).mean()))
    return np.array(out)

def lifetimes(M, eps, fs):
    best = M.max(1); out = []; i = 0; n = len(M)
    while i < n-1:
        p = int(M[i].argmax()); j = i+1
        while j < n and M[j, p] >= best[j]-eps: j += 1
        out.append((j-i)/fs); i = j
    return np.array(out) if out else np.array([0.0])

def ru_pair_correlation(cf, K, NRB):
    """RBG-pair freq correlation from per-RB curve cf at RBG-separation lags. Modular in K."""
    R = NRB // K
    out = {}
    for s in range(1, R):
        lag = s * K
        out[s] = float(cf[lag]) if lag < len(cf) else float("nan")
    return out, R

def ru_corr_matrix(cf, K, NRB):
    """Full RxR RBG-pair correlation matrix from cf at |i-j|*K lags."""
    R = NRB // K
    M = np.eye(R)
    for i in range(R):
        for j in range(R):
            if i == j: continue
            lag = abs(i-j) * K
            M[i, j] = cf[lag] if lag < len(cf) else np.nan
    return M, R

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cap")
    ap.add_argument("-o", "--out", default="metrics.png")
    ap.add_argument("--K", type=int, default=12)
    ap.add_argument("--trim-start", type=float, default=1.5)
    ap.add_argument("--trim-end", type=float, default=1.0)
    ap.add_argument("--budget", type=float, default=0.5)
    ap.add_argument("--mobile", action="store_true")
    ap.add_argument("--avg-win", type=float, default=None)
    ap.add_argument("--mobile-csv", default=None)
    ap.add_argument("--label", default=None)
    ap.add_argument("--reattach-guard", type=float, default=10.0)
    ap.add_argument("--speed", type=float, default=None)
    a = ap.parse_args()

    T, P, mins, fs, RN = load(a.cap, a.trim_start, a.trim_end)
    if a.reattach_guard > 0 and len(RN) > 1:
        g = int(round(a.reattach_guard * fs))
        keep = np.ones(len(RN), bool)
        cuts = [i for i in range(1, len(RN)) if RN[i] != RN[i-1]]
        for c in cuts: keep[max(0, c-g):min(len(RN), c+g)] = False
        if keep.sum() > 100:
            P, mins, RN = P[keep], mins[keep], RN[keep]
            _ra = f"re-attach guard: {a.reattach_guard:.0f}s x2 around {len(cuts)} event(s)"
        else:
            _ra = f"re-attach guard SKIPPED ({len(cuts)} events)"
    else:
        _ra = "re-attach guard: none"
    NRB = P.shape[1]
    if a.avg_win is not None: win_s = a.avg_win/1000.0
    elif a.mobile: win_s = 0.060
    else: win_s = 1.0
    W = max(1, int(round(win_s*fs)))
    M = to_ru_db(P, a.K); R = M.shape[1]
    L = [f"avg window   : {win_s*1000:.0f} ms ({W} samples)", _ra,
         f"file        : {a.cap}",
         f"samples     : {len(M)}   duration {mins[-1]-mins[0]:.1f} min   rate {fs:.1f}/s",
         f"RBs sounded : {NRB}   RBG size K={a.K} -> {R} RBGs"]

    w = max(1, int(round(fs))); kern = np.ones(w)/w
    sig, noi = [], []
    for rb in range(NRB):
        col = P[:, rb]; slow = np.convolve(col, kern, mode="same"); fast = col - slow
        sig.append((slow**2).mean()); noi.append(fast.var())
    snr_db = 10*np.log10(np.maximum(np.array(sig)/np.maximum(np.array(noi),1e-20),1e-12))
    L.append(f"SRS SNR est : median {np.median(snr_db):.1f} dB  p10 {np.percentile(snr_db,10):.1f} dB")

    fig, ax = plt.subplots(2, 3, figsize=(18, 9.5))

    # A regret -> T* (interpolated first crossing)
    Ts = np.logspace(np.log10(2/fs), np.log10(max(2.1/fs, (mins[-1]-mins[0])*60/4)), 40)
    r = regret_vs_T(M, Ts, fs)
    ax[0,0].semilogx(Ts, r, lw=2, color="#1f77b4")
    ax[0,0].axhline(a.budget, ls="--", c="k", lw=1)
    kk = np.where(r > a.budget)[0]
    if len(kk) and kk[0] > 0:
        i = kk[0]; frac = (a.budget - r[i-1])/(r[i]-r[i-1]); Tstar = Ts[i-1]+frac*(Ts[i]-Ts[i-1])
    elif len(kk): Tstar = Ts[0]
    else: Tstar = Ts[-1]
    ax[0,0].plot(Tstar, a.budget, "o", ms=10, mfc="w", mew=2, color="#1f77b4")
    ax[0,0].text(Tstar, a.budget+0.05, f" T*={Tstar:.2f}s", fontsize=9)
    ax[0,0].set_xlabel("re-decision period T (s)"); ax[0,0].set_ylabel("mean power loss (dB)")
    ax[0,0].set_title("Mean power loss vs update rate"); ax[0,0].grid(alpha=0.3, which="both")
    L += [f"T*          : {Tstar:.3f} s at {a.budget} dB budget",
          f"regret(fixed choice) : {r[-1]:.2f} dB"]

    # B lifetime CCDF
    for eps, c in [(0.5,"#4c72b0"),(1.0,"#55a868"),(2.0,"#c44e52"),(3.0,"#8172b2")]:
        li = np.sort(lifetimes(M, eps, fs)); cc = 1-np.arange(1,len(li)+1)/len(li)
        ax[0,1].loglog(np.maximum(li,1/fs), np.maximum(cc,1/len(li)), lw=1.8, color=c,
                       label=f"ε={eps} dB (med {np.median(li):.2f}s)")
        L.append(f"lifetime eps={eps:>3} dB : median {np.median(li):8.3f} s")
    ax[0,1].set_xlabel("decision lifetime (s)"); ax[0,1].set_ylabel("P(lifetime > x)")
    ax[0,1].set_title("Decision lifetime CCDF"); ax[0,1].grid(alpha=0.3, which="both")
    ax[0,1].legend(fontsize=8)

    # E temporal autocorr -> Tc (interpolated, multi-RBG averaged)
    ac_all = []
    for rr in range(R):
        ys = np.convolve(M[:,rr], np.ones(W)/W, mode="valid"); y = ys - ys.mean()
        maxlag = min(len(y)-2, int(60*fs))
        ac_r = np.array([(y[:len(y)-d]*y[d:]).mean() for d in range(maxlag)]); ac_r /= ac_r[0]
        ac_all.append(ac_r)
    ac = np.mean(ac_all, axis=0); lag = np.arange(len(ac))/fs
    ax[0,2].plot(lag, ac, lw=1.8, color="#ff7f0e"); ax[0,2].axhline(0.5, ls="--", c="k", lw=1)
    kk = np.where(ac < 0.5)[0]
    if len(kk) and kk[0] > 0:
        i = kk[0]; frac = (ac[i-1]-0.5)/(ac[i-1]-ac[i]); Tc = lag[i-1]+frac*(lag[i]-lag[i-1])
    elif len(kk): Tc = lag[kk[0]]
    else: Tc = np.nan
    if not np.isnan(Tc):
        ax[0,2].plot(Tc, 0.5, "o", ms=9, mfc="w", mew=2, color="#ff7f0e")
        ax[0,2].text(Tc, 0.55, f" $T_c$={Tc:.2f}s", fontsize=9)
    # inset: zoom on the first crossing so Tc is clearly visible
    if not np.isnan(Tc):
        axins = ax[0,2].inset_axes([0.55, 0.55, 0.42, 0.4])
        zoom_max = max(5*Tc, 0.3)
        mzoom = lag <= zoom_max
        axins.plot(lag[mzoom], ac[mzoom], lw=1.8, color="#ff7f0e")
        axins.axhline(0.5, ls="--", c="k", lw=1)
        axins.plot(Tc, 0.5, "o", ms=7, mfc="w", mew=2, color="#ff7f0e")
        axins.set_xlim(0, zoom_max); axins.set_ylim(0.3, 1.02)
        axins.set_title(f"zoom: $T_c$={Tc:.3f}s", fontsize=8)
        axins.tick_params(labelsize=7); axins.grid(alpha=0.3)
    ax[0,2].set_xlabel("lag (s)"); ax[0,2].set_ylabel("autocorrelation")
    ax[0,2].set_title("Temporal autocorrelation -> $T_c$")
    ax[0,2].set_xlim(0, min(60, lag[-1])); ax[0,2].set_ylim(-0.05, 1.05); ax[0,2].grid(alpha=0.3)
    L.append(f"Tc(0.5)     : {Tc:.3f} s")

    # C frequency correlation -> Bc
    blk = W
    B = P[:len(P)//blk*blk].reshape(-1, blk, NRB).mean(1) if len(P) >= blk else P
    x = B/np.maximum(B.mean(0),1e-15) - 1.0
    cf = np.array([(x[:, :NRB-d]*x[:, d:]).mean() for d in range(NRB)]); cf /= cf[0]
    df = np.arange(NRB)*RBBW/1e6
    ax[1,0].plot(df, cf, lw=2, color="#1f77b4", zorder=3)
    ax[1,0].axhline(0.5, ls="--", c="k", lw=1, zorder=1)
    Bc = df[np.argmax(cf < 0.5)] if (cf < 0.5).any() else np.nan
    if not np.isnan(Bc):
        ax[1,0].axvline(Bc, ls=":", c="r", lw=2, zorder=2)
        ax[1,0].text(Bc+0.2, 0.44, f"$B_c$≈{Bc:.1f} MHz", color="r", fontsize=9, ha="left", va="top")
    xmax = max(24*RBBW/1e6, (Bc if not np.isnan(Bc) else 0)) + 1.5
    for K, st, col in [(4, ":", "#888"), (12, "-.", "#000"), (24, "--", "#888")]:
        fx = K*RBBW/1e6
        ax[1,0].axvline(fx, color=col, ls=st, lw=1.2, zorder=1)
        di = int(round(fx/(RBBW/1e6)))
        cval = cf[di] if di < len(cf) else float("nan")
        ax[1,0].annotate(f"K={K}\n{fx:.1f}MHz", xy=(fx,1.0), xytext=(fx,1.03),
                         textcoords=("data","axes fraction"), fontsize=7.5, ha="center",
                         va="bottom", color=col, annotation_clip=False)
        if not np.isnan(cval):
            ax[1,0].plot(fx, cval, "o", ms=6, color=col, zorder=4)
            ax[1,0].annotate(f"{cval:.2f}", xy=(fx,cval), xytext=(6,0),
                             textcoords="offset points", fontsize=8, va="center",
                             ha="left", color=col, fontweight="bold")
    ax[1,0].set_xlim(0, xmax); ax[1,0].set_ylim(0, 1.15)
    ax[1,0].set_xlabel("frequency separation (MHz)"); ax[1,0].set_ylabel("correlation")
    ax[1,0].set_title("Resource-block frequency correlation", pad=30); ax[1,0].grid(alpha=0.3, zorder=0)
    L.append(f"Bc(0.5)     : {Bc:.2f} MHz  (RBG width K={a.K} = {a.K*RBBW/1e6:.2f} MHz)")

    # G RBG×RBG correlation matrix (heatmap), modular in K
    corrM, R_ru = ru_corr_matrix(cf, a.K, NRB)
    im = ax[1,1].imshow(corrM, vmin=-1, vmax=1, cmap="coolwarm", origin="upper")
    ax[1,1].set_xticks(range(R_ru)); ax[1,1].set_yticks(range(R_ru))
    ax[1,1].set_xticklabels([f"RBG{i}" for i in range(R_ru)], fontsize=8)
    ax[1,1].set_yticklabels([f"RBG{i}" for i in range(R_ru)], fontsize=8)
    for i in range(R_ru):
        for j in range(R_ru):
            val = corrM[i,j]
            ax[1,1].text(j, i, f"{val:.2f}", ha="center", va="center",
                         color="white" if abs(val) > 0.5 else "black", fontsize=8)
    ax[1,1].set_title(f"RBG×RBG freq correlation (K={a.K}, {R_ru} RBGs)")
    fig.colorbar(im, ax=ax[1,1], fraction=0.046, pad=0.04)

    # H RBG-pair correlation vs RBG separation, modular in K
    pairs, _ = ru_pair_correlation(cf, a.K, NRB)
    seps = list(pairs.keys()); cvals = [pairs[s] for s in seps]
    sep_mhz = [s*a.K*RBBW/1e6 for s in seps]
    ax[1,2].plot(sep_mhz, cvals, "o-", lw=2, color="#d62728", ms=8)
    ax[1,2].axhline(0.5, ls="--", c="k", lw=1)
    for s, c, fm in zip(seps, cvals, sep_mhz):
        ax[1,2].annotate(f"RBG0-RBG{s}\n{c:.2f}", xy=(fm,c), xytext=(0,8),
                         textcoords="offset points", fontsize=7.5, ha="center")
    ax[1,2].set_xlabel("RBG separation (MHz)"); ax[1,2].set_ylabel("correlation")
    ax[1,2].set_title("RBG-pair correlation vs separation")
    ax[1,2].set_ylim(-1.05, 1.05); ax[1,2].grid(alpha=0.3)
    L.append(f"RBG-pair freq correlation (K={a.K}, {R_ru} RBGs):")
    for s, c in pairs.items():
        L.append(f"  RBG-sep {s} ({s*a.K*RBBW/1e6:.1f} MHz): corr={c:.2f}")

    cuts = [i for i in range(1, len(RN)) if RN[i] != RN[i-1]]
    L.append(f"re-attaches  : {len(cuts)}  at min " + ", ".join(f"{mins[c]:.1f}" for c in cuts))

    if a.mobile_csv:
        import csv, os
        Tc_theory = (0.42*LAMBDA/a.speed) if a.speed else float("nan")
        row = {"file": a.label if a.label else a.cap.split("/")[-1],
               "speed_mps": a.speed if a.speed is not None else "",
               "Tc_meas_s": round(float(Tc),4) if not np.isnan(Tc) else "",
               "Tc_theory_s": round(Tc_theory,4) if a.speed else "",
               "Tc_ratio": round(float(Tc)/Tc_theory,3) if (a.speed and not np.isnan(Tc)) else "",
               "Tstar_s": round(float(Tstar),4) if not np.isnan(Tstar) else "",
               "regret_fixed_dB": round(float(r[-1]),3),
               "Bc_MHz": round(float(Bc),2) if not np.isnan(Bc) else "",
               "srs_snr_dB": round(float(np.median(snr_db)),1),
               "avg_win_ms": round(win_s*1000), "n_samples": len(M)}
        newf = not os.path.exists(a.mobile_csv)
        with open(a.mobile_csv, "a", newline="") as f:
            wr = csv.DictWriter(f, fieldnames=list(row.keys()))
            if newf: wr.writeheader()
            wr.writerow(row)
        L.append(f"appended mobility row to {a.mobile_csv}")

    fig.suptitle(f"P1 metric suite — {a.cap.split('/')[-1]}", fontsize=12)
    fig.tight_layout(rect=[0,0,1,0.965]); fig.savefig(a.out, dpi=120)
    txt = a.out.rsplit(".",1)[0] + "_metrics.txt"
    open(txt, "w").write("\n".join(L) + "\n")
    print("\n".join(L)); print("\nwrote", a.out, "and", txt)

if __name__ == "__main__":
    main()
