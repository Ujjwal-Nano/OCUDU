#!/usr/bin/env python3
"""plot_timing.py — decision-cost bar charts (timing counterpart to the outage
comparison). Reads the gNB's on-CPU decision timers from swap.jsonl and draws TWO
plots mirroring compare_algorithms.py:

  PLOT 1  <out>.png            epsilon sweep (same set as outage plot 1)
      WRF {eps=1,2,4}, GGM {eps=1,2,4}, WUF {eps=4}, Optimal
      -> how decision cost grows with epsilon.

  PLOT 2  <out>.all_algos.png  all algorithms (same set as outage plot 2)
      WRF/WUF/GGM {eps=4}, Pref, Bulk, Max-min, Optimal
      -> swap vs the paper baselines, as logged by the gNB.

No No-swap bar (its decision time is not measured). Bar height = MEAN decision
time; a gold star marks the 99th percentile. Log y. Colours match the outage
plot: WRF blue, GGM green, WUF orange, Optimal black, Max-min purple, Pref brown,
Bulk cyan.

Fields read: wrf_us_e{1,2,4}, ggm_us_e{1,2,4}, wuf_us_e{1,2,4}, maxmin_us,
pref_us, bulk_us, opt_alloc_us.

Usage:
  python3 plot_timing.py swap.jsonl -o out/timing
  python3 plot_timing.py swap.jsonl --wrf-eps 1,2,4 --ggm-eps 1,2,4 --label E6_4U -o out/timing
"""

import argparse
import json
import sys

import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
from matplotlib.patches import Patch

# colours matched to compare_algorithms.py (outage plot)
BLUE, GREEN, ORANGE, BLACK = "tab:blue", "tab:green", "tab:orange", "black"
PURPLE, BROWN, CYAN = "tab:purple", "tab:brown", "tab:cyan"   # Max-min / Pref / Bulk

ALL_FIELDS = (
    "maxmin_us", "pref_us", "bulk_us", "opt_alloc_us",
    "wrf_us_e1", "wrf_us_e2", "wrf_us_e4",
    "ggm_us_e1", "ggm_us_e2", "ggm_us_e4",
    "wuf_us_e1", "wuf_us_e2", "wuf_us_e4",
)


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
        for k in ALL_FIELDS:
            v = d.get(k)
            if v is not None and float(v) >= 0:
                acc.setdefault(k, []).append(float(v))
    return {k: np.asarray(v) for k, v in acc.items() if len(v)}, meta


def build_bars(data, spec):
    """spec = list of (label, field, colour); keep only fields that are present."""
    bars = []
    for label, field, colour in spec:
        v = data.get(field)
        if v is not None and len(v):
            bars.append((label, colour, v.mean(), np.percentile(v, 99)))
    return bars


def draw(bars, out_png, title, sub, linear=False):
    if not bars:
        print("  (no timers for", out_png, "- skipped)")
        return None
    labels = [b[0] for b in bars]
    cols = [b[1] for b in bars]
    means = np.array([b[2] for b in bars])
    p99 = np.array([b[3] for b in bars])
    x = np.arange(len(bars))
    log = not linear
    floor = max(means.min() / 3.0, 1e-3) if log else 0.0

    fig, ax = plt.subplots(figsize=(max(7.5, 1.0 * len(bars) + 2), 5.8))
    ax.bar(x, means, width=0.66, color=cols, edgecolor="black", linewidth=0.7,
           bottom=(floor if log else 0.0), zorder=3)
    ax.vlines(x, means, np.maximum(p99, means), color="0.35", lw=1.1, zorder=4)
    ax.scatter(x, p99, marker="*", s=240, color="gold", edgecolor="black", linewidth=0.7, zorder=5)
    for xi, mn, p in zip(x, means, p99):
        ax.annotate(f"{mn:.2f}", (xi, mn), ha="center", va="top", xytext=(0, -3),
                    textcoords="offset points", fontsize=9, color="white", fontweight="bold", zorder=6)
        ax.annotate(f"{p:.2f}", (xi, p), ha="center", va="bottom", xytext=(0, 7),
                    textcoords="offset points", fontsize=8.5, color="0.2", zorder=6)
    if log:
        ax.set_yscale("log"); ax.set_ylim(floor, p99.max() * 2.0)
    else:
        ax.set_ylim(0, p99.max() * 1.18)
    ax.set_xticks(x); ax.set_xticklabels(labels, fontsize=10)
    ax.set_ylabel("decision time in gNB (µs)", fontsize=13)
    ax.set_title(title + (f"\n{sub}" if sub else ""), fontsize=13)
    ax.legend(handles=[Patch(facecolor="0.7", edgecolor="black", label="mean"),
                       Line2D([0], [0], marker="*", color="none", markerfacecolor="gold",
                              markeredgecolor="black", markersize=13, label="99th pct")],
              fontsize=10, loc="upper left", framealpha=0.95)
    ax.grid(axis="y", alpha=0.3, which="both", zorder=0)
    fig.tight_layout()
    fig.savefig(out_png, dpi=150)
    fig.savefig(out_png.rsplit(".", 1)[0] + ".pdf", bbox_inches="tight")
    plt.close(fig)
    return bars


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("swaplog")
    ap.add_argument("--wrf-eps", default="1,2,4")
    ap.add_argument("--ggm-eps", default="1,2,4")
    ap.add_argument("--wuf-eps", default="4", help="epsilons for WUF in the SWEEP plot (default 4)")
    ap.add_argument("--linear", action="store_true", help="linear y (default: log)")
    ap.add_argument("-o", "--out", default="timing")
    ap.add_argument("--label", default=None)
    a = ap.parse_args()

    data, meta = load(a.swaplog)
    if not any(k.startswith("wrf_us_e") for k in data):
        sys.exit("no wrf_us_e* timers in %s — rebuild/recapture the gNB with the "
                 "epsilon-swept decision timers." % a.swaplog)

    sub = a.label or (f"{meta['users']} users, {meta['rus_per_user']} RBG/user"
                      if meta["users"] and meta["rus_per_user"] else None)

    # ---- PLOT 1: epsilon sweep ----
    spec1 = [(f"WRF\n$\\epsilon$={e}", f"wrf_us_e{e}", BLUE) for e in parse_eps(a.wrf_eps)]
    spec1 += [(f"GGM\n$\\epsilon$={e}", f"ggm_us_e{e}", GREEN) for e in parse_eps(a.ggm_eps)]
    spec1 += [(f"WUF\n$\\epsilon$={e}", f"wuf_us_e{e}", ORANGE) for e in parse_eps(a.wuf_eps)]
    spec1 += [("Optimal", "opt_alloc_us", BLACK)]
    bars1 = draw(build_bars(data, spec1), a.out + ".png",
                 "Per-algorithm decision cost  ($\\epsilon$ sweep)", sub, a.linear)

    # ---- PLOT 2: all algorithms at eps=4 + baselines ----
    spec2 = [("WRF\n$\\epsilon$=4", "wrf_us_e4", BLUE),
             ("WUF\n$\\epsilon$=4", "wuf_us_e4", ORANGE),
             ("GGM\n$\\epsilon$=4", "ggm_us_e4", GREEN),
             ("Pref", "pref_us", BROWN),
             ("Bulk", "bulk_us", CYAN),
             ("Max-min", "maxmin_us", PURPLE),
             ("Optimal", "opt_alloc_us", BLACK)]
    bars2 = draw(build_bars(data, spec2), a.out + ".all_algos.png",
                 "Per-algorithm decision cost  (all algorithms, $\\epsilon$=4)", sub, a.linear)

    # ---- text sidecar ----
    lines = [f"decision cost — {a.label or a.swaplog.split('/')[-1]}",
             f"{'method':<16}{'mean_us':>10}{'p99_us':>10}", "-" * 36]
    for tag, bars in (("[sweep]", bars1), ("[all algos]", bars2)):
        if not bars:
            continue
        lines.append(tag)
        for lab, _c, mn, p in bars:
            lines.append(f"{lab.replace(chr(10), ' '):<16}{mn:>10.3f}{p:>10.3f}")
    report = "\n".join(lines)
    print(report)
    with open(a.out + ".txt", "w") as f:
        f.write(report + "\n")
    print("wrote", a.out + ".png,", a.out + ".all_algos.png,", a.out + ".txt")


if __name__ == "__main__":
    main()