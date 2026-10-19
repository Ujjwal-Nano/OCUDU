#!/usr/bin/env python3
"""plot_timing.py — decision-cost bar chart (timing counterpart to the fairness
comparison). Reads the gNB's on-CPU decision timers from swap.jsonl.

Bars grouped by policy: WRF at every epsilon together, GGM at every epsilon
together, then a Max-Min bar. Bar height = MEAN decision time; a star on top of
each bar marks the 99th percentile, labelled with its value. Log y so the
sub-microsecond swap bars and the larger baseline bar are both legible.

Usage:
  python3 plot_timing.py swap.jsonl -o out/timing
  python3 plot_timing.py swap.jsonl --wrf-eps 1,2,4 --ggm-eps 1,2,4 -o out/timing
  python3 plot_timing.py swap.jsonl --linear --label E6_4U -o out/timing
"""

import argparse
import json
import sys

import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


def parse_eps(s):
    return [int(x) for x in s.split(",") if x.strip()]


def load(path):
    """Collect every logged value of each decision timer, plus (users, rpu)."""
    acc = {}
    meta = {"users": None, "rus_per_user": None}
    for ln in open(path):
        ln = ln.strip()
        if not ln:
            continue
        try:
            d = json.loads(ln)
        except json.JSONDecodeError:
            continue
        if meta["users"] is None and isinstance(d.get("users"), list):
            meta["users"] = len(d["users"])
        if meta["rus_per_user"] is None and d.get("rus_per_user") is not None:
            meta["rus_per_user"] = int(d["rus_per_user"])
        for k in (
            "maxmin_us",
            "opt_alloc_us",
            "wrf_us_e1",
            "wrf_us_e2",
            "wrf_us_e4",
            "ggm_us_e1",
            "ggm_us_e2",
            "ggm_us_e4",
        ):
            v = d.get(k)
            if v is not None and float(v) >= 0:
                acc.setdefault(k, []).append(float(v))
    return {k: np.asarray(v) for k, v in acc.items() if v}, meta


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("swaplog")
    ap.add_argument("--wrf-eps", default="1,2,4")
    ap.add_argument("--ggm-eps", default="1,2,4")
    ap.add_argument(
        "--maxmin",
        action="store_true",
        default=True,
        help="include the Max-Min bar (default on)",
    )
    ap.add_argument("--no-maxmin", dest="maxmin", action="store_false")
    ap.add_argument("--linear", action="store_true", help="linear y (default: log)")
    ap.add_argument("-o", "--out", default="timing")
    ap.add_argument("--label", default=None)
    ap.add_argument("--title", default=None)
    a = ap.parse_args()

    data, meta = load(a.swaplog)
    if not any(k.startswith("wrf_us_e") for k in data):
        sys.exit(
            "no wrf_us_e* timers in %s — rebuild/recapture the gNB with the "
            "epsilon-swept decision timers." % a.swaplog
        )

    # build the bar list: (label, color, mean, p99)
    BLUE, GREEN, RED = "tab:blue", "tab:green", "tab:red"
    bars = []  # (label, color, mean, p99)
    for e in parse_eps(a.wrf_eps):
        v = data.get(f"wrf_us_e{e}")
        if v is not None:
            bars.append((f"WRF\n$\\epsilon$={e}", BLUE, v.mean(), np.percentile(v, 99)))
    for e in parse_eps(a.ggm_eps):
        v = data.get(f"ggm_us_e{e}")
        if v is not None:
            bars.append(
                (f"GGM\n$\\epsilon$={e}", GREEN, v.mean(), np.percentile(v, 99))
            )
    if a.maxmin and data.get("maxmin_us") is not None:
        v = data["maxmin_us"]
        bars.append(("Max-Min", RED, v.mean(), np.percentile(v, 99)))

    labels = [b[0] for b in bars]
    cols = [b[1] for b in bars]
    means = np.array([b[2] for b in bars])
    p99 = np.array([b[3] for b in bars])
    x = np.arange(len(bars))

    log = not a.linear
    floor = max(means.min() / 3.0, 1e-3) if log else 0.0

    fig, ax = plt.subplots(figsize=(max(7.5, 1.0 * len(bars) + 2), 5.6))
    ax.bar(
        x,
        means,
        width=0.66,
        color=cols,
        edgecolor="black",
        linewidth=0.7,
        bottom=(floor if log else 0.0),
        zorder=3,
    )
    # stem from mean up to p99, then the star at p99
    ax.vlines(x, means, np.maximum(p99, means), color="0.35", lw=1.1, zorder=4)
    ax.scatter(
        x,
        p99,
        marker="*",
        s=240,
        color="gold",
        edgecolor="black",
        linewidth=0.7,
        zorder=5,
    )

    # labels: mean inside/under bar top, p99 value above the star
    for xi, mn, p in zip(x, means, p99):
        ax.annotate(
            f"{mn:.2f}",
            (xi, mn),
            ha="center",
            va="top",
            xytext=(0, -3),
            textcoords="offset points",
            fontsize=9,
            color="white",
            fontweight="bold",
            zorder=6,
        )
        ax.annotate(
            f"{p:.2f}",
            (xi, p),
            ha="center",
            va="bottom",
            xytext=(0, 7),
            textcoords="offset points",
            fontsize=8.5,
            color="0.2",
            zorder=6,
        )

    if log:
        ax.set_yscale("log")
        ax.set_ylim(floor, p99.max() * 2.0)
    else:
        ax.set_ylim(0, p99.max() * 1.18)
    ax.set_xticks(x)
    ax.set_xticklabels(labels, fontsize=10)
    ax.set_ylabel("decision time in gNB (µs)", fontsize=13)
    # subtitle: explicit --label, else "<users> users, <rpu> RBG/user" from the log
    if a.label:
        sub = a.label
    elif meta["users"] and meta["rus_per_user"]:
        sub = f"{meta['users']} users, {meta['rus_per_user']} RBG/user"
    else:
        sub = None
    ttl = (
        (a.title + (f"\n{sub}" if sub is not None else ""))
        if a.title is not None
        else "Per algorithm decision cost" + (f"\n{sub}" if sub is not None else "")
    )
    ax.set_title(ttl, fontsize=13)
    # small key for what the bar and star mean
    from matplotlib.lines import Line2D
    from matplotlib.patches import Patch

    ax.legend(
        handles=[
            Patch(facecolor="0.7", edgecolor="black", label="mean"),
            Line2D(
                [0],
                [0],
                marker="*",
                color="none",
                markerfacecolor="gold",
                markeredgecolor="black",
                markersize=13,
                label="99th pct",
            ),
        ],
        fontsize=10,
        loc="upper left",
        framealpha=0.95,
    )
    ax.grid(axis="y", alpha=0.3, which="both", zorder=0)
    fig.tight_layout()
    fig.savefig(a.out + ".png", dpi=150)
    plt.close(fig)

    # text sidecar
    lines = [
        f"decision cost — {a.label or a.swaplog.split('/')[-1]}",
        f"{'method':<16}{'mean_us':>10}{'p99_us':>10}",
        "-" * 36,
    ]
    for lab, _c, mn, p in bars:
        lines.append(f"{lab.replace(chr(10), ' '):<16}{mn:>10.3f}{p:>10.3f}")
    report = "\n".join(lines)
    print(report)
    with open(a.out + ".txt", "w") as f:
        f.write(report + "\n")
    print("wrote", a.out + ".png,", a.out + ".txt")


if __name__ == "__main__":
    main()
