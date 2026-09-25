#!/usr/bin/env python3
"""correlate_rnti_imsi.py — parse gNB NGAP log + AMF journal to build a
persistent rnti -> IMSI mapping, correctly handling reconnections (same
IMSI, new rnti/amf_ue each time).

Key fix #1 (original): rnti and amf_ue assignments are kept as
TIMESTAMPED LISTS instead of plain dicts. A plain dict overwrites the
previous rnti the moment a UE reconnects, which silently destroys the very
history we need to tell "same UE, different rnti" apart from "different
UE, similar rnti". Sessions are matched to the nearest-in-time rnti, then
grouped by IMSI so you get one row per UE with all of its rnti/time-range
sessions underneath, instead of one flat rnti->imsi table.

Key fix #2 (this version): IMSI resolution no longer depends solely on
decoding a SUCI. A SUCI is only sent on a fresh registration -- a UE
reconnecting via 5G-GUTI (a routine reattach / service request, the common
case) often doesn't resend one, so that session's IMSI used to come back
None and land in a generic shared UNKNOWN_IMSI bucket, even though it's
the SAME already-known UE reconnecting. Open5GS tags nearly every log line
for an established UE context directly with "[imsi-XXXX...]", not just
the one-time SUCI-decode line -- so this version also backfills a
session's IMSI the moment any such line is seen, no SUCI or decoding
required. This backfill only fires when exactly ONE UE context is
currently open (unambiguous); with several UEs open concurrently it
declines to guess rather than risk attributing one UE's session to
another, and falls back to the old SUCI-only behavior for that session.

Key fix #3 (this version): optional --known-imsis whitelist. Sessions
whose resolved IMSI is not a registered subscriber (e.g. a stray session
that resolved to an IMSI not provisioned in the core) are dropped, so
they never enter the grouped output that downstream tools consume.

Usage:
  python3 correlate_rnti_imsi.py --gnb-log /tmp/gnb_srs_test.log \
      --amf-since "2026-09-14 11:00:00" -o rnti_imsi_map.csv \
      --group-out ue_sessions.json \
      --known-imsis known_imsis.txt
"""

import argparse, re, subprocess, csv, json, shlex, sys


def parse_gnb_ngap(path):
    """Extract (timestamp, ran_ue, amf_ue, msg) from NGAP lines, in file order."""
    events = []
    with open(path) as f:
        for line in f:
            m = re.match(
                r"([\d\-T:.]+) \[NGAP.*ran_ue=(\d+)(?: amf_ue=(\d+))?: (\w+)", line
            )
            if not m:
                continue
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


def fetch_amf_journal(since, until=None, host=None):
    """Run journalctl (locally or over SSH on `host`) and return its raw
    stdout text. Split out from the parsing logic below so the parser can
    be unit-tested against a plain string without shelling out."""
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
            + (
                "If this is a password/permission error, the remote user needs "
                "passwordless sudo for journalctl (see --amf-host help) or SSH "
                "key auth needs to be set up to that host."
                if host
                else "Try running with sudo, or check the unit name with "
                "'systemctl list-units | grep open5gs'."
            )
        )
    return result.stdout


def parse_amf_journal_text(out):
    """Pull AMF-side sessions out of raw journalctl text: one entry per
    amf_ue_ngap_id, each resolved to a stable IMSI. 'end' is filled in from
    the matching UE Context Release block, which repeats the same
    RAN_UE_NGAP_ID/AMF_UE_NGAP_ID pair a second time.

    IMSI resolution, in priority order:
      1. Direct "[imsi-XXXX...]" tag on ANY later log line for this
         session's context, backfilled the instant one is seen -- this is
         what makes reconnects-without-a-fresh-SUCI still resolve to the
         same known UE. Only trusted when exactly one UE context is
         currently open (see `open_amf_ues` below), so a genuinely
         concurrent multi-UE capture never risks crossing wires between
         two real users.
      2. SUCI decode (scheme 0 / null-protection only) from the
         "[suci-...]" line inside the same Context Release block as this
         session's RAN/AMF id pair -- the original mechanism, kept as a
         fallback for when >1 context is open or no direct imsi tag shows
         up before the session closes.
      Sessions that get neither stay imsi=None and land in the
      UNKNOWN_IMSI bucket downstream, same as before.
    """
    ts_re = re.compile(r"(\d{2}/\d{2} \d{2}:\d{2}:\d{2}\.\d+)")
    suci_re = re.compile(r"\[suci-([\d\-]+)\]")
    imsi_re = re.compile(r"\[imsi-(\d{5,20})")
    ranamf_re = re.compile(r"RAN_UE_NGAP_ID\[(\d+)\]\s*AMF_UE_NGAP_ID\[(\d+)\]")

    sessions_by_amf_ue = {}
    open_amf_ues = set()  # amf_ue ids with a session started but not yet released
    pending_suci = None  # most recent SUCI seen, not yet consumed by a RAN/AMF id line
    n_direct = n_suci = 0

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
                if imsi:
                    n_suci += 1
                sessions_by_amf_ue[amf_ue] = {
                    "start": ts,
                    "end": None,
                    "ran_ue": ran_ue,
                    "amf_ue": amf_ue,
                    "suci": pending_suci,
                    "imsi": imsi,
                    "imsi_source": "suci" if imsi else None,
                }
                open_amf_ues.add(amf_ue)
            else:
                # second sighting of this amf_ue's RAN/AMF pair = UE Context Release
                sessions_by_amf_ue[amf_ue]["end"] = ts
                open_amf_ues.discard(amf_ue)
            continue  # a RAN/AMF line is never also an imsi-tag line

        m_imsi = imsi_re.search(line)
        if m_imsi and len(open_amf_ues) == 1:
            only_open = next(iter(open_amf_ues))
            sess = sessions_by_amf_ue[only_open]
            if sess["imsi"] is None:
                sess["imsi"] = m_imsi.group(1)
                sess["imsi_source"] = "direct"
                n_direct += 1
            # else: already resolved (via suci or an earlier direct tag) -- leave it

    sessions = list(sessions_by_amf_ue.values())
    n_unresolved = sum(1 for s in sessions if s["imsi"] is None)
    stats = {
        "n_sessions": len(sessions),
        "n_suci": n_suci,
        "n_direct": n_direct,
        "n_unresolved": n_unresolved,
    }
    return sessions, stats


def parse_amf_sessions(since, until=None, host=None):
    """Fetch the AMF journal (locally or via SSH) and parse it. See
    parse_amf_journal_text() for the actual logic and IMSI-resolution
    strategy.

    If host is given (e.g. "user@user-ThinkPad-X230"), the journalctl call
    is run over SSH on that machine instead of locally — this is what lets
    the whole script run from the gNB PC while Open5GS's journal lives on
    a different machine. Requires SSH key-based auth already set up (the
    call uses BatchMode=yes, so it fails fast instead of hanging on a
    password prompt) AND passwordless sudo for journalctl on the remote
    host — see the NOPASSWD note in main()'s --amf-host help text.
    """
    out = fetch_amf_journal(since, until, host)
    sessions, stats = parse_amf_journal_text(out)
    print(
        f"AMF sessions: {stats['n_sessions']} total — "
        f"{stats['n_suci']} resolved via SUCI, {stats['n_direct']} via direct "
        f"[imsi-...] tag, {stats['n_unresolved']} still unresolved"
    )
    return sessions


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
        raw_sessions.append(
            {
                "start": ts,
                "end": None,
                "ran_ue": ran_ue,
                "amf_ue": resolved_amf_ue,
                "rnti": rnti,
                "imsi": imsi,
                "suci": suci,
            }
        )

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


def load_known_imsis(path):
    """Read a whitelist file of registered IMSIs, one per line. Blank lines
    and lines starting with '#' are ignored. Returns a set of IMSI strings,
    or None if path is falsy (whitelist disabled)."""
    if not path:
        return None
    with open(path) as f:
        known = {ln.strip() for ln in f if ln.strip() and not ln.startswith("#")}
    print(f"whitelist: {len(known)} registered IMSIs from {path}")
    return known


def group_by_imsi(sessions, known_imsis=None):
    """UE-centric view: imsi -> list of its rnti sessions with time ranges.
    every session below one IMSI key IS the same UE, by construction.
    Falls back to grouping by SUCI when the IMSI couldn't be decoded.

    If known_imsis is given, sessions whose resolved IMSI is a real IMSI
    but not in the registered set are dropped (removes spurious IMSIs like
    a stray session that resolved to an IMSI not provisioned in the core).
    SUCI-keyed and fully-unresolved sessions are NOT dropped — there is no
    IMSI to check them against, so they pass through to their usual keys."""
    grouped = {}
    unknown = []
    dropped = {}
    for s in sessions:
        entry = {
            "rnti": hex(s["rnti"]) if s["rnti"] else None,
            "ran_ue": s["ran_ue"],
            "amf_ue": s["amf_ue"],
            "start": s["start"],
            "end": s["end"],
        }
        # drop sessions that resolved to a real IMSI not on the whitelist
        if known_imsis is not None and s["imsi"] and s["imsi"] not in known_imsis:
            dropped[s["imsi"]] = dropped.get(s["imsi"], 0) + 1
            continue
        key = s["imsi"] or (
            f"suci-{s['suci']} (imsi undecodable)" if s.get("suci") else None
        )
        if key:
            grouped.setdefault(key, []).append(entry)
        else:
            unknown.append(entry)
    if unknown:
        grouped["UNKNOWN_IMSI"] = unknown
    if dropped:
        print(
            f"dropped {sum(dropped.values())} session(s) from "
            f"{len(dropped)} unregistered IMSI(s): {', '.join(dropped)}",
            file=sys.stderr,
        )
    return grouped


def group_from_rnti_map(rnti_map_path, known_imsis=None, label_map=None):
    """OPTION B: build the grouped ue_sessions structure directly from the gNB's
    own /tmp/rnti_map.jsonl (lines: {"t":ms,"rnti":int,"stmsi":"hex"}), with NO
    AMF query and NO cross-log time-matching. Every attach/reconnection logged a
    line, so all of a phone's rnti sessions share one stmsi -> perfect grouping,
    reconnections included, no UNKNOWN bucket.

    Output shape matches group_by_imsi(): {user_key: [{rnti,ran_ue,amf_ue,start,end,stmsi}, ...]}.
    user_key is the stmsi hex, unless label_map {stmsi: imsi} renames it. If
    known_imsis is given AND a label_map maps into it, non-whitelisted users drop.
    """
    by_stmsi = {}
    with open(rnti_map_path) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                d = json.loads(line)
            except json.JSONDecodeError:
                continue
            stmsi, rnti, t = d.get("stmsi"), d.get("rnti"), d.get("t")
            if stmsi is None or rnti is None:
                continue
            rec = by_stmsi.setdefault(stmsi, {})
            if rnti not in rec:  # earliest t per rnti
                rec[rnti] = t

    grouped = {}
    for stmsi, rntis in by_stmsi.items():
        key = (label_map or {}).get(stmsi, stmsi)
        # whitelist only applies when we actually resolved to an imsi via label_map
        if known_imsis is not None and label_map and stmsi in label_map:
            if key not in known_imsis:
                continue
        sess = grouped.setdefault(key, [])
        for rnti, t in sorted(rntis.items(), key=lambda kv: (kv[1] is None, kv[1])):
            sess.append(
                {
                    "rnti": hex(rnti),
                    "ran_ue": None,
                    "amf_ue": None,
                    "start": t,
                    "end": None,
                    "stmsi": stmsi,
                }
            )
    return grouped


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--gnb-log",
        default=None,
        help="required for AMF-correlation mode; unused with --rnti-map",
    )
    ap.add_argument("--amf-since", default=None, help='e.g. "2026-09-14 11:00:00"')
    ap.add_argument(
        "--rnti-map",
        default=None,
        help="OPTION B: build ue_sessions.json directly from the gNB's "
        "/tmp/rnti_map.jsonl (rnti->5G-S-TMSI). Bypasses the AMF "
        "query entirely; correctly folds in all reconnections.",
    )
    ap.add_argument(
        "--label-map",
        default=None,
        help="with --rnti-map: JSON {stmsi_hex: imsi} to rename user keys",
    )
    ap.add_argument(
        "--amf-until", default=None, help='e.g. "2026-09-14 11:40:00" (optional)'
    )
    ap.add_argument(
        "--amf-host",
        default=None,
        help='e.g. "user@user-ThinkPad-X230" — run journalctl over SSH on this '
        "host instead of locally. Requires: (1) SSH key-based auth already "
        "set up from this machine to that host (no password prompt), and "
        "(2) passwordless sudo for journalctl on that host, e.g. add a line "
        'like "user ALL=(ALL) NOPASSWD: /usr/bin/journalctl" via '
        '"sudo visudo" on the remote host. Without both, this will fail fast '
        "rather than hang.",
    )
    ap.add_argument(
        "-o",
        "--out",
        default="rnti_imsi_map.csv",
        help="flat CSV, one row per session (existing behavior)",
    )
    ap.add_argument(
        "--group-out",
        default="ue_sessions.json",
        help="NEW: JSON grouped by IMSI -> list of rnti sessions",
    )
    ap.add_argument(
        "--known-imsis",
        default=None,
        help="file of registered IMSIs (one per line, # comments ok); "
        "sessions resolving to an unregistered IMSI are dropped",
    )

    a = ap.parse_args()

    known_imsis = load_known_imsis(a.known_imsis)

    # ---- OPTION B: gNB-sourced rnti->5G-S-TMSI map, no AMF ----
    if a.rnti_map:
        label_map = None
        if a.label_map:
            with open(a.label_map) as f:
                label_map = json.load(f)
        grouped = group_from_rnti_map(a.rnti_map, known_imsis, label_map)
        with open(a.group_out, "w") as f:
            json.dump(grouped, f, indent=2)
        print(
            f"[rnti-map mode] wrote {len(grouped)} UE(s) to {a.group_out} "
            f"(no AMF query)"
        )
        for key, sess_list in grouped.items():
            rn = ", ".join(s["rnti"] for s in sess_list)
            print(f"  {key}: {len(sess_list)} rnti session(s) [{rn}]")
        return

    # ---- AMF-correlation mode (original) ----
    if not a.gnb_log or not a.amf_since:
        sys.exit("AMF mode needs --gnb-log and --amf-since (or use --rnti-map)")

    events = parse_gnb_ngap(a.gnb_log)
    rnti_entries = parse_gnb_rrc_rnti(a.gnb_log)
    amf_sessions = parse_amf_sessions(a.amf_since, a.amf_until, a.amf_host)

    sessions = build_sessions(events, rnti_entries, amf_sessions)

    # flat CSV (same shape as before)
    with open(a.out, "w", newline="") as f:
        w = csv.DictWriter(
            f, fieldnames=["timestamp", "rnti", "ran_ue", "amf_ue", "imsi"]
        )
        w.writeheader()
        for s in sessions:
            w.writerow(
                {
                    "timestamp": s["start"],
                    "rnti": hex(s["rnti"]) if s["rnti"] else None,
                    "ran_ue": s["ran_ue"],
                    "amf_ue": s["amf_ue"],
                    "imsi": s["imsi"],
                }
            )
    print(f"wrote {len(sessions)} sessions to {a.out}")

    # grouped-by-IMSI JSON (the new part), with the whitelist applied
    grouped = group_by_imsi(sessions, known_imsis)
    with open(a.group_out, "w") as f:
        json.dump(grouped, f, indent=2)
    print(f"wrote {len(grouped)} UE(s) to {a.group_out}")

    for imsi, sess_list in grouped.items():
        print(f"\n{imsi}  ({len(sess_list)} session(s))")
        for s in sess_list:
            print(
                f"  rnti={s['rnti']:<8} ran_ue={s['ran_ue']:<4} amf_ue={s['amf_ue']!s:<6} "
                f"start={s['start']}  end={s['end']}"
            )


if __name__ == "__main__":
    main()
