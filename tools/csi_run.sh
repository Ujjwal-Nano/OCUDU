#!/bin/bash
# csi_run.sh — systematic CSI capture -> git -> plot workflow
# usage:  csi_run.sh start                      (begin a fresh recording)
#         csi_run.sh save <name> ["note text"]  (snapshot, plot, commit, push)
set -e
LOG=/tmp/swap_metrics.jsonl
REPO=~/OCUDU
case "$1" in
  start)
    : > "$LOG"
    echo "recording started fresh: $LOG (run your experiment now)"
    ;;
  save)
    [ -z "$2" ] && { echo "usage: csi_run.sh save <name> [note]"; exit 1; }
    [ -s "$LOG" ] || { echo "ERROR: $LOG is empty — nothing recorded"; exit 1; }
    STAMP=$(date +%Y%m%d_%H%M)
    BASE="${STAMP}_$2"
    cp "$LOG" "$REPO/datasets/$BASE.jsonl"
    echo "${3:-no note}" > "$REPO/datasets/$BASE.txt"
    python3 "$REPO/tools/plot_csi.py" "$REPO/datasets/$BASE.jsonl" \
            -o "$REPO/datasets/plots/$BASE.png"
    cd "$REPO"
    git add "datasets/$BASE.jsonl" "datasets/$BASE.txt" "datasets/plots/$BASE.png"
    git commit -m "dataset: $BASE — ${3:-no note}"
    git push
    echo "saved + pushed: datasets/$BASE.jsonl  (+ note, + plot)"
    ;;
  *)
    echo "usage: csi_run.sh start | csi_run.sh save <name> [note]"; exit 1 ;;
esac
