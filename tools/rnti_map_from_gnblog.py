#!/usr/bin/env python3
"""rnti_map_from_gnblog.py — build a COMPLETE rnti -> 5G-S-TMSI map from the
gNB detail log, bypassing the fragile RRC-setup tap.

WHY THIS EXISTS
---------------
The gNB "Option B" tap writes /tmp/rnti_map.jsonl by reading the UE identity
from the *rrcSetupRequest*. But that field is only the 5G-S-TMSI when the UE
re-identifies with its stored TMSI. A UE whose registration lapsed (very common
after RLF churn) re-attaches with a RANDOM ue-Identity:

    "ue-Identity": { "randomValue": "...." }

so the tap has nothing to log -> that c-rnti never gets a TMSI binding -> every
per-RB sample under it is DROPPED as "unresolvable", and that user vanishes from
the plots (the 0x4605 / user-49 case).

The real identity is always present one message later, in the *rrcSetupComplete*
NAS (the Registration/Service Request in `dedicatedNAS-Message`). This tool
parses every rrcSetupComplete in the gNB log, pulls the M-TMSI from that NAS,
and ties it to the c-rnti on the block header — one binding per setup, no
concurrency ambiguity, immune to random-ID attaches. The output is drop-in
compatible with what the tap writes, so the rest of the pipeline
(correlate_rnti_imsi.py --rnti-map, rb_to_ru, plots) is unchanged.

USAGE
-----
  # rebuild the map for one capture (plain or .gz log)
  python3 rnti_map_from_gnblog.py 20261012_1341_Experiment_1.gnb_srs_test.log.gz \
          -o 20261012_1341_Experiment_1.rnti_map.jsonl

  # merge the GL-derived bindings INTO an existing (tap) map, keeping both
  python3 rnti_map_from_gnblog.py gnb.log.gz --merge tap.rnti_map.jsonl -o full.rnti_map.jsonl

Output line format (identical to the tap):
  {"t": <epoch_ms>, "rnti": <int>, "stmsi": "40c0...."}
"""
import argparse, gzip, io, json, re, sys
from datetime import datetime

HDR_RE = re.compile(
    r"^([0-9T:\-.]+)\s+\[RRC.*c-rnti=0x([0-9a-fA-F]+):\s*Containerized rrcSetupComplete"
)
NAS_RE = re.compile(r'"dedicatedNAS-Message":\s*"([0-9a-fA-F]+)"')
ANY_HDR_RE = re.compile(r"Containerized (rrcSetup|rrcReestablishment|rrcResume)")


def opener(path):
    if path.endswith(".gz"):
        return io.TextIOWrapper(gzip.open(path, "rb"), encoding="utf-8", errors="replace")
    return open(path, encoding="utf-8", errors="replace")


def ts_to_epoch_ms(ts):
    """ISO 'YYYY-MM-DDTHH:MM:SS.ffffff' (naive, local time — same clock the gNB
    and the tap use) -> epoch milliseconds."""
    try:
        return int(datetime.fromisoformat(ts).timestamp() * 1000)
    except ValueError:
        return None


def extract_stmsi(nas_hex):
    """5G-GUTI (Registration Request) or 5G-S-TMSI (Service Request) -> open5gs
    M-TMSI string (AMF-pointer byte + 4-byte 5G-TMSI = last 5 bytes of the
    5GS mobile identity). Returns None for a SUCI (first-ever attach) or any
    shape we don't recognise."""
    try:
        b = bytes.fromhex(nas_hex)
    except ValueError:
        return None
    i = 0
    # strip 5G NAS security header (7e <sht>): sht 1..4 => +4B MAC +1B seq
    if len(b) >= 2 and b[0] == 0x7E and b[1] in (1, 2, 3, 4):
        i = 7
    # inner plain-NAS header 7e 00
    if i + 2 <= len(b) and b[i] == 0x7E:
        i += 2
    if i >= len(b):
        return None
    msgtype = b[i]; i += 1
    if msgtype == 0x41:            # Registration Request
        i += 1                     # 5GS registration type + ngKSI (1 octet)
        if i + 2 > len(b):
            return None
        ln = int.from_bytes(b[i:i + 2], "big"); i += 2   # 5GS mobile identity, 2-byte length
        ident = b[i:i + ln]
        if ident and (ident[0] & 0x07) == 0x02 and len(ident) >= 5:   # 5G-GUTI
            return ident[-5:].hex()
    elif msgtype == 0x4C:          # Service Request
        i += 1                     # ngKSI + service type (1 octet)
        if i + 2 > len(b):
            return None
        ln = int.from_bytes(b[i:i + 2], "big"); i += 2
        ident = b[i:i + ln]
        if ident and (ident[0] & 0x07) == 0x04 and len(ident) >= 5:   # 5G-S-TMSI
            return ident[-5:].hex()
    return None


def build_map(path, window=60):
    """Scan the gNB log. On each 'Containerized rrcSetupComplete' header, grab
    the c-rnti + timestamp, then look ahead up to `window` lines for the
    dedicatedNAS-Message and extract its M-TMSI."""
    out = []
    n_setups = n_bound = n_suci = 0
    with opener(path) as f:
        pending = None  # (ts, rnti) awaiting its NAS line
        look = 0
        for line in f:
            h = HDR_RE.match(line)
            if h:
                ts, rnti_hex = h.group(1), h.group(2)
                pending = (ts_to_epoch_ms(ts), int(rnti_hex, 16))
                look = window
                n_setups += 1
                continue
            if pending:
                m = NAS_RE.search(line)
                if m:
                    stmsi = extract_stmsi(m.group(1))
                    t, rnti = pending
                    if stmsi:
                        out.append({"t": t, "rnti": rnti, "stmsi": stmsi})
                        n_bound += 1
                    else:
                        n_suci += 1   # SUCI / unrecognised -> leave for AMF-side SUCI decode
                    pending = None
                    continue
                look -= 1
                if look <= 0:
                    pending = None
    return out, (n_setups, n_bound, n_suci)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("gnb_log", help="gNB detail log (.log or .log.gz)")
    ap.add_argument("-o", "--out", default="-", help="output rnti_map.jsonl ('-' = stdout)")
    ap.add_argument("--merge", default=None,
                    help="existing rnti_map.jsonl to union with (tap output); "
                         "GL bindings win on conflict")
    a = ap.parse_args()

    entries, (n_setups, n_bound, n_suci) = build_map(a.gnb_log)

    merged = {}
    if a.merge:
        try:
            for ln in open(a.merge):
                ln = ln.strip()
                if not ln:
                    continue
                d = json.loads(ln)
                merged[(d["rnti"], d["stmsi"])] = d
        except FileNotFoundError:
            print(f"warning: --merge file {a.merge} not found, ignoring", file=sys.stderr)
    for d in entries:                       # GL entries added/override
        merged[(d["rnti"], d["stmsi"])] = d
    allrows = sorted(merged.values(), key=lambda d: (d["t"] or 0))

    fh = sys.stdout if a.out == "-" else open(a.out, "w")
    for d in allrows:
        fh.write(json.dumps(d) + "\n")
    if fh is not sys.stdout:
        fh.close()

    uniq_rnti = len({d["rnti"] for d in allrows})
    print(f"rrcSetupComplete seen={n_setups}  bound(M-TMSI)={n_bound}  "
          f"suci/unparsed={n_suci}  ->  {len(allrows)} map line(s), "
          f"{uniq_rnti} distinct rnti" + (f"  (merged with {a.merge})" if a.merge else ""),
          file=sys.stderr)


if __name__ == "__main__":
    main()
