#!/usr/bin/env python3
"""Compute SRS SNR (= rsrp - noise_var) distribution from an archived gNB SRS log.
Usage: python3 srs_snr.py <gnb_srs_test.log.gz>"""
import sys, gzip, re
import numpy as np
op = gzip.open if sys.argv[1].endswith(".gz") else open
snr=[]
pat=re.compile(r"rsrp=(-?[\d.]+)dB\s+noise_var=(-?[\d.]+)dB")
for line in op(sys.argv[1],"rt"):
    if "SRS:" not in line: continue
    m=pat.search(line)
    if m:
        rsrp=float(m.group(1)); nv=float(m.group(2))
        snr.append(rsrp-nv)
snr=np.array(snr)
if len(snr)==0: sys.exit("no SRS rsrp/noise_var lines found")
print(f"SRS occasions: {len(snr)}")
print(f"SRS SNR (rsrp - noise_var):")
print(f"  median {np.median(snr):6.1f} dB   mean {snr.mean():6.1f} dB")
print(f"  p10 {np.percentile(snr,10):6.1f}   p90 {np.percentile(snr,90):6.1f}   min {snr.min():.1f}  max {snr.max():.1f}")
print(f"  fraction below 6 dB: {100*(snr<6).mean():.1f}%")
