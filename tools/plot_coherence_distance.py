#!/usr/bin/env python3
"""Dc/lambda vs speed, both SRS rates. Left=setpoint, right=motion-mean. Zero-based y, error bars."""
import csv, argparse
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
C=299_792_458.0

def vmean(v_set,x,T_acc):
    full=2*x; T_const=(full-v_set*T_acc)/v_set
    return v_set if T_const<=0 else full/(2*T_acc+T_const)

def load(path):
    rows=[]
    with open(path) as f:
        for r in csv.DictReader(f):
            try:
                rows.append({"v_set":float(r["speed_mps"]),
                             "Tc":float(r["Tc_meas_s"]),
                             "win":int(float(r["avg_win_ms"]))})
            except (ValueError,KeyError): continue
    return rows

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("csv")
    ap.add_argument("--x",type=float,default=0.5)
    ap.add_argument("--t-acc",type=float,default=0.5)
    ap.add_argument("--freq-ghz",type=float,default=3.75)
    ap.add_argument("--drop-speed",type=float,nargs="*",default=[0.02])
    ap.add_argument("--err-samples",type=float,default=0.5)
    ap.add_argument("--only-win",type=int,default=None,help="restrict to one SRS rate (10 or 20)")
    ap.add_argument("-o","--out",default="coherence_distance.png")
    ap.add_argument("--win",type=int,default=None)
    args=ap.parse_args()
    lam=C/(args.freq_ghz*1e9)
    rows=[r for r in load(args.csv) if r["v_set"] not in args.drop_speed]
    def subset(win):
        rs=sorted([r for r in rows if r["win"]==win],key=lambda r:r["v_set"])
        vset=np.array([r["v_set"] for r in rs])
        veff=np.array([vmean(v,args.x,args.t_acc) for v in vset])
        Tc=np.array([r["Tc"] for r in rs])
        return vset,veff,Tc
    d10,d20=subset(10),subset(20)
    fig,axes=plt.subplots(1,2,figsize=(12,5),sharey=True)
    series=[(d10,"#d62728","10 ms SRS",10),(d20,"#1f77b4","20 ms SRS",20)]
    if args.only_win is not None:
        series=[s for s in series if s[3]==args.only_win]
    for ax,mode in zip(axes,["setpoint","motion-mean"]):
        for (vset,veff,Tc),col,lab,win in series:
            if len(vset)==0: continue
            vx=vset if mode=="setpoint" else veff
            Dc=vx*Tc/lam
            dDc=vx*(args.err_samples*win/1000.0)/lam
            m=Dc.mean()
            ax.errorbar(vx,Dc,yerr=dDc,fmt='o-',color=col,ms=7,lw=1.5,capsize=3,
                        label=f'{lab} (mean={m:.3f}\u03bb)')
        ax.set_xlabel(f"UE speed (m/s) [{mode}]"); ax.grid(alpha=0.3); ax.legend(fontsize=9)
        ax.set_ylim(0,None)
    axes[0].set_ylabel("coherence distance $D_c/\\lambda$")
    axes[0].set_title("Setpoint speed"); axes[1].set_title("Motion-mean speed")
    fig.suptitle("Coherence distance vs speed",fontsize=12)
    plt.tight_layout(rect=[0,0,1,0.95]); plt.savefig(args.out,dpi=120)
    print("wrote",args.out)

if __name__=="__main__": main()
