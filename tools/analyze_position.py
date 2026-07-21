#!/usr/bin/env python3
"""
analyze_position.py — per-position persistence / exploitability analysis
for one swap_metrics jsonl capture.

Usage:
    python3 analyze_position.py DATA.jsonl
    python3 analyze_position.py DATA.jsonl --trim-start 1.5 --trim-end 1.0
    python3 analyze_position.py DATA.jsonl --seg "regimeA:1.5-4.3" --seg "regimeB:5-11.8"
    python3 analyze_position.py DATA.jsonl --margin 1.0 --csv results.csv

Metrics per segment:
  mean/std/10th pct per RU, best-worst gap, gap/sigma separation ratio,
  argmax share, flip rate, decision lifetime (margin-based), mean regret.
"""
import argparse, json, sys
import numpy as np

def load(path):
    T, C = [], []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                d = json.loads(line)
            except json.JSONDecodeError:
                continue
            u = d.get("users", [{}])[0]
            csi = u.get("csi")
            if csi is None:
                continue
            T.append(d.get("t", np.nan))
            C.append(csi)
    if not C:
        sys.exit("no usable samples in " + path)
    T = np.array(T, dtype=float)
    M = 10.0 * np.log10(np.maximum(np.array(C, dtype=float), 1e-12))
    if np.isnan(T).any():          # no wall-clock field: assume 20 ms cadence
        T = np.arange(len(M)) * 20.0
    return (T - T[0]) / 60000.0, M   # minutes, dB matrix [t x R]

def analyse(name, mins, M, margin):
    n, R = M.shape
    if n < 10:
        print(f"{name:>18}  too few samples ({n})")
        return None
    lin  = 10.0 ** (M / 10.0)
    mean = 10.0 * np.log10(lin.mean(0))   # mean POWER (linear average), reported in dB
    mean_db = M.mean(0)                   # dB-domain (geometric) mean, for reference
    std = M.std(0); p10 = np.percentile(M, 10, axis=0)
    srt  = np.sort(M, 1)
    gap  = srt[:, -1] - srt[:, 0]
    am   = M.argmax(1)
    flips = int((np.diff(am) != 0).sum())
    share = np.bincount(am, minlength=R) / n * 100.0
    sep   = gap.mean() / std.mean()                      # separation ratio
    warm  = min(50, max(1, n // 10))          # ~1 s warm-up window
    pick  = int(M[:warm].mean(0).argmax())    # decide from a short average, not one sample
    best_fixed = int(mean.argmax())  # by mean power           # hindsight-optimal fixed choice
    viol  = np.where(M[:, pick] < M.max(1) - margin)[0]
    life  = (mins[viol[0]] - mins[0]) if len(viol) else np.nan   # nan = censored
    regret = float((M.max(1) - M[:, pick]).mean())
    best_gain = float((M.max(1) - M.mean(1)).mean())

    print(f"\n{name}  (n={n}, {mins[-1]-mins[0]:.1f} min)")
    for r in range(R):
        print(f"   RU{r}: meanP {mean[r]:7.2f}  (geoM {mean_db[r]:7.2f})  std {std[r]:4.2f}  p10 {p10[r]:7.2f}  argmax {share[r]:5.1f}%")
    print(f"   gap best-worst : mean {gap.mean():.2f} dB  (min {gap.min():.2f}, max {gap.max():.2f})")
    print(f"   separation gap/sigma : {sep:.1f}")
    print(f"   flips          : {flips}  ({flips/n*100:.1f}% of samples)")
    print(f"   decision life  : pick RU{pick} -> " +
          (f">{margin:.0f} dB suboptimal after {life:.2f} min" if not np.isnan(life) else f"never (censored at {mins[-1]-mins[0]:.1f} min)"))
    regret_bf = float((M.max(1) - M[:, best_fixed]).mean())
    print(f"   mean regret    : {regret:.2f} dB   (max available gain vs avg RU: {best_gain:.2f} dB)")
    print(f"   best fixed RU  : RU{best_fixed}  regret {regret_bf:.2f} dB  (cost of not adapting)")
    return dict(seg=name, n=n, dur=float(mins[-1]-mins[0]),
                gap=float(gap.mean()), sigma=float(std.mean()), sep=float(sep),
                flips=flips, flip_pct=float(flips/n*100), pick=pick,
                life_min=float(life) if not np.isnan(life) else "",
                regret=regret, best_gain=best_gain, best_fixed=best_fixed, regret_bf=regret_bf,
                **{f"meanP_RU{r}": float(mean[r]) for r in range(R)},
                **{f"geoM_RU{r}": float(mean_db[r]) for r in range(R)},
                **{f"std_RU{r}": float(std[r]) for r in range(R)},
                **{f"argmax_RU{r}": float(share[r]) for r in range(R)})

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("jsonl")
    ap.add_argument("--trim-start", type=float, default=1.5, help="minutes to drop at start (default 1.5)")
    ap.add_argument("--trim-end",   type=float, default=1.0, help="minutes to drop at end (default 1.0)")
    ap.add_argument("--margin",     type=float, default=1.0, help="dB margin for decision lifetime (default 1.0)")
    ap.add_argument("--seg", action="append", default=[], help='extra segment "name:start-end" in minutes')
    ap.add_argument("--csv", help="append summary rows to this CSV file")
    args = ap.parse_args()

    mins, M = load(args.jsonl)
    total = mins[-1]
    print(f"file      : {args.jsonl}")
    print(f"samples   : {len(M)}   duration {total:.1f} min   RUs {M.shape[1]}")
    print(f"trim      : first {args.trim_start} min, last {args.trim_end} min")

    rows = []
    k = (mins >= args.trim_start) & (mins <= total - args.trim_end)
    r = analyse("WHOLE (trimmed)", mins[k], M[k], args.margin)
    if r: rows.append(r)

    for s in args.seg:
        try:
            name, rng = s.split(":"); a, b = [float(x) for x in rng.split("-")]
        except ValueError:
            sys.exit(f'bad --seg "{s}", expected name:start-end')
        kk = (mins >= a) & (mins < b)
        r = analyse(f"{name} [{a}-{b} min]", mins[kk], M[kk], args.margin)
        if r: rows.append(r)

    if args.csv and rows:
        import csv, os
        newfile = not os.path.exists(args.csv)
        with open(args.csv, "a", newline="") as f:
            w = csv.DictWriter(f, fieldnames=["file"] + list(rows[0].keys()))
            if newfile: w.writeheader()
            for row in rows:
                w.writerow({"file": args.jsonl, **row})
        print(f"\nappended {len(rows)} row(s) to {args.csv}")

if __name__ == "__main__":
    main()
