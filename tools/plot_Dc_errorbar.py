#!/usr/bin/env python3
"""Dc/lambda vs speed, one SRS rate, error bars + constant mean band."""
import csv, argparse
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
C=299_792_458.0
def vmean(v,x,T_acc):
    full=2*x; Tc=(full-v*T_acc)/v
    return v if Tc<=0 else full/(2*T_acc+Tc)
def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("csv"); ap.add_argument("--win",type=int,default=10)
    ap.add_argument("--x",type=float,default=0.5); ap.add_argument("--t-acc",type=float,default=0.5)
    ap.add_argument("--freq-ghz",type=float,default=3.75)
    ap.add_argument("--speed",choices=["v_eff","setpoint"],default="v_eff")
    ap.add_argument("--drop-speed",type=float,nargs="*",default=[0.02])
    ap.add_argument("--reliable-min",type=float,default=0.0)
    ap.add_argument("--err-samples",type=float,default=0.5)
    ap.add_argument("-o","--out",default="Dc_errorbar.png")
    args=ap.parse_args()
    lam=C/(args.freq_ghz*1e9); rows=[]
    with open(args.csv) as f:
        for r in csv.DictReader(f):
            try:
                if int(float(r["avg_win_ms"]))!=args.win: continue
                vs=float(r["speed_mps"])
                if vs in args.drop_speed: continue
                rows.append((vs,float(r["Tc_meas_s"])))
            except (ValueError,KeyError): continue
    rows.sort()
    vs=np.array([r[0] for r in rows]); Tc=np.array([r[1] for r in rows])
    veff=np.array([vmean(v,args.x,args.t_acc) for v in vs])
    vx=veff if args.speed=="v_eff" else vs
    Dc=vx*Tc/lam
    dDc=vx*(args.err_samples*args.win/1000.0)/lam
    mask=vs>=args.reliable_min
    m=Dc[mask].mean(); sd=Dc[mask].std()
    fig,ax=plt.subplots(figsize=(8,5.2))
    ax.axhline(m,color='green',lw=1.6,label=f'mean = {m:.3f} λ  (±{sd:.3f})')
    ax.fill_between([vx.min()*0.9,vx.max()*1.05],m-sd,m+sd,color='green',alpha=0.15)
    ax.errorbar(vx,Dc,yerr=dDc,fmt='o',color='#d62728',ms=8,capsize=4,lw=1.4,
                label=f'measured ({args.win} ms)')
    lab='effective (motion-mean)' if args.speed=='v_eff' else 'setpoint'
    ax.set_xlabel(f'{lab} UE speed  (m/s)')
    ax.set_ylabel('coherence distance  $D_c/\\lambda$')
    ax.set_title(f'Coherence distance vs speed — {args.win} ms SRS',fontsize=12)
    ax.set_ylim(0,max(0.45,(Dc+dDc).max()*1.1))
    ax.set_xlim(vx.min()*0.9,vx.max()*1.05)
    ax.grid(alpha=0.3); ax.legend(fontsize=9,loc='upper right')
    plt.tight_layout(); plt.savefig(args.out,dpi=120)
    print(f"wrote {args.out}")
    print(f"mean(v>={args.reliable_min}) = {m:.3f} ± {sd:.3f} λ = {m*lam*100:.2f} cm")
    for v,d,e in zip(vx,Dc,dDc): print(f"  v={v:.3f}  Dc={d:.3f}±{e:.3f}")
if __name__=="__main__": main()
