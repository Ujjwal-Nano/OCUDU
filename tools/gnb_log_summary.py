#!/usr/bin/env python3
"""Summarise a gNB console log: distributions of CQI/MCS/RSRP/PUSCH + event counts.
Usage: python3 gnb_log_summary.py <gnb_console.txt[.gz]>"""
import sys, gzip, re
from collections import Counter

def op(p): return gzip.open(p,"rt") if p.endswith(".gz") else open(p)

path = sys.argv[1]
cqi, mcs_dl, mcs_ul, rsrp, pusch = Counter(), Counter(), Counter(), [], []
events = Counter()
rows = 0

for line in op(path):
    l = line.strip()
    # major events
    if "==== gNB started" in l: events["gnb_started"] += 1
    if "Connection to AMF" in l or "N2" in l and "completed" in l: events["n2_connected"] += 1
    for kw in ("release","Release","RLF","reestablish","reconfig failure","failure"):
        if kw in l: events[f"evt:{kw.lower()}"] += 1
    # metrics table rows: pci rnti | cqi ri mcs brate ok nok (%) dl_bs | pusch rsrp ri mcs ...
    m = re.match(r'\s*\d+\s+[0-9a-fx]+\s*\|\s*(\d+)\s+[\d.]+\s+(\d+)\s+\S+\s+\d+\s+\d+\s+\d+%.*\|\s*([\-\d.]+|n/a)\s+([\-\d.]+|n/a)\s+[\d.]+\s+(\d+)', l)
    if m:
        rows += 1
        cqi[int(m.group(1))] += 1
        mcs_dl[int(m.group(2))] += 1
        if m.group(3) != "n/a": pusch.append(float(m.group(3)))
        if m.group(4) != "n/a": rsrp.append(float(m.group(4)))
        mcs_ul[int(m.group(5))] += 1

def dist(name, c):
    if not c: print(f"{name}: (none)"); return
    tot = sum(c.values())
    print(f"{name}:")
    for k in sorted(c):
        n = c[k]; bar = "#"*int(40*n/tot)
        print(f"  {k:>3} | {n:6d} {100*n/tot:5.1f}%  {bar}")

def stat(name, xs):
    if not xs: print(f"{name}: (none)"); return
    xs=sorted(xs); n=len(xs)
    print(f"{name}: n={n}  min {xs[0]:.1f}  p10 {xs[int(.1*n)]:.1f}  median {xs[n//2]:.1f}  p90 {xs[int(.9*n)]:.1f}  max {xs[-1]:.1f}")

print(f"=== {path} ===  ({rows} metric rows)\n")
dist("CQI", cqi)
dist("MCS DL", mcs_dl)
dist("MCS UL", mcs_ul)
stat("PUSCH SNR (dB)", pusch)
stat("RSRP (dBm)", rsrp)
print("\nEvents:")
for k in sorted(events): print(f"  {k}: {events[k]}")
