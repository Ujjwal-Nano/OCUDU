#!/bin/bash
# csi_run.sh — capture -> convert -> plot -> analyse -> git
#   csi_run.sh start                     (stop the gNB first!)
#   csi_run.sh save <name> ["note"]
set -e
RB=/tmp/srs_rb.jsonl            # per-RB producer log (every SRS occasion, ~50/s)
SW=/tmp/swap_metrics.jsonl      # per-RU scheduler log (traffic-gated)
REPO=/home/tud/OCUDU
RBS_PER_RU=${RBS_PER_RU:-12}

case "$1" in
  start)
    sudo rm -f "$RB" "$SW" 2>/dev/null || true
    echo "cleared $RB and $SW — start the gNB and run the experiment now"
    ;;
  save)
    [ -z "$2" ] && { echo "usage: csi_run.sh save <name> [note]"; exit 1; }
    [ -s "$RB" ] || { echo "ERROR: $RB empty — did the gNB run with SRS enabled?"; exit 1; }
    STAMP=$(date +%Y%m%d_%H%M); BASE="${STAMP}_$2"
    D="$REPO/datasets"; P="$D/plots"; mkdir -p "$P"

    echo "${3:-no note}" > "$D/$BASE.txt"
    sudo cp "$RB" "$D/$BASE.rb.jsonl"; sudo chown "$USER" "$D/$BASE.rb.jsonl"
    gzip -9f "$D/$BASE.rb.jsonl"

    zcat "$D/$BASE.rb.jsonl.gz" > /tmp/_rb.jsonl
    python3 "$REPO/tools/rb_to_ru.py" /tmp/_rb.jsonl -o "$D/$BASE.jsonl" \
            --rbs-per-ru "$RBS_PER_RU" --skip-rb0
    rm -f /tmp/_rb.jsonl

    python3 "$REPO/tools/plot_csi.py" "$D/$BASE.jsonl" -o "$P/$BASE.png"
    python3 "$REPO/tools/analyze_position.py" "$D/$BASE.jsonl" \
            --csv "$D/campaign.csv" | tee "$P/${BASE}_analysis.txt"

    if [ -s "$SW" ]; then sudo cp "$SW" "$D/$BASE.swap.jsonl"; sudo chown "$USER" "$D/$BASE.swap.jsonl"; fi

    cd "$REPO"
    git add "datasets/$BASE."* "datasets/plots/$BASE"* datasets/campaign.csv
    git commit -m "dataset: $BASE — ${3:-no note}"
    git pull --rebase && git push && echo "PUSH OK" || { echo "PUSH FAILED — commit is local only"; exit 1; }
    echo "saved + pushed: $BASE"
    ;;
  *) echo "usage: csi_run.sh start | csi_run.sh save <name> [note]"; exit 1 ;;
esac
