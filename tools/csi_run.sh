#!/bin/bash
# csi_run.sh — capture -> convert -> plot -> analyse -> git
#   csi_run.sh start                     (stop the gNB first!)
#   csi_run.sh save <name> ["note"]
set -e
RB=/tmp/srs_rb.jsonl            # per-RB producer log (~50/s)
SW=/tmp/swap_metrics.jsonl      # per-RU scheduler log (traffic-gated)
GC=/tmp/gnb_console.txt         # gNB console (CQI/RSRP/MCS via 't', needs 'script' launch)
GL=/tmp/gnb_srs_test.log        # gNB detail log (SRS/PHY/events, from yaml log sink)
REPO=/home/tud/OCUDU
RBS_PER_RU=${RBS_PER_RU:-12}
case "$1" in
  start)
    sudo rm -f "$RB" "$SW" "$GC" "$GL" 2>/dev/null || true
    echo "cleared logs — start the gNB (with 'script' for CQI) and run the experiment"
    ;;
  save)
    [ -z "$2" ] && { echo "usage: csi_run.sh save <name> [--mobile] [--speed V] [note]"; exit 1; }
    [ -s "$RB" ] || { echo "ERROR: $RB empty — did the gNB run with SRS enabled?"; exit 1; }
    NAME="$2"; shift 2
    SPEED=""; MOBILE=0
    while [ $# -gt 0 ]; do
      case "$1" in
        --mobile) MOBILE=1; shift ;;
        --speed)  SPEED="$2"; shift 2 ;;
        *) break ;;
      esac
    done
    NOTE="${1:-no note}"
    STAMP=$(date +%Y%m%d_%H%M); BASE="${STAMP}_$NAME"
    if [ "$MOBILE" = "1" ]; then SUB="Mobility"; else SUB="Static"; fi
    D="$REPO/datasets/$SUB/$NAME"; P="$D/plots"; mkdir -p "$P"
    echo "$NOTE" > "$D/$BASE.txt"

    # raw per-RB (archival)
    sudo cp "$RB" "$D/$BASE.rb.jsonl"; sudo chown "$USER" "$D/$BASE.rb.jsonl"; gzip -9f "$D/$BASE.rb.jsonl"

    # gNB logs (CQI/RSRP/MCS + events), if captured
    for lf in "$GC" "$GL"; do
      if [ -s "$lf" ]; then
        n="$D/$BASE.$(basename "$lf")"
        sudo cp "$lf" "$n"; sudo chown "$USER" "$n"; gzip -9f "$n"
      fi
    done

    # per-RU view
    zcat "$D/$BASE.rb.jsonl.gz" > /tmp/_rb.jsonl
    python3 "$REPO/tools/rb_to_ru.py" /tmp/_rb.jsonl -o "$D/$BASE.jsonl" --rbs-per-ru "$RBS_PER_RU" --skip-rb0
    rm -f /tmp/_rb.jsonl

    # plots + analysis
    python3 "$REPO/tools/plot_csi.py" "$D/$BASE.jsonl" -o "$P/$BASE.png"
    if [ -s "$SW" ]; then
      sudo cp "$SW" "$D/$BASE.swap.jsonl"; sudo chown "$USER" "$D/$BASE.swap.jsonl"
      python3 "$REPO/tools/plot_csi.py" "$D/$BASE.swap.jsonl" -o "$P/${BASE}_swap.png" || true
    fi
    python3 "$REPO/tools/analyze_position.py" "$D/$BASE.jsonl" \
            --csv "$REPO/datasets/campaign.csv" | tee "$P/${BASE}_analysis.txt"
    zcat "$D/$BASE.rb.jsonl.gz" > /tmp/_rbfull.jsonl
    if [ "$MOBILE" = "1" ]; then
      python3 "$REPO/tools/metrics_suite.py" /tmp/_rbfull.jsonl -o "$P/${BASE}_metrics.png" \
              ${SPEED:+--speed "$SPEED"} --avg-win 100 --trim-start 1.0 --trim-end 1.0 \
              --label "$BASE" \
              --mobile-csv "$REPO/datasets/mobility_sweep.csv" || true
    else
      python3 "$REPO/tools/metrics_suite.py" /tmp/_rbfull.jsonl -o "$P/${BASE}_metrics.png" \
              --trim-start 1.5 --trim-end 1.0 || true
    fi
    rm -f /tmp/_rbfull.jsonl

    # gNB console summary (CQI/RSRP/MCS distributions + events), if the log was archived
    if [ -s "$D/$BASE.gnb_console.txt.gz" ]; then
      python3 "$REPO/tools/gnb_log_summary.py" "$D/$BASE.gnb_console.txt.gz" \
              > "$P/${BASE}_gnb_summary.txt" 2>/dev/null || true
    fi

    cd "$REPO"
    git add "$D" datasets/campaign.csv datasets/mobility_sweep.csv tools/
    git commit -m "dataset: $BASE — $NOTE"
    git pull --rebase && git push && echo "PUSH OK" || { echo "PUSH FAILED — commit is local only"; exit 1; }
    echo "saved + pushed: $BASE"
    ;;
  *) echo "usage: csi_run.sh start | csi_run.sh save <name> [note]"; exit 1 ;;
esac
