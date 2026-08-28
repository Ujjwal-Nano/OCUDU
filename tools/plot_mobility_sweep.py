#!/usr/bin/env python3
"""Tc-vs-speed and T*-vs-speed, both SRS periods. Period from avg_win_ms column."""
import argparse, csv
import numpy as np, matplotlib
matplotlib.use("Agg"); import matplotlib.pyplot as plt
import matplotlib.ticker
LAMBDA=0.08

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("csv"); ap.add_argument("-o","--out",default="mobility_sweep.png")
    ap.add_argument("--err-samples",type=float,default=0.5)
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
        win=getf(r,"avg_win_ms")
        per={10:"10ms",20:"20ms"}.get(int(win) if not np.isnan(win) else -1)
        if per is None: continue
        v=getf(r,"speed_mps")
        if not np.isnan(v) and abs(v-0.02)<1e-6: continue
        tc=getf(r,"Tc_plateau_s","Tc_meas_s","Tc_meas"); ts=getf(r,"Tstar_s")
        if not np.isnan(v):
            data[per]["v"].append(v); data[per]["tc"].append(tc); data[per]["tstar"].append(ts)

    fig,ax=plt.subplots(1,2,figsize=(13,5.2))
    colors={"20ms":"#1f77b4","10ms":"#d62728"}; markers={"20ms":"o","10ms":"s"}
    per_ms={"20ms":0.020,"10ms":0.010}

    # ---- Tc vs speed (with error bars) ----
    all_tc=[]
    for per in ("20ms","10ms"):
        v=np.array(data[per]["v"]); tc=np.array(data[per]["tc"])
        o=np.argsort(v); v,tc=v[o],tc[o]; m=~np.isnan(tc)
        dtc=a.err_samples*per_ms[per]  # symmetric Tc resolution
        ax[0].errorbar(v[m],tc[m],yerr=dtc,fmt=markers[per]+"-",color=colors[per],
                       label=f"measured ({per})",ms=7,capsize=3,lw=1.3)
        all_tc.extend(tc[m].tolist())
    vv=np.linspace(0.015,0.55,100)
    ax[0].plot(vv,0.42*LAMBDA/vv,"k--",lw=1.3,label="theory 0.42·λ/v")
    ax[0].set_xlabel("UE speed (m/s)"); ax[0].set_ylabel("coherence time Tc (s)")
    ax[0].set_title("Coherence time vs speed"); ax[0].set_yscale("log")
    if all_tc:
        lo,hi=min(all_tc),max(all_tc)
        cands=[mm*10.0**e for e in range(int(np.floor(np.log10(lo))),int(np.ceil(np.log10(hi)))+1) for mm in (1,2,5)]
        yt=sorted(t for t in cands if lo*0.9<=t<=hi*1.1)
        ax[0].set_yticks(yt); ax[0].set_yticklabels([f"{t:g}" for t in yt])
        ax[0].yaxis.set_minor_formatter(matplotlib.ticker.NullFormatter())
    ax[0].set_xlim(0.048,0.502); ax[0].grid(alpha=.3,which="both"); ax[0].legend(fontsize=8)

    # ---- T* vs speed (with error bars) ----
    for per in ("20ms","10ms"):
        v=np.array(data[per]["v"]); ts=np.array(data[per]["tstar"])
        o=np.argsort(v); v,ts=v[o],ts[o]; m=~np.isnan(ts)
        dts=a.err_samples*per_ms[per]
        ax[1].errorbar(v[m],ts[m],yerr=dts,fmt=markers[per]+"-",color=colors[per],
                       label=f"T* ({per})",ms=7,capsize=3,lw=1.3)
    ax[1].set_xlabel("UE speed (m/s)"); ax[1].set_ylabel("required re-decision period T* (s)")
    ax[1].set_title("Required update rate vs speed")
    ax[1].set_xlim(0.048,0.502); ax[1].grid(alpha=.3); ax[1].legend(fontsize=8); ax[1].set_ylim(bottom=0)

    fig.suptitle("Mobility sweep",fontsize=12)
    fig.tight_layout(rect=[0,0,1,0.95]); fig.savefig(a.out,dpi=140)
    print("wrote",a.out)
    print("\nspeed  Tc(20ms) Tc(10ms)  T*(20ms) T*(10ms)")
    allv=sorted(set(data["20ms"]["v"])|set(data["10ms"]["v"]))
    for vq in allv:
        def pick(per,field):
            for i,vv2 in enumerate(data[per]["v"]):
                if abs(vv2-vq)<1e-6: return data[per][field][i]
            return np.nan
        print(f"{vq:5.2f}  {pick('20ms','tc'):8.3f} {pick('10ms','tc'):8.3f}  {pick('20ms','tstar'):8.3f} {pick('10ms','tstar'):8.3f}")

if __name__=="__main__": main()
