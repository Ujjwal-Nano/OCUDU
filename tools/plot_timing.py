#!/usr/bin/env python3
"""plot_timing.py — decision-cost bar chart from compare_algorithms' _timing.csv.

One bar per method = MEAN decision time on the gNB CPU (us), on a LOG y-axis so
the ~1 us swap and the ~400 us exhaustive are both legible. A star (*) sits on
top of each bar at that method's 99th-percentile time, with a thin stem from the
mean up to it, so the bar shows the typical cost and the star shows the tail.

Apples-to-apples note (matches compare_algorithms): every bar is DECISION-ONLY
(compute + form an allocation), EXCEPT 'swap-applied' (alloc_us) which is the
full applied swap step (decision + apply to the live allocator + bookkeeping).
That bar is hatched to flag it as a different kind of measurement.

Input CSV columns: algorithm,epsilon,mean_us,p50_us,p99_us,max_us
(epsilon is '-' for the single-shot methods; '1'/'2'/'4' for the WRF/GGM sweep).

Usage:
  python3 plot_timing.py run_algocmp.txt_timing.csv -o timing.png
  python3 plot_timing.py t.csv -o timing.png --order mean          # climb by mean
  python3 plot_timing.py t.csv -o timing.png --order family        # group families
  python3 plot_timing.py t.csv -o timing.png --linear              # linear y-axis
  python3 plot_timing.py t.csv -o timing.png --title "U=4, 2 RBG/user"
"""

import argparse, csv, sys
import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
from matplotlib.patches import Patch

# base colour per method family; WRF/GGM epsilon variants are shaded within it
FAMILY = {
    "WRF": "Blues",
    "GGM": "Greens",
    "WUF": "PuBu",
    "Max-Min": "tab:orange",
    "Preference": "tab:red",
    "Bulk": "tab:brown",
    "Exhaustive": "0.45",
    "swap-applied": "tab:purple",
}
# a clean reading order when grouping by family (low -> high, swap stuff first)
FAMILY_ORDER = ["WRF", "GGM", "WUF", "swap-applied",
                "Preference", "Bulk", "Max-Min", "Exhaustive"]


def load(path):
    rows = []
    with open(path) as f:
        for r in csv.DictReader(f):
            try:
                rows.append({
                    "algo": r["algorithm"],
                    "eps": r.get("epsilon", "-"),
                    "mean": float(r["mean_us"]),
                    "p99": float(r["p99_us"]),
                    "max": float(r["max_us"]),
                })
            except (KeyError, ValueError):
                continue
    return rows


def colour_for(rows):
    """Assign a colour per row; epsilon variants shade light->dark within family."""
    # count epsilon variants per family to spread the colormap
    fam_rows = {}
    for i, r in enumerate(rows):
        fam_rows.setdefault(r["algo"], []).append(i)
    col = [None] * len(rows)
    for algo, idxs in fam_rows.items():
        spec = FAMILY.get(algo, "tab:gray")
        if isinstance(spec, str) and spec in plt.colormaps():
            cmap = plt.get_cmap(spec)
            shades = np.linspace(0.55, 0.9, len(idxs))
            for j, i in enumerate(idxs):
                col[i] = cmap(shades[j])
        else:
            for i in idxs:
                col[i] = spec
    return col


def label_for(r):
    return f"{r['algo']}\n$\\epsilon$={r['eps']}" if r["eps"] not in ("-", "", None) else r["algo"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv", help="the _timing.csv written by compare_algorithms.py")
    ap.add_argument("-o", "--out", default="timing.png")
    ap.add_argument("--order", choices=["mean", "family", "file"], default="mean",
                    help="mean: climb by mean time (default); family: group methods; "
                         "file: keep CSV order")
    ap.add_argument("--linear", action="store_true", help="linear y-axis (default: log)")
    ap.add_argument("--title", default=None, help="extra context for the title")
    ap.add_argument("--label", default=None)
    a = ap.parse_args()

    rows = load(a.csv)
    if not rows:
        sys.exit("no usable rows in %s" % a.csv)

    if a.order == "mean":
        rows.sort(key=lambda r: r["mean"])
    elif a.order == "family":
        rank = {f: i for i, f in enumerate(FAMILY_ORDER)}
        rows.sort(key=lambda r: (rank.get(r["algo"], 99),
                                 float(r["eps"]) if r["eps"] not in ("-", "", None) else 0.0))
    # 'file' keeps load order

    x = np.arange(len(rows))
    mean = np.array([r["mean"] for r in rows])
    p99 = np.array([r["p99"] for r in rows])
    cols = colour_for(rows)
    labels = [label_for(r) for r in rows]

    # log axis needs a positive floor for the bar bottoms
    floor = max(mean.min() / 3.0, 1e-3)

    fig, ax = plt.subplots(figsize=(max(8.0, 1.05 * len(rows)), 6.0))
    bars = ax.bar(x, mean, width=0.68, color=cols, edgecolor="black",
                  linewidth=0.7, bottom=(floor if not a.linear else 0.0), zorder=3)
    # hatch the applied-swap bar: it is the full applied step, not decision-only
    for r, b in zip(rows, bars):
        if r["algo"] == "swap-applied":
            b.set_hatch("///")

    # stem from bar top (mean) up to p99, then the star at p99
    ax.vlines(x, mean, np.maximum(p99, mean), color="0.35", lw=1.2, zorder=4)
    ax.scatter(x, p99, marker="*", s=240, color="gold", edgecolor="black",
               linewidth=0.7, zorder=5)

    # annotate mean (on the bar) and p99 (above the star)
    for xi, m, p in zip(x, mean, p99):
        ax.annotate(f"{m:.2f}", (xi, m), ha="center", va="top",
                    xytext=(0, -3), textcoords="offset points",
                    fontsize=8.5, color="white", fontweight="bold", zorder=6)
        ax.annotate(f"{p:.2f}", (xi, p), ha="center", va="bottom",
                    xytext=(0, 7), textcoords="offset points",
                    fontsize=8, color="0.2", zorder=6)

    if not a.linear:
        ax.set_yscale("log")
    ax.set_xticks(x)
    ax.set_xticklabels(labels, fontsize=9.5)
    ax.set_ylabel("decision time on the gNB CPU  (µs)"
                  + ("  — log scale" if not a.linear else ""), fontsize=12.5)
    ttl = "Per-method decision cost  (bar = mean,  ★ = p99)"
    if a.title:
        ttl += f"\n{a.title}"
    elif a.label:
        ttl += f"\n{a.label}"
    ax.set_title(ttl, fontsize=12.5)
    ax.grid(axis="y", alpha=0.3, which="both", zorder=0)
    # headroom above the tallest star
    top = p99.max()
    if not a.linear:
        ax.set_ylim(floor, top * 2.2)
    else:
        ax.set_ylim(0, top * 1.18)

    legend = [
        Line2D([0], [0], marker="*", color="none", markerfacecolor="gold",
               markeredgecolor="black", markersize=14, label="99th percentile"),
        Patch(facecolor="0.7", edgecolor="black", label="mean (bar height)"),
        Patch(facecolor="tab:purple", edgecolor="black", hatch="///",
              label="swap-applied = full applied step (not decision-only)"),
    ]
    ax.legend(handles=legend, fontsize=9.5, loc="upper left", framealpha=0.95)

    fig.tight_layout()
    fig.savefig(a.out, dpi=150)
    plt.close(fig)

    # text summary alongside
    txt = a.out.rsplit(".", 1)[0] + ".txt"
    with open(txt, "w") as f:
        f.write(f"decision-cost bars — {a.label or a.csv.split('/')[-1]}\n")
        f.write(f"{'method':<16}{'mean_us':>10}{'p99_us':>10}{'max_us':>10}\n")
        f.write("-" * 46 + "\n")
        for r in rows:
            lab = f"{r['algo']}" + (f" e{r['eps']}" if r["eps"] not in ("-", "", None) else "")
            f.write(f"{lab:<16}{r['mean']:>10.3f}{r['p99']:>10.3f}{r['max']:>10.3f}\n")
    print("wrote", a.out, "and", txt)


if __name__ == "__main__":
    main()
