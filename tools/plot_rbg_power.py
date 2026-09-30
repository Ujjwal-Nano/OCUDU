#!/usr/bin/env python3
"""
plot_rbg_power.py — RBG power distribution & swapping-benefit figure.
3 panels:
  1. Per-RBG power distribution (box plot over the whole capture)
  2. Best-RBG fraction (what % of time each RBG is strongest) — "fair distribution"
  3. Gain-from-swapping distribution (best - average power, dB) — swapping benefit
Modular in K (RBs per RBG).
Usage:
  python3 plot_rbg_power.py CAP.rb.jsonl[.gz] [-o out.png] [--K 12]
    [--trim-start 1.0] [--trim-end 1.0] [--reattach-guard 10]
"""

import argparse, gzip, json, sys
import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

RBBW = 0.36e6

def load_rnti_to_imsi(sessions_path):
    with open(sessions_path) as f:
        grouped = json.load(f)
    if isinstance(grouped, dict) and isinstance(grouped.get("users"), dict):
        grouped = grouped["users"]
    m = {}
    for imsi, sessions in grouped.items():
        if imsi == "UNKNOWN_IMSI":
            continue
        for s in (sessions if isinstance(sessions, list) else []):
            rh = s.get("rnti") if isinstance(s, dict) else None
            if not rh:
                continue
            try:
                r = int(rh, 16) if isinstance(rh, str) and rh.lower().startswith("0x") \
                    else int(rh)
            except Exception:
                continue
            m[r] = imsi
    return m



def load(path, trim_start=0.0, trim_end=0.0):
    op = gzip.open if path.endswith(".gz") else open
    T, P, RN = [], [], []
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
            RN.append(d.get("rnti", -1))
    if not P:
        sys.exit("no samples in " + path)
    T = np.array(T, float)
    P = np.array(P, float)
    RN = np.array(RN)
    P = P[:, 1:]
    mins = (T - T[0]) / 60000.0
    k = (mins >= trim_start) & (mins <= mins[-1] - trim_end)
    if k.sum() < 100:
        k = np.ones(len(mins), bool)
    return T, P, RN


def to_ru_db(P, K):
    R = P.shape[1] // K
    return 10 * np.log10(
        np.maximum(
            np.stack([P[:, r * K : (r + 1) * K].sum(1) for r in range(R)], 1), 1e-15
        )
    )


def to_ru_lin(P, K):
    R = P.shape[1] // K
    return np.maximum(
        np.stack([P[:, r * K : (r + 1) * K].sum(1) for r in range(R)], 1), 1e-15
    )


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cap")
    ap.add_argument("-o", "--out", default="rbg_power.png")
    ap.add_argument("--K", type=int, default=12)
    ap.add_argument("--trim-start", type=float, default=1.0)
    ap.add_argument("--trim-end", type=float, default=1.0)
    ap.add_argument("--reattach-guard", type=float, default=10.0)
    ap.add_argument("--label", default=None)
    ap.add_argument("--sessions", default=None,
                    help="ue_sessions.json: split the 3 panels PER user (IMSI). "
                         "Without it, all records are pooled (single-user only).")
    a = ap.parse_args()
    T, P_all, RN = load(a.cap)

    rnti2imsi = load_rnti_to_imsi(a.sessions) if a.sessions else None
    if rnti2imsi:
        ukeys = np.array([rnti2imsi.get(int(r), None) for r in RN], dtype=object)
        users = [u for u in sorted(set(ukeys.tolist()), key=lambda x:(x is None,str(x)))
                 if u is not None]
        if not users:
            sys.exit("no records resolved to a user via --sessions")
    else:
        ukeys = np.array(["all"] * len(RN), dtype=object)
        users = ["all"]

    base_label = a.label if a.label else a.cap.split("/")[-1]

    def process_one(P, ukey, ax_row, R_ref):
        # per-user trim by time
        T_u = T[ukeys == ukey] if False else None  # (time already implicit in P order)
        Plin = to_ru_lin(P, a.K)
        M = 10 * np.log10(Plin)
        R = M.shape[1]
        best = M.argmax(1)
        best_frac = np.array([100 * (best == r).mean() for r in range(R)])
        return M, R, best_frac, gain

    # one row of 3 panels per user
    nU = len(users)
    fig, axes = plt.subplots(nU, 2, figsize=(12, 4.6 * nU), squeeze=False)
    summary_lines = []
    for row, ukey in enumerate(users):
        mask = ukeys == ukey
        P = P_all[mask]
        RNu = RN[mask]
        # per-user reattach guard (RNTI changes WITHIN this user = reconnections)
        if a.reattach_guard > 0 and len(RNu) > 1:
            fs_u = 100.0
            g = int(round(a.reattach_guard * fs_u))
            keep = np.ones(len(RNu), bool)
            for c in [i for i in range(1, len(RNu)) if RNu[i] != RNu[i-1]]:
                keep[max(0, c-g):min(len(RNu), c+g)] = False
            if keep.sum() > 100:
                P = P[keep]
        if P.shape[0] < 10:
            continue
        Plin = to_ru_lin(P, a.K)
        M = 10 * np.log10(Plin)
        R = M.shape[1]
        best = M.argmax(1)
        best_frac = np.array([100 * (best == r).mean() for r in range(R)])
        label = f"{ukey}"

        ax = axes[row]
        # Panel 1
        ax[0].boxplot([M[:, r] for r in range(R)],
                      labels=[f"RBG{r}" for r in range(R)], showfliers=False,
                      patch_artist=True, boxprops=dict(facecolor="#8fb3e0"),
                      medianprops=dict(color="k"))
        ax[0].set_ylabel("RBG power (dB)")
        ax[0].set_title(f"user {label} — per-RBG power (K={a.K})")
        ax[0].grid(alpha=0.3, axis="y")
        # Panel 2 — the mobile-vs-static tell
        bars = ax[1].bar([f"RBG{r}" for r in range(R)], best_frac,
                         color="#d62728", alpha=0.8)
        for b, fr in zip(bars, best_frac):
            ax[1].text(b.get_x()+b.get_width()/2, fr+0.5, f"{fr:.0f}%",
                       ha="center", fontsize=9)
        ax[1].axhline(100/R, ls="--", c="k", lw=1, label=f"uniform ({100/R:.0f}%)")
        spread = best_frac.max() - best_frac.min()
        tag = "STATIC-like (one dominant RBG)" if best_frac.max() > 100/R*1.8 \
              else "MOBILE-like (spread)"
        ax[1].set_ylabel("% of time best")
        ax[1].set_title(f"best-RBG fraction — {tag}")
        ax[1].set_ylim(0, max(best_frac.max()*1.2, 100/R*1.5))
        ax[1].grid(alpha=0.3, axis="y"); ax[1].legend(fontsize=8)
        summary_lines.append(
            f"user {label}: best-RBG frac " +
            ", ".join(f"RBG{r}={best_frac[r]:.0f}%" for r in range(R)) +
            f"  | spread={spread:.0f}pts ({tag})")

    fig.suptitle(f"RBG power & mobility signature — {base_label}", fontsize=13)
    fig.tight_layout(rect=[0, 0, 1, 0.97])
    fig.savefig(a.out, dpi=120)
    txt = a.out.rsplit(".",1)[0] + "_power.txt"
    open(txt,"w").write("\n".join(summary_lines) + "\n")
    print("\n".join(summary_lines))
    print("wrote", a.out, "and", txt)
    return


if __name__ == "__main__":
    main()