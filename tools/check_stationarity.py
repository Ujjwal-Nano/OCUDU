#!/usr/bin/env python3
"""
check_stationarity.py — sanity check for the frequency-correlation normalization
in metrics_suite.py.

metrics_suite.py normalizes per-RB power by a GLOBAL mean over the whole capture
before computing frequency correlation (Bc). That's only safe if the wideband
power is roughly stationary over time. This script plots wideband power vs time
for one or more captures and flags runs where it drifts/ramps (likely from
motion / shadowing), which would bias Bc high.

Usage:
  python3 check_stationarity.py CAP1.rb.jsonl[.gz] [CAP2 ...] \
      [-o stationarity.png] [--avg-win MS] [--trim-start 1.5] [--trim-end 1.0] \
      [--drift-thresh 3.0]

--drift-thresh: peak-to-peak swing (dB) of the smoothed wideband power above
which a run is flagged as "non-stationary". 3 dB is a reasonable starting
point — adjust based on what you see.
"""

import argparse, gzip, json, sys
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def load(path, trim_start, trim_end):
    op = gzip.open if path.endswith(".gz") else open
    T, P = [], []
    with op(path, "rt") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                d = json.loads(line)
            except json.JSONDecodeError:
                continue
            T.append(d["t"])
            P.append(d["rb"])
    if not P:
        sys.exit("no samples in " + path)
    T = np.array(T, float)
    P = np.array(P, float)
    P = P[:, 1:]
    mins = (T - T[0]) / 60000.0
    k = (mins >= trim_start) & (mins <= mins[-1] - trim_end)
    if k.sum() < 100:
        k = np.ones(len(mins), bool)
    T, P, mins = T[k], P[k], mins[k]
    fs = 1000.0 / np.median(np.diff(T))
    return P, mins, fs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("caps", nargs="+", help="one or more capture files")
    ap.add_argument("-o", "--out", default="stationarity.png")
    ap.add_argument("--avg-win", type=float, default=60.0,
                     help="block-average window in ms, matches --mobile default in metrics_suite.py")
    ap.add_argument("--trim-start", type=float, default=1.5)
    ap.add_argument("--trim-end", type=float, default=1.0)
    ap.add_argument("--drift-thresh", type=float, default=3.0,
                     help="peak-to-peak dB swing (smoothed) above which a run is flagged")
    ap.add_argument("--trend-thresh", type=float, default=0.15,
                     help="|slope| in dB/min above which a run is flagged (this is the primary criterion)")
    ap.add_argument("--smooth-s", type=float, default=5.0,
                     help="smoothing window in seconds (was nblk//20 before, too short for slow fading)")
    a = ap.parse_args()

    n = len(a.caps)
    fig, axes = plt.subplots(n, 1, figsize=(11, 3.2 * n), squeeze=False)
    axes = axes[:, 0]

    print(f"{'file':45s} {'p2p(dB)':>9s} {'trend(dB/min)':>14s}  flag")
    print("-" * 85)

    for ax, cap in zip(axes, a.caps):
        P, mins, fs = load(cap, a.trim_start, a.trim_end)
        win_s = a.avg_win / 1000.0
        W = max(1, int(round(win_s * fs)))

        # same block-averaging metrics_suite.py uses before frequency correlation
        blk = W
        nblk = len(P) // blk
        if nblk < 3:
            print(f"{cap:45s}  too short to block-average, skipping")
            continue
        B = P[: nblk * blk].reshape(nblk, blk, -1).mean(1)
        wideband_db = 10 * np.log10(np.maximum(B.sum(1), 1e-15))
        t_blk = mins[: nblk * blk].reshape(nblk, blk).mean(1)

        # smooth over a fixed real-time window (seconds), not a fraction of block count,
        # so the window actually spans multiple fading cycles and isolates slow shadowing
        blk_dt = (t_blk[-1] - t_blk[0]) * 60.0 / max(1, nblk - 1)  # seconds per block
        sm_w = max(1, int(round(a.smooth_s / max(blk_dt, 1e-9))))
        kern = np.ones(sm_w) / sm_w
        smooth = np.convolve(wideband_db, kern, mode="same")

        p2p = float(smooth.max() - smooth.min())
        # linear trend, dB per minute -- this is the primary indicator of the kind of
        # drift that breaks the global-mean normalization (monotonic/slow shadowing),
        # as opposed to fast fading which still shows up as p2p but averages out.
        if t_blk[-1] > t_blk[0]:
            trend = float(np.polyfit(t_blk, smooth, 1)[0])
        else:
            trend = 0.0

        flag = "DRIFT" if abs(trend) > a.trend_thresh else "ok"
        label = cap.split("/")[-1]
        print(f"{label:45s} {p2p:9.2f} {trend:14.3f}  {flag}")

        ax.plot(t_blk, wideband_db, lw=0.6, color="#aaaaaa", label="raw (per block)")
        ax.plot(t_blk, smooth, lw=2, color="#d62728" if flag == "DRIFT" else "#2ca02c",
                 label=f"smoothed (p2p={p2p:.1f} dB, trend={trend:.2f} dB/min)")
        ax.set_title(f"{label}  —  {flag}", fontsize=10,
                     color="#d62728" if flag == "DRIFT" else "#333333")
        ax.set_xlabel("time (min)")
        ax.set_ylabel("wideband power (dB)")
        ax.grid(alpha=0.3)
        ax.legend(fontsize=8, loc="best")

    fig.suptitle("Wideband power stationarity check (for Bc normalization validity)", fontsize=12)
    fig.tight_layout(rect=[0, 0, 1, 0.97])
    fig.savefig(a.out, dpi=130)
    print(f"\nwrote {a.out}")
    print(f"\nRule of thumb: |trend| > {a.trend_thresh} dB/min (smoothed over {a.smooth_s:.0f}s)")
    print("means the global-mean normalization in metrics_suite.py's frequency-correlation")
    print("panel is likely biasing Bc for that run. p2p alone is not a reliable flag --")
    print("fast fading still shows up there even in stationary runs.")


if __name__ == "__main__":
    main()
