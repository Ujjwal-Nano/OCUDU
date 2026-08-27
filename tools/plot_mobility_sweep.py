#!/usr/bin/env python3
"""
plot_mobility_sweep.py — Tc-vs-speed and T*-vs-speed, both SRS periods overlaid.
Linear axes, explicit speed ticks. Dedups repeated (speed,period) rows by averaging.

Usage:
  python3 plot_mobility_sweep.py <csv> -o out.png [--min-speed 0.05]
"""
import argparse, csv, re
import numpy as np, matplotlib
matplotlib.use("Agg"); import matplotlib.pyplot as plt

LAMBDA=0.08

def period_of(fname):
    ms=re.findall(r"E(\d+)", fname)
    if not ms: return "?"
    return "10ms" if len(ms[-1])>=4 else "20ms"

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("csv"); ap.add_argument("-o","--out",default="mobility_sweep.png")
    ap.add_argument("--min-speed", type=float, default=0.0, help="drop speeds below this")
    a=ap.parse_args()

    rows=list(csv.DictReader(open(a.csv)))
    def getf(r,*keys):
        for k in keys:
            if k in r and r[k] not in ("","None"):
                try: return float(r[k])
                except: pass
        return np.nan

    # collect, then average duplicates per (period, speed)
    from collections import defaultdict
    acc=defaultdict(lambda: {"tc":[], "tstar":[]})
    for r in rows:
        per=period_of(r.get("file",""))
        if per not in ("10ms","20ms"): continue
        v=getf(r,"speed_mps")
        if np.isnan(v) or v < a.min_speed: continue
        acc[(per,round(v,3))]["tc"].append(getf(r,"Tc_plateau_s","Tc_meas_s"))
        acc[(per,round(v,3))]["tstar"].append(getf(r,"Tstar_s"))

    data={"10ms":{"v":[],"tc":[],"tstar":[]}, "20ms":{"v":[],"tc":[],"tstar":[]}}
    for (per,v),d in acc.items():
        data[per]["v"].append(v)
        data[per]["tc"].append(np.nanmean(d["tc"]) if d["tc"] else np.nan)
        data[per]["tstar"].append(np.nanmean(d["tstar"]) if d["tstar"] else np.nan)

    speeds=sorted(set(data["10ms"]["v"])|set(data["20ms"]["v"]))

    fig,ax=plt.subplots(1,2,figsize=(13,5.2))
    colors={"20ms":"#1f77b4","10ms":"#d62728"}
    markers={"20ms":"o","10ms":"s"}

    # ---- Tc vs speed (linear) ----
    for per in ("20ms","10ms"):
        if not data[per]["v"]: continue
        v=np.array(data[per]["v"]); tc=np.array(data[per]["tc"])
        o=np.argsort(v); v,tc=v[o],tc[o]; m=~np.isnan(tc)
        ax[0].plot(v[m],tc[m],markers[per]+"-",color=colors[per],label=f"measured ({per})",ms=7,lw=1.6)
    vv=np.linspace(max(0.03,a.min_speed),0.52,200)
    ax[0].plot(vv,0.42*LAMBDA/vv,"k--",lw=1.3,label="theory 0.42·λ/v")
    ax[0].plot(vv,0.30*LAMBDA/vv,"k:",lw=1.1,label="0.30·λ/v")
    ax[0].set_xlabel("UE speed (m/s)"); ax[0].set_ylabel("coherence time  Tc (s)")
    ax[0].set_title("Coherence time vs speed")
    ax[0].set_xticks(speeds); ax[0].set_xticklabels([f"{s:g}" for s in speeds])
    ax[0].grid(alpha=.3); ax[0].legend(fontsize=8)
    ax[0].set_ylim(bottom=0)

    # ---- T* vs speed (linear) ----
    for per in ("20ms","10ms"):
        if not data[per]["v"]: continue
        v=np.array(data[per]["v"]); ts=np.array(data[per]["tstar"])
        o=np.argsort(v); v,ts=v[o],ts[o]; m=~np.isnan(ts)
        ax[1].plot(v[m],ts[m],markers[per]+"-",color=colors[per],label=f"T* ({per})",ms=7,lw=1.6)
    ax[1].set_xlabel("UE speed (m/s)"); ax[1].set_ylabel("required re-decision period  T* (s)")
    ax[1].set_title("Required update rate vs speed")
    ax[1].set_xticks(speeds); ax[1].set_xticklabels([f"{s:g}" for s in speeds])
    ax[1].grid(alpha=.3); ax[1].legend(fontsize=8)
    ax[1].set_ylim(bottom=0)

    fig.suptitle("Mobility sweep — SRS 10 ms vs 20 ms period",fontsize=12)
    fig.tight_layout(rect=[0,0,1,0.95]); fig.savefig(a.out,dpi=140)
    print("wrote",a.out)
    print("\nspeed  Tc(20ms) Tc(10ms)  T*(20ms) T*(10ms)")
    for vq in speeds:
        def pick(per,f):
            for i,vv2 in enumerate(data[per]["v"]):
                if abs(vv2-vq)<1e-6: return data[per][f][i]
            return np.nan
        print(f"{vq:5.2f}  {pick('20ms','tc'):8.3f} {pick('10ms','tc'):8.3f}  {pick('20ms','tstar'):8.3f} {pick('10ms','tstar'):8.3f}")

if __name__=="__main__":
    main()
