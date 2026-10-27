#!/bin/bash
# csi_run_ploss.sh — PACKET-LOSS / throughput evaluation for the swap A/B toggle.
#
# Deliberately SEPARATE from csi_run_swap.sh. This mode runs the swap with the
# A/B toggle ON (yaml swap_ab_period > 0), so the swap flips ON/OFF within one
# session and we compare per-user loss/throughput between the two states on the
# SAME channel. The swap-quality plots (outage/epsilon/quality/algocmp) do NOT
# apply to toggled data and are intentionally absent here — this script only
# COLLECTS the logs the loss analysis needs.
#
#   csi_run_ploss.sh start                  # clear logs BEFORE launching the gNB
#   csi_run_ploss.sh run <name> [opts] [note]
#
# Precondition for `run`: the gNB is ALREADY up with its metrics TABLE tee'd to
# a file and 't' pressed once, e.g.
#   cd ~/OCUDU
#   sudo script -q -c "./build/apps/gnb/gnb -c configs/gnb_srs_test.yaml" /dev/null \
#        | ts '%.s' | tee ~/gnb_ab.log
# and the yaml has  epsilon: 1  AND  swap_ab_period: 100 .
#
# `run` does everything automatically:
#   keepalive-wake the UEs -> wait until all attach -> mark (clean all-attached
#   start) -> blast --rate/UE for --dur s (swap toggling) -> snapshot the swap
#   log + the matching gNB-table slice (+ raw logs) into
#   datasets/Packet_loss/<name>/ -> print the files to send.
#
# Tunables (env or flags): --rate 3M  --dur 90  --users N  --table ~/gnb_ab.log
set -e
RB=/tmp/srs_rb.jsonl            # per-RB producer log
SW=/tmp/swap_metrics.jsonl      # per-RU scheduler log (has swap_enabled + per-user CSI)
GL=/tmp/gnb_srs_test.log        # gNB detail log (SRS/PHY)
STARTFILE=/tmp/ploss_capture_start.txt
REPO=/home/tud/OCUDU
GNB_YAML="$REPO/configs/gnb_srs_packet.yaml"
# The UDP blaster MUST run on the 5G CORE / UPF PC (where ogstun lives and the
# route to the UE IPs exists) — NOT here on the gNB PC. So we run it over SSH.
# Passwordless SSH to the core is required for this to be automatic:
#   ssh-copy-id user@192.168.200.207      (one time)
CORE_HOST=${CORE_HOST:-user@192.168.200.207}    # the core PC
CORE_DLLOAD=${CORE_DLLOAD:-'~/dl_load.sh'}       # path to dl_load.sh ON the core (remote home)
GNB_TABLE=${GNB_TABLE:-/home/tud/gnb_ab.log}    # where the gNB metrics table is tee'd (local, gNB PC)
UES=(${UES:-10.45.0.11 10.45.0.12 10.45.0.13 10.45.0.14})
PT_RATE=${PT_RATE:-3M}
PT_DUR=${PT_DUR:-240}
NUM_USERS=${NUM_USERS:-$(grep -oP 'num_users:\s*\K[0-9]+' "$GNB_YAML" 2>/dev/null | head -1)}
NUM_USERS=${NUM_USERS:-4}
SWAP_AB=${SWAP_AB:-$(grep -oP 'swap_ab_period:\s*\K[0-9]+' "$GNB_YAML" 2>/dev/null | head -1)}
SWAP_AB=${SWAP_AB:-0}

# max number of simultaneously-real users (rnti!=0, non-floor CSI) in the last
# ~40 swap-log lines. Echoes one integer.
_attached_count() {
  python3 - "$SW" <<'PY'
import sys, json
try:
    lines = open(sys.argv[1]).read().splitlines()[-40:]
except Exception:
    print(0); raise SystemExit
best = 0
for ln in lines:
    ln = ln.strip()
    if not ln:
        continue
    try:
        r = json.loads(ln)
    except Exception:
        continue
    us = r.get("users", [])
    c = sum(1 for u in us
            if u.get("rnti", 0) != 0 and max(u.get("csi", [0]) or [0]) > 2e-9)
    best = max(best, c)
print(best)
PY
}

case "$1" in
  start)
    sudo rm -f "$RB" "$SW" "$GL" 2>/dev/null || true
    date +"%Y-%m-%d %H:%M:%S" > "$STARTFILE"
    echo "cleared logs — now launch the gNB (script|ts|tee ~/gnb_ab.log, press 't'),"
    echo "then: csi_run_ploss.sh run <name>"
    ;;

  run)
    [ -z "$2" ] && { echo "usage: csi_run_ploss.sh run <name> [--rate 3M] [--dur 90] [--users N] [--table path] [note]"; exit 1; }
    NAME="$2"; shift 2
    PT_USERS="$NUM_USERS"
    while [ $# -gt 0 ]; do
      case "$1" in
        --rate)  PT_RATE="$2"; shift 2 ;;
        --dur)   PT_DUR="$2";  shift 2 ;;
        --users) PT_USERS="$2"; shift 2 ;;
        --table) GNB_TABLE="$2"; shift 2 ;;
        *) break ;;
      esac
    done
    NOTE="${1:-no note}"

    # The blaster runs on the CORE over SSH — verify we can reach it and the
    # script is there, before we start (passwordless SSH required for automation).
    if ! ssh -o BatchMode=yes -o ConnectTimeout=6 "$CORE_HOST" "test -x $CORE_DLLOAD" 2>/dev/null; then
      echo "ERROR: can't run the blaster on the core via SSH." >&2
      echo "       Need passwordless SSH to $CORE_HOST and $CORE_DLLOAD present there." >&2
      echo "       Set it up once:  ssh-copy-id $CORE_HOST" >&2
      echo "       (override host/path with CORE_HOST=.. CORE_DLLOAD=..)" >&2
      exit 1
    fi
    if [ "$SWAP_AB" -eq 0 ] 2>/dev/null; then
      echo "WARNING: swap_ab_period=0 in the yaml — swap is ON the WHOLE run, so there is" >&2
      echo "         no OFF phase to compare. Set swap_ab_period: 100 and restart the gNB." >&2
    else
      echo ">> swap_ab_period=$SWAP_AB (toggle ON — ON/OFF phases alternate)."
    fi
    if [ ! -s "$GNB_TABLE" ]; then
      echo "WARNING: gNB table '$GNB_TABLE' empty/missing — launch gNB with script|ts|tee and press 't'," >&2
      echo "         or pass --table. Swap log will still be captured, but throughput/BLER needs the table." >&2
    fi

    echo ">> ploss '$NAME': rate=$PT_RATE/UE  dur=${PT_DUR}s  expect $PT_USERS users"
    echo ">> UEs: ${UES[*]}"

    # (a) keepalive on the CORE (via SSH) so UEs get scheduled and the swap log
    #     fills while we wait for them to attach.
    echo ">> keepalive (0.5M, on core $CORE_HOST) to wake/hold UEs while waiting..."
    ssh "$CORE_HOST" "RATE=0.5M DUR=0 $CORE_DLLOAD ${UES[*]} >/tmp/ploss_keepalive.log 2>&1" &
    KA=$!

    # (b) wait until all PT_USERS attach (or time out at 3 min)
    deadline=$(( $(date +%s) + 180 ))
    n=0
    while :; do
      n=$(_attached_count)
      echo "   attached: ${n}/${PT_USERS}"
      [ "$n" -ge "$PT_USERS" ] && { echo ">> all $PT_USERS attached."; break; }
      if [ "$(date +%s)" -ge "$deadline" ]; then
        echo ">> TIMEOUT — proceeding with $n user(s)." >&2
        break
      fi
      sleep 3
    done

    # stop the keepalive on the core, and the local ssh that launched it
    ssh "$CORE_HOST" "pkill -f dl_load" 2>/dev/null || true
    kill "$KA" 2>/dev/null || true
    wait "$KA" 2>/dev/null || true
    sleep 1

    # (c) MARK: clean all-attached start (truncate in place; gNB holds the fds)
    for f in "$RB" "$SW"; do
      [ -e "$f" ] && sudo truncate -s 0 "$f"
    done
    date +"%Y-%m-%d %H:%M:%S" > "$STARTFILE"
    echo ">> marked (clean all-attached start)."

    # remember table position so we slice out only the test window
    TBL_START=1
    [ -s "$GNB_TABLE" ] && TBL_START=$(( $(wc -l < "$GNB_TABLE") + 1 ))

    # (d) the measured blast on the CORE (via SSH; blocks PT_DUR seconds)
    echo ">> blasting $PT_RATE/UE for ${PT_DUR}s (on core $CORE_HOST)..."
    ssh "$CORE_HOST" "RATE=$PT_RATE DUR=$PT_DUR $CORE_DLLOAD ${UES[*]}" || true
    echo ">> blast done."
    sleep 1

    # (e) snapshot
    STAMP=$(date +%Y%m%d_%H%M); BASE="${STAMP}_$NAME"
    D="$REPO/datasets/Packet_loss/$NAME"; mkdir -p "$D"
    printf '%s\nploss: rate=%s/UE dur=%ss users=%s swap_ab_period=%s\n' \
           "$NOTE" "$PT_RATE" "$PT_DUR" "$PT_USERS" "$SWAP_AB" > "$D/$BASE.ploss.txt"

    if [ -s "$SW" ]; then
      sudo cp "$SW" "$D/$BASE.swap.jsonl"; sudo chown "$USER" "$D/$BASE.swap.jsonl" 2>/dev/null || true
      echo ">> saved swap log : $D/$BASE.swap.jsonl ($(wc -l < "$D/$BASE.swap.jsonl") periods)"
    else
      echo ">> WARNING: swap log empty — no traffic hit the swap?" >&2
    fi
    if [ -s "$GNB_TABLE" ]; then
      sed -n "${TBL_START},\$p" "$GNB_TABLE" > "$D/$BASE.gnb_table.log" 2>/dev/null || true
      echo ">> saved gNB table: $D/$BASE.gnb_table.log ($(wc -l < "$D/$BASE.gnb_table.log") lines)"
    fi
    if [ -s "$RB" ]; then
      sudo cp "$RB" "$D/$BASE.rb.jsonl"; sudo chown "$USER" "$D/$BASE.rb.jsonl" 2>/dev/null || true
      gzip -9f "$D/$BASE.rb.jsonl"; echo ">> saved per-RB log: $D/$BASE.rb.jsonl.gz"
    fi
    if [ -s "$GL" ]; then
      sudo cp "$GL" "$D/$BASE.gnb_srs_test.log"; sudo chown "$USER" "$D/$BASE.gnb_srs_test.log" 2>/dev/null || true
      gzip -9f "$D/$BASE.gnb_srs_test.log"; echo ">> saved detail log: $D/$BASE.gnb_srs_test.log.gz"
    fi

    echo ""
    echo "============================================================"
    echo " ploss run complete -> datasets/Packet_loss/$NAME/"
    echo "   send these two for the ON-vs-OFF loss/throughput analysis:"
    echo "     $D/$BASE.swap.jsonl"
    echo "     $D/$BASE.gnb_table.log"
    echo " (gNB is still running — Ctrl-C it when you're done.)"
    echo "============================================================"
    ;;

  *) echo "usage: csi_run_ploss.sh start | run <name> [--rate 3M] [--dur 90] [--users N] [--table path] [note]"; exit 1 ;;
esac
