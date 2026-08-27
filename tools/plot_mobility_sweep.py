#!/usr/bin/env python3
"""
plot_mobility_sweep.py — Tc-vs-speed and T*-vs-speed, both SRS periods overlaid.

Reads a CSV with columns including speed_mps, Tc (or Tc_plateau_s), Tstar_s, and
a way to tell 10ms vs 20ms captures apart (filename: E00xx=10ms, E0xx=20ms).

Usage:
  python3 plot_mobility_sweep.py <csv> -o out.png
"""
import argparse, csv, sys
import numpy as np, matplotlib
matplotlib.use("Agg"); import matplotlib.pyplot as plt

LAMBDA=0.08

def period_of(fname):
    # E00xx (4-digit-ish after E) = 10ms ; E0xx = 20ms. Heuristic on the label.
    import re
    ms=re.findall(r"E(\d+)", fname)
    if not ms: return "?"
    digits=ms[-1]
    return "10ms" if len(digits)>=4 else "20ms"

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("csv"); ap.add_argument("-o","--out",default="mobility_sweep.png")
    a=ap.parse_args()

    rows=list(csv.DictReader(open(a.csv)))
    def getf(r,*keys):
        for k in keys:
            if k in r and r[k] not in ("","None"):
                try: return float(r[k])
                except: pass
        return np.nan

    data={"10ms":{"v":[],"tc":[],"tstar":[]}, "20ms":{"v":[],"tc":[],"tstar":[]}}
    for r in rows:
        fname=r.get("file","")
        per=period_of(fname)
        if per not in data: continue
        v=getf(r,"speed_mps")
        tc=getf(r,"Tc_plateau_s","Tc_meas_s","Tc_meas")
        ts=getf(r,"Tstar_s")
        if not np.isnan(v):
            data[per]["v"].append(v); data[per]["tc"].append(tc); data[per]["tstar"].append(ts)

    fig,ax=plt.subplots(1,2,figsize=(13,5.2))
    colors={"20ms":"#1f77b4","10ms":"#d62728"}
    markers={"20ms":"o","10ms":"s"}

    # ---- Tc vs speed ----
    for per in ("20ms","10ms"):
        v=np.array(data[per]["v"]); tc=np.array(data[per]["tc"])
        o=np.argsort(v); v,tc=v[o],tc[o]
        m=~np.isnan(tc)
        ax[0].plot(v[m],tc[m],markers[per]+"-",color=colors[per],label=f"measured ({per})",ms=7)
    # theory line
    vv=np.linspace(0.015,0.55,100)
    ax[0].plot(vv,0.42*LAMBDA/vv,"k--",lw=1.3,label="theory 0.42·λ/v")
    ax[0].plot(vv,0.30*LAMBDA/vv,"k:",lw=1,label="0.30·λ/v (fit)")
    ax[0].set_xlabel("UE speed (m/s)"); ax[0].set_ylabel("coherence time Tc (s)")
    ax[0].set_title("Coherence time vs speed")
    
    ax[0].grid(alpha=.3); ax[0].legend(fontsize=8); ax[0].set_ylim(bottom=0)

    # ---- T* vs speed ----
    for per in ("20ms","10ms"):
        v=np.array(data[per]["v"]); ts=np.array(data[per]["tstar"])
        o=np.argsort(v); v,ts=v[o],ts[o]
        m=~np.isnan(ts)
        ax[1].plot(v[m],ts[m],markers[per]+"-",color=colors[per],label=f"T* ({per})",ms=7)
    ax[1].set_xlabel("UE speed (m/s)"); ax[1].set_ylabel("required re-decision period T* (s)")
    ax[1].set_title("Required update rate vs speed")
    
    ax[1].grid(alpha=.3); ax[1].legend(fontsize=8); ax[1].set_ylim(bottom=0)

    fig.suptitle("Mobility sweep — SRS 10 ms vs 20 ms period",fontsize=12)
    fig.tight_layout(rect=[0,0,1,0.95]); fig.savefig(a.out,dpi=140)
    print("wrote",a.out)
    # also print the paired table
    print("\nspeed  Tc(20ms) Tc(10ms)  T*(20ms) T*(10ms)")
    allv=sorted(set(data["20ms"]["v"])|set(data["10ms"]["v"]))
    for vq in allv:
        def pick(per,field):
            for i,vv2 in enumerate(data[per]["v"]):
                if abs(vv2-vq)<1e-6: return data[per][field][i]
            return np.nan
        print(f"{vq:5.2f}  {pick('20ms','tc'):8.3f} {pick('10ms','tc'):8.3f}  {pick('20ms','tstar'):8.3f} {pick('10ms','tstar'):8.3f}")

if __name__=="__main__":
    main()
