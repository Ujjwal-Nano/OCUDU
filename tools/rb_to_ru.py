#!/usr/bin/env python3
"""rb_to_ru.py — aggregate a per-RB SRS log into the per-RU jsonl schema.
Usage: python3 rb_to_ru.py /tmp/srs_rb.jsonl -o out.jsonl [--rbs-per-ru 12] [--skip-rb0]
Output lines match swap_metrics format, so plot_csi.py / analyze_position.py work as-is."""
import argparse, json
ap = argparse.ArgumentParser()
ap.add_argument("src"); ap.add_argument("-o", "--out", required=True)
ap.add_argument("--rbs-per-ru", type=int, default=12)
ap.add_argument("--skip-rb0", action="store_true", help="drop CRB0 (never sounded)")
a = ap.parse_args()
n_in = n_out = 0
with open(a.src) as f, open(a.out, "w") as g:
    for line in f:
        line = line.strip()
        if not line: continue
        try: d = json.loads(line)
        except json.JSONDecodeError: continue
        n_in += 1
        rb = d["rb"][1:] if a.skip_rb0 else d["rb"]
        R = len(rb) // a.rbs_per_ru
        if R == 0: continue
        csi = [sum(rb[r*a.rbs_per_ru:(r+1)*a.rbs_per_ru]) for r in range(R)]
        rec = {"t": d["t"], "slot": 0,
               "users": [{"u": 0, "rnti": d["rnti"], "served": sum(csi),
                          "rus": list(range(R)), "csi": csi}],
               "weakest_user_csi": sum(csi)}
        g.write(json.dumps(rec) + "\n"); n_out += 1
print(f"{a.src}: {n_in} lines -> {a.out}: {n_out} lines, {R} RUs x {a.rbs_per_ru} RB")
