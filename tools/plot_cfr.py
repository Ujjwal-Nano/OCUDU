#!/usr/bin/env python3
"""plot_cfr.py — per-user per-subcarrier CFR from /tmp/srs_cfr.jsonl.

Schema (one line per SRS occasion, per rx port), written by the translator:
  {"t":<ms>,"rnti":<int>,"rx":0,"sc":[...],"re":[...],"im":[...]}
where sc are the SOUNDED subcarrier indices (comb-4 => every 4th),
re/im the TA-compensated complex CFR H(sc).

Produces, per user (rnti):
  L: |H| in dB heatmap over time  (subcarrier on y, time on x)
  R: latest |H(f)| in dB and unwrapped phase vs subcarrier

Caveats baked into the labels:
  - |H| (magnitude) is clean and directly usable.
  - phase is TA-compensated and, with a disciplined clock (Meinberg), stable
    WITHIN a capture — but NOT reciprocity-calibrated (RF-chain phase offset
    remains). Treat phase as relative, not absolute.
  - comb-4: only 1 subcarrier in 4 is sounded; gaps are unmeasured, not zero.

Usage:
  python3 plot_cfr.py /tmp/srs_cfr.jsonl -o cfr.png [--rnti 0x4609] [--last-n 400]
"""
import argparse, json, sys
from collections import defaultdict
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

EPS = 1e-15

def _isint(k):
    return isinstance(k, int)

def _lab(k):
    """Label a user key: IMSI string as-is, int rnti as hex."""
    return f"0x{k:x}" if isinstance(k, int) else str(k)

def load_rnti_to_imsi(sessions_path):
    """Flatten ue_sessions.json ({imsi: [{rnti:"0x..",...},...]}) into a plain
    int(rnti) -> imsi lookup, matching rb_to_ru.py's behavior. Later session
    wins on the rare rnti-reuse collision (warned)."""
    with open(sessions_path) as fh:
        grouped = json.load(fh)
    m = {}
    collisions = set()
    for imsi, sessions in grouped.items():
        if imsi == "UNKNOWN_IMSI":
            continue
        for s in sessions:
            rh = s.get("rnti")
            if not rh:
                continue
            r = int(rh, 16) if isinstance(rh, str) and rh.lower().startswith("0x") \
                else (int(rh, 16) if isinstance(rh, str) else int(rh))
            if r in m and m[r] != imsi:
                collisions.add(r)
            m[r] = imsi
    if collisions:
        print("WARNING: rnti(s) reused across IMSIs: "
              + ", ".join(hex(r) for r in sorted(collisions))
              + " -- later session wins", file=sys.stderr)
    return m


def load(path, rnti2imsi=None):
    users = defaultdict(lambda: {"t": [], "sc": None, "H": []})
    with open(path) as f:
        for ln in f:
            ln = ln.strip()
            if not ln:
                continue
            try:
                d = json.loads(ln)
            except json.JSONDecodeError:
                continue
            r = d.get("rnti")
            if rnti2imsi is not None and r is not None:
                r = rnti2imsi.get(int(r), f"rnti-0x{int(r):x}")  # unmatched -> keep rnti tag
            sc = d.get("sc"); re = d.get("re"); im = d.get("im")
            if r is None or sc is None or re is None or im is None:
                continue
            if len(sc) != len(re) or len(re) != len(im) or not sc:
                continue
            H = np.array(re, float) + 1j*np.array(im, float)
            u = users[r]
            # sc set can shift if bandwidth/comb offset changes mid-run; keep the
            # modal length, skip odd-length lines so the heatmap stays rectangular.
            if u["sc"] is None:
                u["sc"] = np.array(sc)
            if len(sc) != len(u["sc"]):
                continue
            u["t"].append(d.get("t"))
            u["H"].append(H)
    return users

def to_db(x):
    return 20.0*np.log10(np.maximum(np.abs(x), EPS))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("jsonl")
    ap.add_argument("-o", "--out", default="cfr.png")
    ap.add_argument("--rnti", default=None,
                    help="only this rnti (int or 0x..)")
    ap.add_argument("--last-n", type=int, default=600,
                    help="most recent N occasions in the heatmap")
    ap.add_argument("--sessions", default=None,
                    help="ue_sessions.json from correlate_rnti_imsi.py --group-out; "
                         "merges each user's re-attach RNTIs under its IMSI")
    a = ap.parse_args()

    rnti2imsi = load_rnti_to_imsi(a.sessions) if a.sessions else None
    users = load(a.jsonl, rnti2imsi)
    if not users:
        sys.exit("no usable CFR lines in " + a.jsonl +
                 " (did the gNB rebuild with the CFR tap? is the file non-empty?)")

    if a.rnti is not None:
        want = a.rnti  # match against label string form
        users = {k: v for k, v in users.items() if _lab(k) == want or (_isint(k) and (hex(k) == want.lower() or str(k) == want))}
        if not users:
            sys.exit(f"user {a.rnti} not found; present: " + ", ".join(_lab(k) for k in load(a.jsonl, rnti2imsi)))

    order = sorted(users.keys(), key=lambda k: (isinstance(k, str), k))
    n = len(order)
    fig, axes = plt.subplots(n, 2, figsize=(16, 4.2*n), squeeze=False)
    fig.suptitle("Per-subcarrier CFR — |H| clean; phase stable-in-capture, "
                 "NOT reciprocity-calibrated", y=0.997, fontsize=12)

    for row, r in enumerate(order):
        u = users[r]
        sc = u["sc"]
        H = np.array(u["H"])                       # [time, subcarrier]
        if H.ndim != 2 or H.shape[0] == 0:
            continue
        Hlast = H[-a.last_n:]
        Mdb = to_db(Hlast)

        # time axis
        t = np.array(u["t"][-a.last_n:], float)
        if np.all(np.isfinite(t)) and t.size > 1:
            tt = (t - t[0]) / 1000.0
            xlab = "time (s, last window)"
        else:
            tt = np.arange(Hlast.shape[0]); xlab = "occasion idx"

        # L: |H| heatmap over time
        axL = axes[row, 0]
        im0 = axL.imshow(Mdb.T, aspect="auto", origin="lower",
                         extent=[tt[0], tt[-1], sc[0], sc[-1]],
                         cmap="viridis")
        axL.set_title(f"user {_lab(r)} — |H| (dB) over time  "
                      f"[{H.shape[0]} occasions, {sc.size} sounded sc]")
        axL.set_xlabel(xlab); axL.set_ylabel("subcarrier")
        fig.colorbar(im0, ax=axL, label="|H| dB")

        # R: latest magnitude + unwrapped phase vs subcarrier
        axR = axes[row, 1]
        mag = to_db(H[-1])
        axR.plot(sc, mag, marker=".", ms=3, lw=1, color="tab:blue", label="|H| dB")
        axR.set_xlabel("subcarrier"); axR.set_ylabel("|H| dB", color="tab:blue")
        axR.tick_params(axis="y", labelcolor="tab:blue")
        axR.grid(alpha=0.3)
        axP = axR.twinx()
        ph = np.unwrap(np.angle(H[-1]))
        axP.plot(sc, ph, marker=".", ms=3, lw=1, color="tab:red", alpha=0.7,
                 label="phase (rel)")
        axP.set_ylabel("phase [rad] (uncalibrated)", color="tab:red")
        axP.tick_params(axis="y", labelcolor="tab:red")
        axR.set_title(f"user {_lab(r)} — latest snapshot  (RB spans {sc[0]//12}-{sc[-1]//12})")

    fig.tight_layout(rect=[0, 0, 1, 0.97])
    fig.savefig(a.out, dpi=140)
    print("users:", ", ".join(_lab(k) for k in order))
    for r in order:
        print(f"  {_lab(r)}: {len(users[r]['H'])} occasions, "
              f"{users[r]['sc'].size} sounded subcarriers")
    print("wrote", a.out)

if __name__ == "__main__":
    main()