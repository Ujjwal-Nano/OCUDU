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

RBBW = 0.36e6          # 12 subcarriers x 30 kHz
LAMBDA = 3e8 / 3.75e9  # ~0.08 m

def load(path, trim_start, trim_end):
    op = gzip.open if path.endswith(".gz") else open
    T, P, RN = [], [], []
    with op(path, "rt") as f:
        for line in f:
            line = line.strip()
            if not line: continue
            try: d = json.loads(line)
            except json.JSONDecodeError: continue
            T.append(d["t"]); P.append(d["rb"]); RN.append(d.get("rnti",-1))
    if not P: sys.exit("no samples in " + path)
    T = np.array(T, float); P = np.array(P, float); RN = np.array(RN)
    P = P[:, 1:]                                   # drop CRB0 (never sounded)
    mins = (T - T[0]) / 60000.0
    k = (mins >= trim_start) & (mins <= mins[-1] - trim_end)
    if k.sum() < 100: k = np.ones(len(mins), bool)  # short capture: keep all
    T, P, mins, RN = T[k], P[k], mins[k], RN[k]
    fs = 1000.0 / np.median(np.diff(T))            # samples per second
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

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cap"); ap.add_argument("-o", "--out", default="metrics.png")
    ap.add_argument("--K", type=int, default=12)
    ap.add_argument("--trim-start", type=float, default=1.5)
    ap.add_argument("--trim-end", type=float, default=1.0)
    ap.add_argument("--budget", type=float, default=0.5, help="regret budget for T* (dB)")
    ap.add_argument("--mobile", action="store_true", help="short averaging window for moving UE (default 60 ms)")
    ap.add_argument("--avg-win", type=float, default=None, help="averaging window in ms (overrides --mobile default)")
    ap.add_argument("--speed", type=float, default=None, help="UE speed m/s, for the Doppler line")
    a = ap.parse_args()

    T, P, mins, fs, RN = load(a.cap, a.trim_start, a.trim_end)
    NRB = P.shape[1]
    # averaging window: 1 s static (noise suppression) vs short for mobility (preserve fast fading)
    if a.avg_win is not None:   win_s = a.avg_win/1000.0
    elif a.mobile:              win_s = 0.060
    else:                       win_s = 1.0
    W = max(1, int(round(win_s*fs)))
    _winnote = f"avg window   : {win_s*1000:.0f} ms ({W} samples)" + ("  [MOBILE]" if (a.mobile or a.avg_win) else "")
    M = to_ru_db(P, a.K); R = M.shape[1]
    L = []
    L.append(_winnote)
    L.append(f"file        : {a.cap}")
    L.append(f"samples     : {len(M)}   duration {mins[-1]-mins[0]:.1f} min   rate {fs:.1f}/s")
    L.append(f"RBs sounded : {NRB}   RU size K={a.K} -> {R} RUs")
    # SRS SNR estimate from per-RB data: slow(signal) vs fast(noise) split over 1 s
    w=max(1,int(round(fs)))
    kern=np.ones(w)/w
    sig_pow, noi_pow = [], []
    for rb in range(NRB):
        col=P[:,rb]
        slow=np.convolve(col, kern, mode="same")
        fast=col-slow
        sig_pow.append(slow.mean()**2 if False else (slow**2).mean())
        noi_pow.append(fast.var())
    snr_lin=np.array(sig_pow)/np.maximum(np.array(noi_pow),1e-20)
    snr_db=10*np.log10(np.maximum(snr_lin,1e-12))
    L.append(f"SRS SNR est : median {np.median(snr_db):.1f} dB  p10 {np.percentile(snr_db,10):.1f} dB  "
             f"(per-RB, slow/fast split; lower bound if UE moving)")

    fig, ax = plt.subplots(2, 3, figsize=(16.5, 9))

    # A regret vs T
    Ts = np.logspace(np.log10(2/fs), np.log10(max(2.1/fs, (mins[-1]-mins[0])*60/4)), 40)
    r = regret_vs_T(M, Ts, fs)
    ax[0,0].semilogx(Ts, r, lw=2, color="#1f77b4")
    ax[0,0].axhline(a.budget, ls="--", c="k", lw=1)
    ok = np.where(r <= a.budget)[0]
    Tstar = Ts[ok[-1]] if len(ok) else np.nan
    if len(ok):
        ax[0,0].plot(Tstar, r[ok[-1]], "o", ms=10, mfc="w", mew=2, color="#1f77b4")
        ax[0,0].text(Tstar, r[ok[-1]]+0.05, f" T*={Tstar:.2f}s", fontsize=9)
    ax[0,0].set_xlabel("re-decision period T (s)"); ax[0,0].set_ylabel("mean regret (dB)")
    ax[0,0].set_title(f"A  regret vs update rate (budget {a.budget} dB)")
    ax[0,0].grid(alpha=.3, which="both")
    L.append(f"T*          : {Tstar:.3f} s at {a.budget} dB budget")
    L.append(f"regret(fixed choice) : {r[-1]:.2f} dB")

    # B lifetime CCDF
    for eps, c in [(0.5,"#4c72b0"),(1.0,"#55a868"),(2.0,"#c44e52"),(3.0,"#8172b2")]:
        li = np.sort(lifetimes(M, eps, fs)); cc = 1-np.arange(1,len(li)+1)/len(li)
        ax[0,1].loglog(np.maximum(li,1/fs), np.maximum(cc,1/len(li)), lw=1.8, color=c,
                       label=f"ε={eps} dB (med {np.median(li):.2f}s)")
        L.append(f"lifetime eps={eps:>3} dB : median {np.median(li):8.3f} s   p10 {np.percentile(li,10):8.3f} s")
    ax[0,1].set_xlabel("decision lifetime (s)"); ax[0,1].set_ylabel("P(lifetime > x)")
    ax[0,1].set_title("B  decision lifetime CCDF"); ax[0,1].grid(alpha=.3, which="both"); ax[0,1].legend(fontsize=8)

    # C frequency correlation -> Bc  (time-average 1 s to suppress per-occasion noise floor)
    blk = W
    B = P[:len(P)//blk*blk].reshape(-1, blk, NRB).mean(1) if len(P) >= blk else P
    x = B/np.maximum(B.mean(0),1e-15) - 1.0
    cf = np.array([(x[:, :NRB-d]*x[:, d:]).mean() for d in range(NRB)]); cf /= cf[0]
    L.append(f"freq-corr lag1 (1s-avg): {cf[1]:.2f}  (low value => noise-dominated raw data)")
    df = np.arange(NRB)*RBBW/1e6
    ax[0,2].plot(df, cf, lw=2, color="#1f77b4"); ax[0,2].axhline(.5, ls="--", c="k", lw=1)
    Bc = df[np.argmax(cf<0.5)] if (cf<0.5).any() else np.nan
    if not np.isnan(Bc):
        ax[0,2].axvline(Bc, ls=":", c="r", lw=2); ax[0,2].text(Bc*1.05,.8,f"$B_c$≈{Bc:.1f} MHz",color="r",fontsize=9)
    for K,st in [(4,":"),(12,"-."),(24,"--")]:
        ax[0,2].axvline(K*RBBW/1e6, color="gray", ls=st, lw=1)
        ax[0,2].text(K*RBBW/1e6,.05,f"K={K}",rotation=90,fontsize=7,color="gray")
    ax[0,2].set_xlabel("frequency separation (MHz)"); ax[0,2].set_ylabel("correlation")
    ax[0,2].set_title("C  frequency correlation -> $B_c$"); ax[0,2].grid(alpha=.3)
    L.append(f"Bc(0.5)     : {Bc:.2f} MHz  (RU width K={a.K} = {a.K*RBBW/1e6:.2f} MHz)")

    # D granularity sweep
    Ks=[k for k in (4,6,8,12,16,24) if NRB//k >= 2]; G=[];S=[]
    for K in Ks:
        MK = to_ru_db(P,K)
        G.append(float((MK.max(1)-MK.mean(1)).mean()))
        S.append(float((MK.max(1)-MK.min(1)).mean()/MK.std(0).mean()))
        L.append(f"  K={K:>2} ({NRB//K} RUs): G={G[-1]:5.2f} dB  sep={S[-1]:5.2f}")
    b=ax[1,0]; b.plot(Ks,G,"o-",lw=2,color="#1f77b4"); b.set_ylabel("opportunity G (dB)",color="#1f77b4")
    b2=b.twinx(); b2.plot(Ks,S,"s--",lw=2,color="#d62728"); b2.set_ylabel("separation gap/σ",color="#d62728")
    b.set_xlabel("RU size K (RBs)"); b.set_xticks(Ks); b.set_title("D  granularity sweep"); b.grid(alpha=.3)

    # E temporal autocorrelation -> Tc
    # smooth 1 s to remove the per-occasion noise floor before temporal correlation
    w=W
    ys=np.convolve(M[:,0], np.ones(w)/w, mode="valid")
    y=ys-ys.mean()
    maxlag=min(len(y)-2, int(60*fs))
    ac=np.array([(y[:len(y)-d]*y[d:]).mean() for d in range(maxlag)]); ac/=ac[0]
    lag=np.arange(maxlag)/fs
    ax[1,1].plot(lag,ac,lw=1.8,color="#ff7f0e"); ax[1,1].axhline(.5,ls="--",c="k",lw=1)
    k=np.where(ac<0.5)[0]; Tc=lag[k[0]] if len(k) else np.nan
    if not np.isnan(Tc):
        ax[1,1].plot(Tc,.5,"o",ms=9,mfc="w",mew=2,color="#ff7f0e"); ax[1,1].text(Tc,.55,f" $T_c$={Tc:.2f}s",fontsize=9)
    if a.speed:
        Tth=0.42*LAMBDA/a.speed
        ax[1,1].axvline(Tth,ls=":",c="g",lw=2); ax[1,1].text(Tth,.85,f" theory {Tth*1000:.0f} ms",color="g",fontsize=8)
        L.append(f"Tc theory   : {Tth*1000:.1f} ms at v={a.speed} m/s (0.42*lambda/v)")
    ax[1,1].set_xlabel("lag (s)"); ax[1,1].set_ylabel("autocorrelation")
    ax[1,1].set_title("E  temporal autocorrelation -> $T_c$"); ax[1,1].grid(alpha=.3)
    L.append(f"Tc(0.5)     : {Tc:.3f} s")

    # F context: per-RU traces
    for rr in range(R):
        ax[1,2].plot(mins, M[:,rr], lw=.8, label=f"RU{rr}")
    cuts = [i for i in range(1,len(RN)) if RN[i]!=RN[i-1]]
    for c in cuts:
        tc = mins[c]
        ax[1,2].axvline(tc, color="k", ls="--", lw=1, alpha=0.6)
        ax[1,2].annotate(f"re-attach {tc:.1f}min\n{hex(int(RN[c]))}", xy=(tc, ax[1,2].get_ylim()[1]),
                         fontsize=6, ha="left", va="top", color="k")
    L.append(f"re-attaches  : {len(cuts)}  at min " + ", ".join(f"{mins[c]:.1f}" for c in cuts))
    ax[1,2].set_xlabel("time (min)"); ax[1,2].set_ylabel("per-RU power (dB)")
    ax[1,2].set_title("F  per-RU time series (context)"); ax[1,2].grid(alpha=.3); ax[1,2].legend(fontsize=7, ncol=2)

    fig.suptitle(f"P1 metric suite — {a.cap.split('/')[-1]}", fontsize=12)
    fig.tight_layout(rect=[0,0,1,0.965]); fig.savefig(a.out, dpi=130)
    txt = a.out.rsplit(".",1)[0]+"_metrics.txt"
    open(txt,"w").write("\n".join(L)+"\n")
    print("\n".join(L)); print("\nwrote", a.out, "and", txt)

if __name__ == "__main__":
    main()
