#!/usr/bin/env python3
"""
swap_value.py — decision-oriented metrics for sub-band (RU) scheduling.

Answers four questions from measured per-RB SRS captures:
  1) Rate gain vs RU size K, honestly (select on a noisy sample, get paid on the next one)
  2) Hysteresis threshold: swaps/min and regret vs delta -> the design knee
  3) Opportunity duration: how long is another RU better by >= delta -> is a swap worth it
  4) Two-user trace-driven: blind split vs best-RU assignment vs oracle (weakest-user rate)

Usage:
  python3 swap_value.py A.rb.jsonl.gz [-o out.png] [--second B.rb.jsonl.gz]
                        [--trim-start 1.5] [--trim-end 1.0] [--mobile]
"""
import argparse, gzip, json, sys
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

RBBW = 0.36e6

def load(path, ts, te):
    op = gzip.open if path.endswith(".gz") else open
    T, P = [], []
    with op(path, "rt") as f:
        for line in f:
            line = line.strip()
            if not line: continue
            try: d = json.loads(line)
            except json.JSONDecodeError: continue
            T.append(d["t"]); P.append(d["rb"])
    T = np.array(T, float); P = np.array(P, float)[:, 1:]
    mins = (T - T[0]) / 60000.0
    k = (mins >= ts) & (mins <= mins[-1] - te)
    if k.sum() < 100: k = np.ones(len(mins), bool)
    T, P, mins = T[k], P[k], mins[k]
    fs = 1000.0 / np.median(np.diff(T))
    return P, mins, fs

def to_ru_lin(P, K):
    """per-RU linear power, normalised so mean over band = 1 (relative SNR)."""
    R = P.shape[1] // K
    X = np.stack([P[:, r*K:(r+1)*K].mean(1) for r in range(R)], 1)
    return X / X.mean()

def rate(snr_lin, snr0_db=10.0, cap=7.4):
    """bits/s/Hz from relative power. snr0_db = mean-band operating SNR."""
    s = snr_lin * 10**(snr0_db/10.0)
    return np.minimum(np.log2(1.0 + s), cap)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cap"); ap.add_argument("-o","--out",default="swap_value.png")
    ap.add_argument("--second", default=None, help="second capture = virtual user 1")
    ap.add_argument("--trim-start", type=float, default=1.5)
    ap.add_argument("--trim-end", type=float, default=1.0)
    ap.add_argument("--snr0", type=float, default=10.0, help="mean operating SNR (dB)")
    ap.add_argument("--pairs", nargs="*", default=None,
                    help="extra captures: sweep ALL pairs -> gain vs profile complementarity")
    a = ap.parse_args()

    P, mins, fs = load(a.cap, a.trim_start, a.trim_end)
    NRB = P.shape[1]
    L = [f"file : {a.cap}", f"samples {len(P)}  {mins[-1]-mins[0]:.1f} min  rate {fs:.0f}/s  RBs {NRB}",
         f"rate model: log2(1+SNR), mean-band SNR {a.snr0} dB, cap 7.4 b/s/Hz"]

    fig, ax = plt.subplots(2, 2, figsize=(13.5, 9))

    # ---- 1) rate gain vs K, in-sample (optimistic) vs out-of-sample (honest)
    Ks = [k for k in (2,3,4,6,8,12,16,24) if NRB % k == 0 and NRB//k >= 2]
    gin, gout, nrus = [], [], []
    for K in Ks:
        X = to_ru_lin(P, K)                      # [t x R]
        r = rate(X, a.snr0)
        blind = rate(X.mean(1), a.snr0)          # whole-band average = stock scheduler view
        pick_now = X.argmax(1)                   # select using this sample
        insample  = r[np.arange(len(r)), pick_now]
        # honest: select on sample t, get paid at t+1 (noise in the estimate no longer helps)
        outs = r[np.arange(len(r)-1)+1, pick_now[:-1]]
        gin.append(float((insample - blind).mean()))
        gout.append(float((outs - blind[1:]).mean()))
        nrus.append(NRB//K)
        L.append(f"  K={K:>2} ({NRB//K:>2} RUs): rate gain in-sample {gin[-1]:+.3f}  honest {gout[-1]:+.3f} b/s/Hz")
    b = ax[0,0]
    b.plot(Ks, gin, "o--", color="#aaaaaa", label="in-sample (optimistic)")
    b.plot(Ks, gout, "o-", lw=2, color="#1f77b4", label="out-of-sample (honest)")
    kbest = Ks[int(np.argmax(gout))]
    b.axvline(kbest, ls=":", c="r"); b.text(kbest, min(gout), f" best K={kbest}", color="r", fontsize=9)
    b.set_xlabel("RU size K (RBs)"); b.set_ylabel("rate gain over blind (b/s/Hz)")
    b.set_title("1  gain vs RU size — winner's curse visible"); b.grid(alpha=.3); b.legend(fontsize=8)
    L.append(f"OPTIMAL K (honest): {kbest} RBs = {NRB//kbest} RUs, gain {max(gout):+.3f} b/s/Hz")

    # ---- 2) hysteresis sweep: swaps/min and regret vs delta
    K = 12 if NRB % 12 == 0 else Ks[len(Ks)//2]
    X = to_ru_lin(P, K); Xdb = 10*np.log10(X); R = X.shape[1]
    best_db = Xdb.max(1)
    deltas = np.arange(0, 3.01, 0.25)
    sw_per_min, reg = [], []
    for d in deltas:
        cur = int(Xdb[0].argmax()); nsw = 0; loss = 0.0
        for t in range(len(Xdb)):
            cand = int(Xdb[t].argmax())
            if cand != cur and Xdb[t, cand] > Xdb[t, cur] + d:
                cur = cand; nsw += 1
            loss += best_db[t] - Xdb[t, cur]
        sw_per_min.append(nsw / (mins[-1]-mins[0]))
        reg.append(loss/len(Xdb))
    c = ax[0,1]
    c.plot(deltas, reg, "o-", lw=2, color="#d62728"); c.set_ylabel("mean regret (dB)", color="#d62728")
    c2 = c.twinx(); c2.semilogy(deltas, np.maximum(sw_per_min,1e-3), "s--", lw=2, color="#2ca02c")
    c2.set_ylabel("swaps per minute", color="#2ca02c")
    sig = Xdb.std(0).mean(); dmin = 2*sig*np.sqrt(2)
    c.axvline(dmin, ls=":", c="k"); c.text(dmin, max(reg)*0.9, f" noise floor\n Δ>{dmin:.1f} dB", fontsize=8)
    c.set_xlabel("hysteresis threshold Δ (dB)"); c.set_title(f"2  when is a swap worth it (K={K})")
    c.grid(alpha=.3)
    L.append(f"per-RU sigma {sig:.2f} dB -> noise-limited minimum useful Δ ≈ {dmin:.1f} dB")
    for i,d in enumerate(deltas):
        if abs(d-1.0)<1e-6 or abs(d-2.0)<1e-6:
            L.append(f"  Δ={d:.1f} dB: {sw_per_min[i]:.1f} swaps/min, regret {reg[i]:.2f} dB")

    # ---- 3) opportunity duration: how long is another RU better by >= delta
    d3 = ax[1,0]
    for dlt, col in [(0.5,"#4c72b0"),(1.0,"#55a868"),(2.0,"#c44e52")]:
        cur = int(Xdb[:min(len(Xdb),int(fs))].mean(0).argmax())    # warm-up pick, held
        adv = Xdb.max(1) - Xdb[:, cur]
        on = adv >= dlt
        # run lengths of True
        runs=[]; i=0
        while i < len(on):
            if on[i]:
                j=i
                while j<len(on) and on[j]: j+=1
                runs.append((j-i)/fs); i=j
            else: i+=1
        if not runs: runs=[0.0]
        runs=np.sort(np.array(runs)); cc=1-np.arange(1,len(runs)+1)/len(runs)
        d3.loglog(np.maximum(runs,1/fs), np.maximum(cc,1/len(runs)), lw=1.8, color=col,
                  label=f"Δ≥{dlt} dB (med {np.median(runs)*1000:.0f} ms, {100*on.mean():.1f}% of time)")
        L.append(f"opportunity Δ≥{dlt} dB: present {100*on.mean():5.1f}% of time, median duration {np.median(runs)*1000:7.0f} ms, "
                 f"value {dlt*np.median(runs):.2f} dB·s")
    d3.set_xlabel("duration of the opportunity (s)"); d3.set_ylabel("P(duration > x)")
    d3.set_title("3  how long is a swap worth taking"); d3.grid(alpha=.3, which="both"); d3.legend(fontsize=7)

    # ---- 4) two-user trace-driven
    e = ax[1,1]
    if a.second:
        P2, m2, fs2 = load(a.second, a.trim_start, a.trim_end)
        n = min(len(P), len(P2))
        X0 = to_ru_lin(P[:n], K); X1 = to_ru_lin(P2[:n], K)
        r0, r1 = rate(X0, a.snr0), rate(X1, a.snr0)
        Rn = X0.shape[1]; half = Rn//2
        # blind fixed split: user0 gets RUs 0..half-1, user1 the rest
        b0 = r0[:, :half].mean(1); b1 = r1[:, half:].mean(1)
        blind_w = np.minimum(b0, b1)
        # best-RU assignment (each user takes its own best half, ties broken to help the weaker)
        o0, o1 = [], []
        idx0 = np.argsort(-X0, axis=1)[:, :half]
        idx1 = np.argsort(-X1, axis=1)[:, :half]
        s0 = np.take_along_axis(r0, idx0, 1).mean(1)
        s1 = np.take_along_axis(r1, idx1, 1).mean(1)
        greedy_w = np.minimum(s0, s1)      # may collide; upper-ish reference
        for nm, v, col in [("blind split", blind_w, "#888888"),
                           ("best-RU (per user)", greedy_w, "#1f77b4")]:
            vs=np.sort(v); cc=np.arange(1,len(vs)+1)/len(vs)
            e.plot(vs, cc, lw=2, color=col, label=f"{nm} (mean {v.mean():.2f})")
            L.append(f"two-user weakest rate — {nm}: mean {v.mean():.3f}, p10 {np.percentile(v,10):.3f} b/s/Hz")
        e.set_xlabel("weakest-user rate (b/s/Hz)"); e.set_ylabel("CDF")
        e.set_title("4  two-user (trace-driven): weakest-user rate")
        e.grid(alpha=.3); e.legend(fontsize=8)
        L.append("NOTE: users may contend for the same RU; 'best-RU' here is an upper reference, not a feasible allocation.")
    elif a.pairs:
        files = [a.cap] + list(a.pairs)
        prof, RUs, names = {}, {}, {}
        for f in files:
            Pf, mf, ff = load(f, a.trim_start, a.trim_end)
            Xf = to_ru_lin(Pf, K)
            names[f] = f.split("/")[-1].split(".")[0][-12:]
            prof[f] = 10*np.log10(Xf.mean(0))          # per-RU dB profile
            RUs[f] = Xf
        pts = []
        for i in range(len(files)):
            for j in range(i+1, len(files)):
                fi, fj = files[i], files[j]
                pi, pj = prof[fi]-prof[fi].mean(), prof[fj]-prof[fj].mean()
                denom = (np.linalg.norm(pi)*np.linalg.norm(pj))
                corr = float((pi*pj).sum()/denom) if denom > 0 else 0.0
                n = min(len(RUs[fi]), len(RUs[fj]))
                r0, r1 = rate(RUs[fi][:n], a.snr0), rate(RUs[fj][:n], a.snr0)
                Rn = r0.shape[1]; half = max(1, Rn//2)
                bw = np.minimum(r0[:, :half].mean(1), r1[:, half:].mean(1))
                i0 = np.argsort(-RUs[fi][:n], 1)[:, :half]; i1 = np.argsort(-RUs[fj][:n], 1)[:, :half]
                sw = np.minimum(np.take_along_axis(r0, i0, 1).mean(1),
                                np.take_along_axis(r1, i1, 1).mean(1))
                g = 100.0*(sw.mean()-bw.mean())/bw.mean()
                pts.append((corr, g, names[fi]+"/"+names[fj]))
                L.append(f"pair {names[fi]:>12} + {names[fj]:>12}: profile corr {corr:+.2f}  weakest-user gain {g:+.2f}%")
        cs = np.array([p[0] for p in pts]); gs = np.array([p[1] for p in pts])
        e.scatter(cs, gs, s=60, c="#1f77b4", zorder=3)
        for cc, gg, nm in pts:
            e.annotate(nm, (cc, gg), fontsize=5, xytext=(3,3), textcoords="offset points")
        if len(cs) > 2:
            k = np.polyfit(cs, gs, 1); xx = np.linspace(cs.min(), cs.max(), 20)
            e.plot(xx, np.polyval(k, xx), "--", color="#d62728", lw=1.5,
                   label=f"fit: slope {k[0]:+.1f} %/corr")
            rho = float(np.corrcoef(cs, gs)[0,1])
            e.legend(fontsize=8); L.append(f"gain-vs-complementarity: slope {k[0]:+.2f} %/unit, r={rho:+.2f}")
        e.axhline(0, color="k", lw=.8)
        e.set_xlabel("profile complementarity  (corr of per-RU dB profiles; -1 = opposite)")
        e.set_ylabel("weakest-user rate gain (%)")
        e.set_title("4  gain vs how different the two users are"); e.grid(alpha=.3)
    else:
        e.text(.5,.5,"pass --second <capture>  or  --pairs <c1> <c2> ...",
               ha="center", va="center", fontsize=11); e.axis("off")

    fig.suptitle(f"swap value — {a.cap.split('/')[-1]}", fontsize=12)
    fig.tight_layout(rect=[0,0,1,.96]); fig.savefig(a.out, dpi=130)
    txt=a.out.rsplit(".",1)[0]+"_swapvalue.txt"
    open(txt,"w").write("\n".join(L)+"\n")
    print("\n".join(L)); print("\nwrote", a.out, "and", txt)

if __name__ == "__main__":
    main()
