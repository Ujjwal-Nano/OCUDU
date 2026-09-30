#!/usr/bin/env python3
"""plot_weakest.py — show the swap RAISING THE FLOOR (weakest user) over time.

The weakest user changes slot-by-slot (e.g. a moving UE that is far sometimes,
close other times). So at EACH swap decision we take whoever is weakest right
then, under two policies, and plot both over the real experiment time:

  "no swap" = min over users of (their power on their FIXED RU)
              -> the worst-off user's power if nobody swapped
  "swap"    = min over users of (served)  == weakest_user_csi in the log
              -> the worst-off user's power after the swap reassigned RUs

The swap line sitting ABOVE the no-swap line = the swap raising the floor
(max-min fairness), following whoever is currently weakest -- including the
moving user when it is far.

Both come straight from swap_metrics.jsonl (each line has every user's full
per-RU csi + assigned rus). x-axis is real time; swap is uplink-buffer gated,
so sampling is irregular and the gaps are real.

Usage:
  python3 plot_weakest.py /tmp/swap_metrics.jsonl -o weakest.png [--snr0 10] [--smooth 50]
"""
import argparse, json, sys
from collections import Counter, defaultdict
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

EPS = 1e-12

def load_rnti_to_imsi(sessions_path):
    with open(sessions_path) as f:
        g = json.load(f)
    if isinstance(g, dict) and isinstance(g.get("users"), dict):
        g = g["users"]
    m = {}
    for imsi, sess in g.items():
        if imsi == "UNKNOWN_IMSI":
            continue
        for s in (sess if isinstance(sess, list) else []):
            rh = s.get("rnti") if isinstance(s, dict) else None
            if not rh:
                continue
            try:
                r = int(rh, 16) if isinstance(rh, str) and rh.lower().startswith("0x") else int(rh)
            except Exception:
                continue
            m[r] = imsi
    return m



def load(path):
    slots = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                d = json.loads(line)
            except json.JSONDecodeError:
                continue
            us = d.get("users", [])
            if not us:
                continue
            rec = {}
            for u in us:
                csi = u.get("csi")
                if not csi or max(csi) <= 1e-8:
                    continue
                rec[u.get("u")] = {"csi": np.asarray(csi, float),
                                   "rus": list(u.get("rus", [])),
                                   "served": float(u.get("served", 0.0)),
                                   "rnti": u.get("rnti")}
            if len(rec) >= 1:
                slots.append({"t": d.get("t"), "users": rec})
    return slots


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("swaplog")
    ap.add_argument("-o", "--out", default="weakest.png")
    ap.add_argument("--smooth", type=int, default=50)
    ap.add_argument("--fixed-ru", default="mode", choices=["mode", "first"],
                    help="each user's no-swap RU: most-frequent ('mode') or initial ('first')")
    ap.add_argument("--sessions", default=None,
                    help="ue_sessions.json: relabel the swap log's per-user "
                         "index/rnti to real IMSI in the weakest-share output")
    ap.add_argument("--min-users", type=int, default=2,
                    help="only use slots with at least this many real users")
    a = ap.parse_args()

    slots = load(a.swaplog)
    if not slots:
        sys.exit("no usable slots in " + a.swaplog)

    r2i = load_rnti_to_imsi(a.sessions) if a.sessions else {}
    # map each internal user id -> IMSI, via the rnti seen for that id
    uid2imsi = {}
    if r2i:
        for s in slots:
            for uid, u in s["users"].items():
                if uid not in uid2imsi and u.get("rnti") is not None:
                    imsi = r2i.get(int(u["rnti"]))
                    if imsi:
                        uid2imsi[uid] = imsi
    def lab(uid):
        return uid2imsi.get(uid, str(uid))

    # each user's FIXED (no-swap) RU: most-frequent assigned RU across the run
    ru_hist = defaultdict(Counter); first_ru = {}
    for s in slots:
        for uid, u in s["users"].items():
            if u["rus"]:
                ru_hist[uid][u["rus"][0]] += 1
                first_ru.setdefault(uid, u["rus"][0])
    fixed_ru = {uid: (first_ru[uid] if a.fixed_ru == "first"
                      else h.most_common(1)[0][0]) for uid, h in ru_hist.items()}

    # per slot: weakest-user power under no-swap and under swap
    T, w_noswap, w_swap = [], [], []
    who_noswap, who_swap = [], []
    for s in slots:
        us = s["users"]
        if len(us) < a.min_users:
            continue
        R = len(next(iter(us.values()))["csi"])
        # no-swap: each user on its fixed RU -> floor
        ns = {}
        for uid, u in us.items():
            fr = fixed_ru.get(uid, 0); fr = fr if fr < R else 0
            ns[uid] = u["csi"][fr]
        # swap: each user's served (power on assigned RU) -> floor
        sw = {uid: (u["served"] if u["served"] > 0 else
                    (u["csi"][u["rus"][0]] if u["rus"] else 0.0))
              for uid, u in us.items()}
        ns_uid = min(ns, key=ns.get); sw_uid = min(sw, key=sw.get)
        T.append(s["t"])
        w_noswap.append(ns[ns_uid]); w_swap.append(sw[sw_uid])
        who_noswap.append(ns_uid); who_swap.append(sw_uid)

    if not T:
        sys.exit("no slots with >= %d users" % a.min_users)

    T = np.array(T, float)
    to_db = lambda x: 10 * np.log10(np.maximum(np.asarray(x, float), EPS))
    y_ns, y_sw = to_db(w_noswap), to_db(w_swap)
    if np.all(np.isfinite(T)) and T.size > 1:
        x = (T - T[0]) / 1000.0; xlabel = "time (s, experiment)"
    else:
        x = np.arange(len(T)); xlabel = "swap decision index"

    def sm(v, w):
        v = np.asarray(v, float)
        return v if (w <= 1 or len(v) < w) else np.convolve(v, np.ones(w)/w, mode="same")

    fig, ax = plt.subplots(figsize=(15, 5.5))
    ax.plot(x, y_ns, color="tab:red", alpha=0.15, lw=0.5)
    ax.plot(x, y_sw, color="tab:blue", alpha=0.15, lw=0.5)
    ax.plot(x, sm(y_ns, a.smooth), color="tab:red", lw=1.6,
            label="no swap  (weakest user on fixed RUs)")
    ax.plot(x, sm(y_sw, a.smooth), color="tab:blue", lw=1.8,
            label="swap  (weakest user after reassignment)")
    lift = float(np.mean(y_sw) - np.mean(y_ns))
    ax.set_xlabel(xlabel); ax.set_ylabel("weakest-user (floor) power (dB)")
    ax.set_title(f"Swap raises the floor — weakest-user lift = {lift:+.2f} dB "
                 f"(weakest user tracked per slot)")
    ax.grid(alpha=0.3); ax.legend(loc="upper right")
    fig.tight_layout(); fig.savefig(a.out, dpi=140); plt.close(fig)

    # who is weakest, how often (shows the moving user taking the floor)
    cnt = Counter(who_swap)
    print(f"slots used: {len(T)}   mean floor  no-swap={np.mean(y_ns):.2f} dB  "
          f"swap={np.mean(y_sw):.2f} dB  lift={lift:+.2f} dB")
    print("weakest-user (under swap) share of slots:")
    for uid, c in cnt.most_common():
        print(f"   {lab(uid)}: {100*c/len(who_swap):.0f}%")
    print("wrote", a.out)


if __name__ == "__main__":
    main()