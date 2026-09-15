#!/usr/bin/env python3
"""correlate_rnti_imsi.py — parse gNB NGAP log + AMF journal to build a
persistent rnti -> IMSI mapping, correctly handling reconnections (same
IMSI, new rnti/amf_ue each time).

Key fix vs. the original version: rnti and amf_ue assignments are kept as
TIMESTAMPED LISTS instead of plain dicts. A plain dict overwrites the
previous rnti the moment a UE reconnects, which silently destroys the very
history we need to tell "same UE, different rnti" apart from "different
UE, similar rnti". Sessions are matched to the nearest-in-time rnti, then
grouped by IMSI so you get one row per UE with all of its rnti/time-range
sessions underneath, instead of one flat rnti->imsi table.

Usage:
  python3 correlate_rnti_imsi.py --gnb-log /tmp/gnb_srs_test.log \
      --amf-since "2026-09-14 11:00:00" -o rnti_imsi_map.csv \
      --group-out ue_sessions.json
"""
import argparse, re, subprocess, csv, json, shlex

def parse_gnb_ngap(path):
    """Extract (timestamp, ran_ue, amf_ue, msg) from NGAP lines, in file order."""
    events = []
    with open(path) as f:
        for line in f:
            m = re.match(r"([\d\-T:.]+) \[NGAP.*ran_ue=(\d+)(?: amf_ue=(\d+))?: (\w+)", line)
            if not m: continue
            ts, ran_ue, amf_ue, msg = m.groups()
            events.append((ts, ran_ue, amf_ue, msg))
    return events

def parse_gnb_rrc_rnti(path):
    """Extract (timestamp, ran_ue, rnti) triples IN ORDER, one entry per assignment.

    Deliberately a list, not a dict keyed by ran_ue: the gNB reuses ran_ue
    ids, and a UE reconnecting gets a brand new c-rnti on the *same*
    ran_ue id. Overwriting into a dict would silently drop every rnti
    except the last one seen for that id.
    """
    entries = []
    ts_re = re.compile(r"^([\d\-T:.]+)")
    with open(path) as f:
        for line in f:
            m = re.search(r"ue=(\d+) c-rnti=0x([0-9a-fA-F]+)", line)
            if not m:
                continue
            ts_m = ts_re.match(line)
            ts = ts_m.group(1) if ts_m else None
            entries.append((ts, m.group(1), int(m.group(2), 16)))
    return entries

def rnti_at(rnti_entries, ran_ue, ts):
    """Return the c-rnti in effect for ran_ue at time ts: the latest
    assignment at-or-before ts, falling back to the earliest known
    assignment for that ran_ue if none precede ts (e.g. missing timestamps)."""
    candidates = [e for e in rnti_entries if e[1] == ran_ue]
    if not candidates:
        return None
    if ts:
        before = [e for e in candidates if e[0] and e[0] <= ts]
        if before:
            return before[-1][2]
    return candidates[0][2]

def decode_suci(suci_digits):
    """Decode SUCI '<supi-type>-<mcc>-<mnc>-<routing-ind>-<scheme>-<hnkey>-<scheme-output>'
    into an IMSI. Only possible when scheme==0 (null/no protection scheme),
    which is open5gs' common default in lab/test deployments — in that case
    the 'scheme output' field IS the plaintext MSIN, so imsi = mcc+mnc+msin.
    Returns None if the scheme isn't 0 (i.e. it's genuinely encrypted and
    can't be recovered from the log alone)."""
    parts = suci_digits.split("-")
    if len(parts) != 7:
        return None
    _supi_type, mcc, mnc, _routing_ind, scheme, _hnkey, scheme_output = parts
    if scheme != "0":
        return None
    return mcc + mnc + scheme_output

def parse_amf_sessions(since, until=None, host=None):
    """Pull AMF-side sessions from journalctl: one entry per amf_ue_ngap_id,
    each resolved to a stable IMSI via SUCI decoding (not via the separate
    'imsi-...' line, which — critically — can appear AFTER the
    AMF_UE_NGAP_ID line, sometimes long after, and carries no id of its own
    to say which session it belongs to once two UEs' sessions overlap).
    'end' is filled in from the matching UE Context Release block, which
    repeats the same RAN_UE_NGAP_ID/AMF_UE_NGAP_ID pair a second time.

    If host is given (e.g. "user@user-ThinkPad-X230"), the journalctl call
    is run over SSH on that machine instead of locally — this is what lets
    the whole script run from the gNB PC while Open5GS's journal lives on
    a different machine. Requires SSH key-based auth already set up (the
    call uses BatchMode=yes, so it fails fast instead of hanging on a
    password prompt) AND passwordless sudo for journalctl on the remote
    host — see the NOPASSWD note in main()'s --amf-host help text.
    """
    cmd = ["sudo", "journalctl", "-u", "open5gs-amfd", "--since", since, "--no-pager"]
    if until:
        cmd += ["--until", until]
    if host:
        remote_cmd = " ".join(shlex.quote(c) for c in cmd)
        cmd = ["ssh", "-o", "BatchMode=yes", host, remote_cmd]

    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        where = f"on {host} via SSH" if host else "locally"
        raise RuntimeError(
            f"journalctl failed {where} (exit {result.returncode}).\n"
            f"stderr: {result.stderr.strip()}\n"
            + ("If this is a password/permission error, the remote user needs "
               "passwordless sudo for journalctl (see --amf-host help) or SSH "
               "key auth needs to be set up to that host." if host else
               "Try running with sudo, or check the unit name with "
               "'systemctl list-units | grep open5gs'.")
        )
    out = result.stdout

    ts_re = re.compile(r"(\d{2}/\d{2} \d{2}:\d{2}:\d{2}\.\d+)")
    suci_re = re.compile(r"\[suci-([\d\-]+)\]")
    ranamf_re = re.compile(r"RAN_UE_NGAP_ID\[(\d+)\]\s*AMF_UE_NGAP_ID\[(\d+)\]")

    sessions_by_amf_ue = {}
    pending_suci = None  # most recent SUCI seen, not yet consumed by a RAN/AMF id line

    for line in out.splitlines():
        ts_m = ts_re.search(line)
        ts = ts_m.group(1) if ts_m else None

        m_suci = suci_re.search(line)
        if m_suci:
            pending_suci = m_suci.group(1)

        m_ranamf = ranamf_re.search(line)
        if m_ranamf:
            ran_ue, amf_ue = m_ranamf.groups()
            if amf_ue not in sessions_by_amf_ue:
                imsi = decode_suci(pending_suci) if pending_suci else None
                sessions_by_amf_ue[amf_ue] = {
                    "start": ts, "end": None, "ran_ue": ran_ue, "amf_ue": amf_ue,
                    "suci": pending_suci, "imsi": imsi,
                }
            else:
                # second sighting of this amf_ue's RAN/AMF pair = UE Context Release
                sessions_by_amf_ue[amf_ue]["end"] = ts

    return list(sessions_by_amf_ue.values())


def amf_ue_at(events, ran_ue, start_ts):
    """amf_ue_ngap_id only appears on a LATER line (InitialContextSetupRequest)
    than the InitialUEMessage that opens the session, so it can't be read off
    the same event tuple. Find the amf_ue assigned to this ran_ue on the
    nearest event at-or-after start_ts, before that ran_ue id gets reused."""
    candidates = [e for e in events if e[1] == ran_ue and e[2] is not None]
    if not candidates:
        return None
    after = [e for e in candidates if not start_ts or not e[0] or e[0] >= start_ts]
    chosen = after[0] if after else candidates[-1]
    return chosen[2]

def build_sessions(events, rnti_entries, amf_sessions):
    """One row per NGAP session (InitialUEMessage), each resolved to its
    rnti (nearest-in-time for that ran_ue) and imsi (via matching amf_ue
    against the AMF-side session list). session_end = timestamp of the
    NEXT session on that same ran_ue id, if any — i.e. "this rnti/session
    was live until the ran_ue id got reused." None means still-open / no
    later reuse observed in this log window.
    """
    amf_by_id = {s["amf_ue"]: s for s in amf_sessions}

    raw_sessions = []
    for ts, ran_ue, amf_ue, msg in events:
        if msg != "InitialUEMessage":
            continue
        rnti = rnti_at(rnti_entries, ran_ue, ts)
        resolved_amf_ue = amf_ue_at(events, ran_ue, ts)
        amf_sess = amf_by_id.get(resolved_amf_ue) if resolved_amf_ue else None
        imsi = amf_sess["imsi"] if amf_sess else None
        suci = amf_sess["suci"] if amf_sess else None
        raw_sessions.append({
            "start": ts, "end": None, "ran_ue": ran_ue,
            "amf_ue": resolved_amf_ue, "rnti": rnti, "imsi": imsi, "suci": suci,
        })

    # fill in session_end = start of the next session sharing the same ran_ue
    by_ran_ue = {}
    for s in raw_sessions:
        by_ran_ue.setdefault(s["ran_ue"], []).append(s)
    for ran_ue, sess_list in by_ran_ue.items():
        sess_list.sort(key=lambda s: s["start"] or "")
        for i in range(len(sess_list) - 1):
            sess_list[i]["end"] = sess_list[i + 1]["start"]

    raw_sessions.sort(key=lambda s: s["start"] or "")
    return raw_sessions

def group_by_imsi(sessions):
    """UE-centric view: imsi -> list of its rnti sessions with time ranges.
    This is the actual answer to 'same UE, different rnti on reconnect' —
    every session below one IMSI key IS the same UE, by construction,
    because the AMF only ever issues one IMSI per subscriber.
    Falls back to grouping by SUCI when the IMSI couldn't be decoded (e.g.
    a non-null SUCI protection scheme) — still a stable per-UE key, just
    not human-readable as an IMSI."""
    grouped = {}
    unknown = []
    for s in sessions:
        entry = {"rnti": hex(s["rnti"]) if s["rnti"] else None,
                  "ran_ue": s["ran_ue"], "amf_ue": s["amf_ue"],
                  "start": s["start"], "end": s["end"]}
        key = s["imsi"] or (f"suci-{s['suci']} (imsi undecodable)" if s.get("suci") else None)
        if key:
            grouped.setdefault(key, []).append(entry)
        else:
            unknown.append(entry)
    if unknown:
        grouped["UNKNOWN_IMSI"] = unknown
    return grouped

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--gnb-log", required=True)
    ap.add_argument("--amf-since", required=True, help='e.g. "2026-09-14 11:00:00"')
    ap.add_argument("--amf-until", default=None, help='e.g. "2026-09-14 11:40:00" (optional)')
    ap.add_argument("--amf-host", default=None,
                     help='e.g. "user@user-ThinkPad-X230" — run journalctl over SSH on this '
                          'host instead of locally. Requires: (1) SSH key-based auth already '
                          'set up from this machine to that host (no password prompt), and '
                          '(2) passwordless sudo for journalctl on that host, e.g. add a line '
                          'like "user ALL=(ALL) NOPASSWD: /usr/bin/journalctl" via '
                          '"sudo visudo" on the remote host. Without both, this will fail fast '
                          'rather than hang.')
    ap.add_argument("-o", "--out", default="rnti_imsi_map.csv",
                     help="flat CSV, one row per session (existing behavior)")
    ap.add_argument("--group-out", default="ue_sessions.json",
                     help="NEW: JSON grouped by IMSI -> list of rnti sessions")
    a = ap.parse_args()

    events = parse_gnb_ngap(a.gnb_log)
    rnti_entries = parse_gnb_rrc_rnti(a.gnb_log)
    amf_sessions = parse_amf_sessions(a.amf_since, a.amf_until, a.amf_host)

    sessions = build_sessions(events, rnti_entries, amf_sessions)

    # flat CSV (same shape as before)
    with open(a.out, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=["timestamp","rnti","ran_ue","amf_ue","imsi"])
        w.writeheader()
        for s in sessions:
            w.writerow({"timestamp": s["start"],
                        "rnti": hex(s["rnti"]) if s["rnti"] else None,
                        "ran_ue": s["ran_ue"], "amf_ue": s["amf_ue"], "imsi": s["imsi"]})
    print(f"wrote {len(sessions)} sessions to {a.out}")

    # grouped-by-IMSI JSON (the new part)
    grouped = group_by_imsi(sessions)
    with open(a.group_out, "w") as f:
        json.dump(grouped, f, indent=2)
    print(f"wrote {len(grouped)} UE(s) to {a.group_out}")

    for imsi, sess_list in grouped.items():
        print(f"\n{imsi}  ({len(sess_list)} session(s))")
        for s in sess_list:
            print(f"  rnti={s['rnti']:<8} ran_ue={s['ran_ue']:<4} amf_ue={s['amf_ue']!s:<6} "
                  f"start={s['start']}  end={s['end']}")

if __name__ == "__main__":
    main()
