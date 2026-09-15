#!/usr/bin/env python3
"""rb_to_ru.py — aggregate a per-RB SRS log into the per-RU jsonl schema,
resolving each record's rnti to a stable per-user id.

Usage:
  python3 rb_to_ru.py /tmp/srs_rb.jsonl -o out.jsonl [--rbs-per-ru 12] [--skip-rb0]
  python3 rb_to_ru.py /tmp/srs_rb.jsonl -o out.jsonl --sessions ue_sessions.json

Output lines match swap_metrics format, so plot_csi.py / analyze_position.py
work as-is.

Modular by design: with --sessions, the "u" field is set to the real IMSI
instead of a hardcoded 0. plot_csi.py already groups its plots by "u", one
row per distinct value, and sizes the figure to len(users) automatically —
so N real users just becomes N rows with zero code changes anywhere
downstream as more UEs join a capture. Without --sessions, everything still
collapses onto "u": 0, same as the old behavior (useful for quick
single-UE tests where running the correlation step isn't worth it).
"""
import argparse, json, sys

def load_rnti_to_imsi(sessions_path):
    """Flatten ue_sessions.json (imsi -> [{rnti, start, end, ...}, ...]) into
    a plain rnti(int) -> imsi lookup. Within one capture window the gNB's
    rnti values are effectively unique per connection instance, so a direct
    lookup (ignoring session start/end) is enough — no need to re-parse
    timestamps here. If the same rnti value genuinely got reused by two
    different IMSIs in this window (rare, only possible on very long
    captures with heavy rnti churn), the later session in the JSON wins and
    a warning is printed so you notice rather than silently mis-attribute
    data."""
    with open(sessions_path) as f:
        grouped = json.load(f)
    rnti_to_imsi = {}
    collisions = set()
    for imsi, sessions in grouped.items():
        if imsi == "UNKNOWN_IMSI":
            continue
        for s in sessions:
            rnti_hex = s.get("rnti")
            if not rnti_hex:
                continue
            rnti = int(rnti_hex, 16)
            if rnti in rnti_to_imsi and rnti_to_imsi[rnti] != imsi:
                collisions.add(rnti)
            rnti_to_imsi[rnti] = imsi
    if collisions:
        print(f"WARNING: rnti(s) reused across more than one IMSI in "
              f"{sessions_path}: {', '.join(hex(r) for r in sorted(collisions))} "
              f"-- later session in the file wins for those lines",
              file=sys.stderr)
    return rnti_to_imsi


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src")
    ap.add_argument("-o", "--out", required=True)
    ap.add_argument("--rbs-per-ru", type=int, default=12)
    ap.add_argument("--skip-rb0", action="store_true", help="drop CRB0 (never sounded)")
    ap.add_argument("--sessions", default=None,
                     help="ue_sessions.json from correlate_rnti_imsi.py --group-out; "
                          "when given, 'u' is set to the resolved IMSI per rnti "
                          "instead of always 0, so downstream tools separate "
                          "users automatically")
    a = ap.parse_args()

    rnti_to_imsi = load_rnti_to_imsi(a.sessions) if a.sessions else {}
    unmatched = 0
    n_in = n_out = 0
    R = 0
    with open(a.src) as f, open(a.out, "w") as g:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                d = json.loads(line)
            except json.JSONDecodeError:
                continue
            n_in += 1
            rb = d["rb"][1:] if a.skip_rb0 else d["rb"]
            R = len(rb) // a.rbs_per_ru
            if R == 0:
                continue
            csi = [sum(rb[r*a.rbs_per_ru:(r+1)*a.rbs_per_ru]) for r in range(R)]
            rnti = d["rnti"]
            if a.sessions:
                u = rnti_to_imsi.get(rnti)
                if u is None:
                    unmatched += 1
                    u = f"unknown-rnti-0x{rnti:x}"
            else:
                u = 0
            rec = {"t": d["t"], "slot": 0,
                   "users": [{"u": u, "rnti": rnti, "served": sum(csi),
                              "rus": list(range(R)), "csi": csi}],
                   "weakest_user_csi": sum(csi)}
            g.write(json.dumps(rec) + "\n")
            n_out += 1

    msg = f"{a.src}: {n_in} lines -> {a.out}: {n_out} lines, {R} RUs x {a.rbs_per_ru} RB"
    if a.sessions:
        n_users = len(set(rnti_to_imsi.values()))
        msg += f", resolved against {n_users} user(s) in {a.sessions}"
        if unmatched:
            msg += f", {unmatched} line(s) had no rnti match (see unknown-rnti-* in output)"
    print(msg)


if __name__ == "__main__":
    main()