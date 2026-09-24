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
        help="seconds of history kept per RNTI (for the trace)",
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
    # --- mode selection -----------------------------------------------------
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
    a = ap.parse_args()

    # System Constants for 5G NR 20MHz BW @ 30kHz SCS
    FC_GHZ = 3.75
    SCS_GHZ = 30e3 / 1e9  # 30 kHz converted to GHz
    CENTER_SC = 294  # Absolute center DC index of 48 RB grid

    # per-rnti: latest H, its sc axis, and last-seen wallclock
    latest = {}  # rnti -> (sc_array, H_complex_latest)
    lastseen = {}  # rnti -> monotonic time
    fh = None

    cmap = plt.get_cmap("tab10")

    plt.ion()
    fig = plt.figure(figsize=(13, 7))
    fig.suptitle(
        "Live Channel Frequency Response (CFR) |H| (dB) vs Frequency", fontsize=18
    )
    plt.show(block=False)

    def to_freq(sc):
        return FC_GHZ + (sc - CENTER_SC) * SCS_GHZ

    print(
        f"tailing {a.file} — mode: {'OVERLAY (one plot)' if a.overlay else 'STACKED (per-user)'} — close the window to stop"
    )
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

            # drop stale rntis
            active = [r for r in latest if (now - lastseen.get(r, 0)) <= a.stale]
            active.sort()

            fig.clf()
            fig.suptitle(
                "Live Channel Frequency Response (CFR) |H| (dB) vs Frequency",
                fontsize=18,
            )

            if not active:
                ax = fig.add_subplot(111)
                ax.text(
                    0.5,
                    0.5,
                    "waiting for SRS...\n(no RNTI sounded in last "
                    f"{a.stale:.0f}s)\n\nis the gNB running and a UE attached?",
                    ha="center",
                    va="center",
                    fontsize=13,
                    color="gray",
                )
                ax.axis("off")

            elif a.overlay:
                # ---- ONE plot, all users overlaid ----
                ax = fig.add_subplot(111)
                for i, r in enumerate(active):
                    sc, H = latest[r]
                    sort_idx = np.argsort(sc)
                    sc_sorted = sc[sort_idx]
                    H_sorted = H[sort_idx]
                    y = db(H_sorted) + i * a.offset_db
                    age = now - lastseen[r]
                    lbl = f"0x{r:x}  ({age*1000:.0f} ms)"
                    if a.offset_db:
                        lbl += f"  [+{i*a.offset_db:.0f} dB]"
                    ax.plot(to_freq(sc_sorted), y, lw=2, color=cmap(i % 10), label=lbl)
                ax.set_xlabel("Frequency [GHz])", fontsize=14)
                ax.set_ylabel("|H| [dB]", fontsize=14)
                ax.xaxis.set_major_formatter(mticker.FormatStrFormatter("%.4f"))
                ax.tick_params(axis="both", labelsize=12)
                ax.set_xlim(to_freq(sc_sorted[0]), to_freq(sc_sorted[-1]))

                ax.grid(alpha=0.3)
                ax.legend(
                    fontsize=9, loc="upper right", ncol=max(1, (len(active) + 3) // 4)
                )
                ax.set_title(f"Active users — overlaid", fontsize=16)

            else:
                # ---- one panel per user (stacked) : your original view ----
                n = len(active)
                for i, r in enumerate(active):
                    sc, H = latest[r]
                    ax = fig.add_subplot(n, 1, i + 1)

                    sort_idx = np.argsort(sc)
                    sc_sorted = sc[sort_idx]
                    H_sorted = H[sort_idx]

                    ax.plot(to_freq(sc_sorted), db(H_sorted), lw=2, color=cmap(i % 10))

                    age = now - lastseen[r]
                    ax.set_ylabel(f"User: 0x{r:x}\n |H| [dB]", fontsize=14)
                    ax.xaxis.set_major_formatter(mticker.FormatStrFormatter("%.4f"))
                    ax.tick_params(axis="both", labelsize=12)
                    ax.set_xlim(to_freq(sc_sorted[0]), to_freq(sc_sorted[-1]))
                    ax.grid(alpha=0.3)

                    if i == n - 1:
                        ax.set_xlabel("Frequency (GHz)", fontsize=14)

            fig.tight_layout(rect=[0, 0, 1, 0.96])
            fig.canvas.draw_idle()
            fig.canvas.flush_events()
            plt.pause(a.refresh)
    except KeyboardInterrupt:
        pass
    print("stopped.")


if __name__ == "__main__":
    main()
