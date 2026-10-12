#!/usr/bin/env python3


import argparse, json, os, time, collections
import numpy as np
import matplotlib
import matplotlib.ticker as mticker  # FIX: ticker must be imported explicitly

# Interactive backend: on the gNB PC's own screen this shows a live window.
import matplotlib.pyplot as plt

EPS = 1e-15


def db(x):
    return 20.0 * np.log10(np.maximum(np.abs(x), EPS))


def wideband_db(H):
    """Wideband CFR power: mean over subcarriers of |H|^2, in dB."""
    p = np.mean(np.abs(np.asarray(H)) ** 2)
    return 10.0 * np.log10(max(p, EPS))


def tail_lines(path, fh):

    out = []
    try:
        size = os.path.getsize(path)
    except OSError:
        return out, fh
    if fh is None:
        fh = open(path, "r")
        fh.seek(0, os.SEEK_END)  # start at end: only NEW data
        return out, fh
    # detect truncation / recreation
    if size < fh.tell():
        try:
            fh.close()
        except Exception:
            pass
        fh = open(path, "r")
        fh.seek(0, os.SEEK_END)
        return out, fh
    while True:
        line = fh.readline()
        if not line:
            break
        line = line.strip()
        if line:
            out.append(line)
    return out, fh


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--file", default="/tmp/srs_cfr.jsonl")
    ap.add_argument(
        "--window",
        type=float,
        default=5.0,
        help="seconds of history kept per RNTI (wideband-power trace length)",
    )
    ap.add_argument(
        "--stale",
        type=float,
        default=3.0,
        help="drop an RNTI from the view after this many s of silence",
    )
    ap.add_argument(
        "--refresh", type=float, default=0.4, help="redraw period in seconds"
    )
    ap.add_argument(
        "--overlay",
        action="store_true",
        help="ALL users on ONE plot (compare). Default (omit) = one panel per user (stacked).",
    )
    ap.add_argument(
        "--offset-db",
        type=float,
        default=0.0,
        help="in --overlay: shift each user vertically by this many dB so overlapping "
        "traces separate (e.g. 15). 0 = true overlay, absolute dB.",
    )
    ap.add_argument(
        "--ymin", type=float, default=-80.0, help="overlay |H| y-axis minimum (dB)"
    )
    ap.add_argument(
        "--ymax", type=float, default=-20.0, help="overlay |H| y-axis maximum (dB)"
    )
    a = ap.parse_args()

    # System Constants for 5G NR 20MHz BW @ 30kHz SCS
    FC_GHZ = 3.75
    SCS_GHZ = 30e3 / 1e9  # 30 kHz converted to GHz
    CENTER_SC = 294  # Absolute center DC index of 48 RB grid

    latest = {}  # rnti -> (sc_array, H_complex_latest)
    lastseen = {}  # rnti -> monotonic time
    # wideband power history per rnti: deque of (t_monotonic, wideband_dB)
    wb_hist = collections.defaultdict(lambda: collections.deque(maxlen=4000))
    fh = None

    cmap = plt.get_cmap("tab10")

    plt.ion()
    fig = plt.figure(figsize=(16, 9))
    plt.show(block=False)

    def to_freq(sc):
        return FC_GHZ + (sc - CENTER_SC) * SCS_GHZ

    try:
        while plt.fignum_exists(fig.number):
            lines, fh = tail_lines(a.file, fh)
            now = time.monotonic()
            for ln in lines:
                try:
                    d = json.loads(ln)
                except json.JSONDecodeError:
                    continue
                r = d.get("rnti")
                sc = d.get("sc")
                re = d.get("re")
                im = d.get("im")
                if r is None or not sc or re is None or im is None:
                    continue
                if len(sc) != len(re) or len(re) != len(im):
                    continue
                H = np.array(re, float) + 1j * np.array(im, float)
                latest[r] = (np.array(sc), H)
                lastseen[r] = now
                wb_hist[r].append((now, wideband_db(H)))

            active = [r for r in latest if (now - lastseen.get(r, 0)) <= a.stale]
            active.sort()

            fig.clf()
            fig.suptitle("Live Channel Frequency Response (CFR)", fontsize=32)

            if not active:
                ax = fig.add_subplot(111)
                ax.text(
                    0.5,
                    0.5,
                    "waiting for SRS ...",
                    ha="center",
                    va="center",
                    fontsize=22,
                    color="gray",
                )
                ax.axis("off")
                fig.canvas.draw_idle()
                fig.canvas.flush_events()
                plt.pause(a.refresh)
                continue

            # ============ TOP: |H| vs frequency ============
            if a.overlay:
                axT = fig.add_subplot(2, 1, 1)
                last_sc_sorted = None
                for i, r in enumerate(active):
                    sc, H = latest[r]
                    idx = np.argsort(sc)
                    sc_s = sc[idx]
                    last_sc_sorted = sc_s
                    y = db(H[idx]) + i * a.offset_db
                    age = now - lastseen[r]
                    lbl = f"0x{r:x}  ({age*1000:.0f} ms)"
                    if a.offset_db:
                        lbl += f"  [+{i*a.offset_db:.0f} dB]"
                    axT.plot(to_freq(sc_s), y, lw=4, color=cmap(i % 10), label=lbl)
                axT.set_xlabel("Frequency [GHz]", fontsize=24)
                axT.set_ylabel("|H| [dB]", fontsize=24)
                axT.xaxis.set_major_formatter(mticker.FormatStrFormatter("%.4f"))
                axT.tick_params(axis="both", labelsize=20)
                if last_sc_sorted is not None:
                    axT.set_xlim(
                        to_freq(last_sc_sorted[0]), to_freq(last_sc_sorted[-1])
                    )
                if not a.offset_db:
                    axT.set_ylim(a.ymin, a.ymax)  # FIXED y-axis in overlay
                axT.grid(alpha=0.3)
                axT.legend(
                    fontsize=18, loc="upper right", ncol=max(1, (len(active) + 3) // 4)
                )
                axT.set_title("CFR for Active Users", fontsize=26)
            else:
                # stacked |H| as a sub-grid in the top half (one row per user)
                nU = len(active)
                for i, r in enumerate(active):
                    sc, H = latest[r]
                    axT = fig.add_subplot(2 * nU, 1, i + 1)
                    idx = np.argsort(sc)
                    sc_s = sc[idx]
                    axT.plot(to_freq(sc_s), db(H[idx]), lw=4, color=cmap(i % 10))
                    axT.set_ylabel(f"0x{r:x}\n|H|[dB]", fontsize=22)
                    axT.xaxis.set_major_formatter(mticker.FormatStrFormatter("%.4f"))
                    axT.tick_params(axis="both", labelsize=20)
                    axT.set_xlim(to_freq(sc_s[0]), to_freq(sc_s[-1]))
                    axT.grid(alpha=0.3)
                    if i == 0:
                        axT.set_title("|H| vs frequency (per user)", fontsize=26)

            # ============ BOTTOM: wideband power vs time ============
            axB = fig.add_subplot(2, 1, 2)
            for i, r in enumerate(active):
                hist = [p for p in wb_hist[r] if now - p[0] <= a.window]
                if len(hist) < 2:
                    continue
                ts = np.array([p[0] for p in hist])
                ys = np.array([p[1] for p in hist])
                tx = ts - now  # seconds relative to 'now' (negative = past)
                axB.plot(
                    tx,
                    ys,
                    lw=4,
                    color=cmap(i % 10),
                    label=f"0x{r:x}",
                )
            axB.set_xlabel("time [s]  (0 = current)", fontsize=24)
            axB.set_ylabel("wideband power [dB]", fontsize=24)
            axB.tick_params(axis="both", labelsize=20)
            axB.set_xlim(-a.window + 1, 0)
            axB.set_ylim(-80, -20)
            axB.grid(alpha=0.3)
            axB.legend(
                fontsize=18, loc="upper right", ncol=max(1, (len(active) + 3) // 4)
            )
            axB.set_title(
                "Wideband Channel Power over the last " f"{a.window-1:.0f} s",
                fontsize=26,
            )

            fig.subplots_adjust(
                left=0.07, right=0.985, top=0.90, bottom=0.075, hspace=0.38
            )
            fig.canvas.draw_idle()
            fig.canvas.flush_events()
            plt.pause(a.refresh)
    except KeyboardInterrupt:
        pass
    print("stopped.")


if __name__ == "__main__":
    main()
