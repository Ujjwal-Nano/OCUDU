#!/usr/bin/env python3
"""
mobility_tc.py — window-independent coherence time via plateau detection.

For a capture, sweeps the averaging window and finds the plateau where Tc is
stable (noise-suppressed but not smoothing-biased). Reports that Tc, or flags
resolution-limited if no plateau exists.

Usage:
  python3 mobility_tc.py <capture.rb.jsonl.gz> --speed V [--windows 20,30,40,60,80,100,150,200]
"""

import argparse, gzip, json, sys, subprocess, re, os, glob
import numpy as np

LAMBDA = 0.08  # 3.75 GHz


def load_rb(path, ts=1.0, te=1.0):
    op = gzip.open if path.endswith(".gz") else open
    T, P = [], []
    with op(path, "rt") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                d = json.loads(line)
            except:
                continue
            T.append(d["t"])
            P.append(d["rb"])
    T = np.array(T, float)
    P = np.array(P, float)[:, 1:]
    mins = (T - T[0]) / 60000.0
    k = (mins >= ts) & (mins <= mins[-1] - te)
    if k.sum() < 100:
        k = np.ones(len(mins), bool)
    T, P = T[k], P[k]
    fs = 1000.0 / np.median(np.diff(T))
    return T, P, fs


def tc_at_window(P, fs, win_ms, K=12):
    """coherence time (0.5 crossing) of RU0 after win_ms moving average."""
    R = P.shape[1] // K
    ru = np.stack([P[:, r * K : (r + 1) * K].mean(1) for r in range(R)], 1)
    m = 10 * np.log10(ru[:, 0])
    w = max(1, int(round(win_ms / 1000.0 * fs)))
    if w > 1:
        m = np.convolve(m, np.ones(w) / w, mode="valid")
    m = m - m.mean()
    n = len(m)
    ac = np.correlate(m, m, mode="full")[n - 1 :]
    ac = ac / ac[0]
    below = np.where(ac < 0.5)[0]
    if len(below) == 0:
        return np.nan
    i = below[0]
    if i == 0:
        return 0.0
    frac = (ac[i - 1] - 0.5) / (ac[i - 1] - ac[i])
    return (i - 1 + frac) / fs


def find_plateau(P, fs, windows, tol=0.011):
    """Return (Tc, win_used, status, sweep).
    A plateau is a run of windows over which Tc is FLAT (max-min <= tol), i.e. all
    values equal within one sample. Prefer the plateau at the SHORTEST windows
    (inflation grows with window, so the earliest flat region is the true value).
    """
    tcs = [tc_at_window(P, fs, w) for w in windows]
    best = None
    n = len(windows)
    for i in range(n):
        if np.isnan(tcs[i]):
            continue
        j = i
        while (
            j + 1 < n
            and not np.isnan(tcs[j + 1])
            and (max(tcs[i : j + 2]) - min(tcs[i : j + 2])) <= tol
        ):
            j += 1
        run_len = j - i + 1
        key = (run_len, -i)
        if run_len >= 2 and (best is None or key > best[0:2]):
            best = (run_len, -i, i, j)
    if best is not None:
        _, _, i, j = best
        vals = tcs[i : j + 1]
        tc = float(np.median(vals))
        win = windows[i]
        return tc, win, "plateau", list(zip(windows, tcs))
    else:
        return np.nan, None, "NO_PLATEAU", list(zip(windows, tcs))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cap", nargs="?")
    ap.add_argument("--speed", type=float)
    ap.add_argument("--windows", default="20,30,40,60,80,100,150,200")
    ap.add_argument("--trim-start", type=float, default=1.0)
    ap.add_argument("--trim-end", type=float, default=1.0)
    ap.add_argument("--csv", default=None, help="append a row to this CSV")
    a = ap.parse_args()

    windows = [float(x) for x in a.windows.split(",")]
    T, P, fs = load_rb(a.cap, a.trim_start, a.trim_end)
    tc, win, status, sweep = find_plateau(P, fs, windows)
    theory = 0.42 * LAMBDA / a.speed if a.speed else np.nan

    print(f"file    : {a.cap.split('/')[-1]}")
    print(f"speed   : {a.speed} m/s   fs {fs:.0f} Hz")
    print("window sweep:")
    for w, t in sweep:
        mark = (
            " <-- plateau"
            if (status == "plateau" and not np.isnan(t) and abs(t - tc) <= 0.011)
            else ""
        )
        print(
            f"  {w:5.0f} ms : Tc = {t:.3f} s{mark}"
            if not np.isnan(t)
            else f"  {w:5.0f} ms : Tc = --"
        )
    if status == "plateau":
        ratio = tc / theory if theory == theory else float("nan")
        print(
            f"RESULT  : Tc = {tc:.3f} s  (plateau, window {win:.0f} ms)   theory {theory:.3f}   ratio {ratio:.2f}"
        )
    else:
        print(
            f"RESULT  : NO PLATEAU — resolution limited (Tc keeps rising with window)"
        )
        print(
            f"          shortest-window Tc = {sweep[0][1]:.3f} s is a lower bound; need faster SRS to resolve"
        )

    if a.csv:
        import csv

        newf = not os.path.exists(a.csv)
        row = {
            "file": a.cap.split("/")[-1],
            "speed_mps": a.speed,
            "Tc_plateau_s": round(tc, 3) if tc == tc else "",
            "plateau_window_ms": int(win) if win else "",
            "Tc_theory_s": round(theory, 3) if theory == theory else "",
            "ratio": round(tc / theory, 3) if (tc == tc and theory == theory) else "",
            "status": status,
        }
        with open(a.csv, "a", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(row.keys()))
            if newf:
                w.writeheader()
            w.writerow(row)
        print(f"appended to {a.csv}")


if __name__ == "__main__":
    main()
