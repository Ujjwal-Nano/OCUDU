#!/usr/bin/env python3
"""Truncate a raw per-RB capture at a given elapsed-minute cutoff.
Usage: python3 truncate_capture.py <in.rb.jsonl.gz> <out.rb.jsonl.gz> <cutoff_min>"""
import sys, gzip, json
inp, out, cut = sys.argv[1], sys.argv[2], float(sys.argv[3])
op = gzip.open if inp.endswith(".gz") else open
t0 = None; kept = 0; total = 0
with op(inp,"rt") as f, gzip.open(out,"wt") as g:
    for line in f:
        line=line.strip()
        if not line: continue
        total+=1
        try: d=json.loads(line)
        except: continue
        if t0 is None: t0=d["t"]
        elapsed_min = (d["t"]-t0)/60000.0
        if elapsed_min <= cut:
            g.write(line+"\n"); kept+=1
print(f"kept {kept}/{total} lines (<= {cut} min)")
