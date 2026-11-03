#!/bin/bash
# swap_ab_run.sh  -  ONE command, run from Q957, for the swap ON/OFF experiment.
#
# Prereq: the yaml has  swap_ab_period: 100  under swap_sched:  AND the gNB was
# (re)started with it, so the swap actually toggles ON/OFF within the run.
#
# It (1) runs the logger on the CORE over SSH, (2) snapshots the gNB swap log
# (local on Q957) for exactly the run window, (3) pulls the CSV back to Q957,
# (4) runs swap_compare.py (ON-vs-OFF table + RTT CDF) and plot_perf.py
# (whole-run view). Everything lands in ~/datasets/packet_loss.
#
# Usage:
#   ./swap_ab_run.sh [ue_ip ...]                      # 3M, 120s, 4 UEs
#   RATE=5M DUR=120 NAME=ab_3M ./swap_ab_run.sh
#
# Env knobs:
#   RATE (3M)  DUR (120, >0)  NAME (label)  CORE (user@ip)  OUT (dir)
#   SWAPLOG (/tmp/swap_metrics.jsonl on Q957)   PLOT / CMP (script paths on Q957)
#   CLKOFF (0)  seconds added to swap-log time if core & gNB clocks differ
set -e

CORE=${CORE:-user@192.168.200.207}
OUT=${OUT:-$HOME/datasets/packet_loss}
PLOT=${PLOT:-$HOME/plot_perf.py}
CMP=${CMP:-$HOME/swap_compare.py}
SWAPLOG=${SWAPLOG:-/tmp/swap_metrics.jsonl}
RATE=${RATE:-3M}
DUR=${DUR:-120}
NAME=${NAME:-ab}
CLKOFF=${CLKOFF:-0}

UES=("$@")
[ ${#UES[@]} -eq 0 ] && UES=(10.45.0.11 10.45.0.12 10.45.0.13 10.45.0.14)

if ! [ "$DUR" -gt 0 ] 2>/dev/null; then
  echo "ERROR: DUR must be a positive number of seconds (got '$DUR')." >&2; exit 1
fi
[ -f "$SWAPLOG" ] || { echo "ERROR: swap log $SWAPLOG not found on this machine (is the gNB up here?)." >&2; exit 1; }
mkdir -p "$OUT"

STAMP=$(date +%Y%m%d_%H%M)
echo ">> core=$CORE  rate=$RATE/UE  dur=${DUR}s  ues=${UES[*]}"

# remember where the swap log is now, so we snapshot only the run window
SW_START=$(( $(wc -l < "$SWAPLOG") + 1 ))

echo ">> running logger on the core (live line below)..."
echo "------------------------------------------------------------"
ssh -t "$CORE" "RATE=$RATE DUR=$DUR python3 ~/dl_logger.py ${UES[*]}"
echo "------------------------------------------------------------"

# snapshot the swap-log slice covering this run (before any gNB restart wipes it)
SWSNAP="$OUT/${STAMP}_${NAME}.swap.jsonl"
tail -n +"$SW_START" "$SWAPLOG" > "$SWSNAP"
echo ">> swap-log snapshot: $SWSNAP ($(wc -l < "$SWSNAP") lines)"
if ! grep -q '"swap_enabled":0' "$SWSNAP"; then
  echo "   !! no swap_enabled:0 seen - the toggle did NOT run (swap_ab_period unset or gNB not restarted)." >&2
fi

echo ">> pulling CSV from the core..."
F=$(ssh "$CORE" 'ls -t ~/5g_perf_log_*.csv | head -1')
scp "$CORE:$F" "$OUT/"
CSV="$OUT/$(basename "$F")"
echo "   CSV: $CSV"

echo ">> swap ON/OFF comparison..."
python3 "$CMP" "$CSV" --swap "$SWSNAP" --out "$OUT" --clock-offset "$CLKOFF"

echo ">> whole-run view..."
python3 "$PLOT" "$CSV" --swap "$SWSNAP" --out "$OUT"

echo ">> done. files in: $OUT"
ls -t "$OUT"/*"$(basename "${F%.csv}")"* 2>/dev/null
