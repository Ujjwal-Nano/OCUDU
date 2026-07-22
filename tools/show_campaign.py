#!/usr/bin/env python3
"""
show_campaign.py — pretty-print the campaign.csv summary table.

Usage:
    python3 show_campaign.py ~/OCUDU/datasets/campaign.csv
    python3 show_campaign.py campaign.csv --full     # all columns, one block per row
"""
import argparse, csv, os, sys

def f(v, nd=2):
    try:
        return f"{float(v):.{nd}f}"
    except (TypeError, ValueError):
        return "-"

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv", nargs="?", default=os.path.expanduser("~/OCUDU/datasets/campaign.csv"))
    ap.add_argument("--full", action="store_true")
    a = ap.parse_args()
    if not os.path.exists(a.csv):
        sys.exit("not found: " + a.csv)
    rows = list(csv.DictReader(open(a.csv)))
    if not rows:
        sys.exit("empty csv")

    if a.full:
        for r in rows:
            print("=" * 70)
            for k, v in r.items():
                print(f"  {k:>14}: {v}")
        return

    hdr = (f"{'position':<28}{'seg':<10}{'n':>6}{'dur':>6}"
           f"{'gap':>7}{'sig':>6}{'sep':>6}{'flip%':>7}{'regret':>8}{'noAdapt':>9}{'genie':>7}  verdict")
    print(hdr); print("-" * len(hdr))
    for r in rows:
        name = os.path.basename(r["file"]).replace(".jsonl", "")
        name = name[:27]
        seg = r["seg"].split(" ")[0][:9]
        sep = float(r["sep"]) if r["sep"] else 0
        gap = float(r["gap"]) if r["gap"] else 0
        verdict = ("strong: measure once" if sep >= 8 else
                   "usable" if sep >= 3 or gap >= 2.5 else
                   "weak: little to gain")
        print(f"{name:<28}{seg:<10}{r['n']:>6}{f(r['dur'],1):>6}"
              f"{f(gap):>7}{f(r['sigma']):>6}{f(sep,1):>6}{f(r['flip_pct'],1):>7}"
              f"{f(r['regret']):>8}{f(r.get('regret_bf')):>9}{f(r['best_gain']):>7}  {verdict}")

    print("""
columns
  gap     mean best-worst RU difference per sample (dB)  -> size of the opportunity
  sig     mean per-RU std over time (dB)                 -> how noisy the channel is
  sep     gap / sig                                      -> is the opportunity distinguishable?
  flip%   share of samples where the best RU changed     -> raw ranking churn (noise-sensitive)
  regret  dB lost by keeping the warm-up pick            -> cost of deciding once, early
  noAdapt dB lost by the best FIXED RU vs per-sample max -> value of continuous adaptation
  genie   per-sample max minus average RU (dB)           -> total opportunity, upper bound
""")

if __name__ == "__main__":
    main()
