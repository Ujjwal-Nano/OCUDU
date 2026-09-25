#!/bin/bash
# csi_run.sh — capture -> correlate (rnti->IMSI) -> convert -> plot -> analyse -> git
#   csi_run.sh start                                    (stop the gNB first!)
#   csi_run.sh save <name> [--mobile] [--speed V] [--amf-host user@host] ["note"]
set -e
RB=/tmp/srs_rb.jsonl            # per-RB producer log (~50/s)
SW=/tmp/swap_metrics.jsonl      # per-RU scheduler log (traffic-gated)
GC=/tmp/gnb_console.txt         # gNB console (CQI/RSRP/MCS via 't', needs 'script' launch)
GL=/tmp/gnb_srs_test.log        # gNB detail log (SRS/PHY/events, from yaml log sink)
RM=/tmp/rnti_map.jsonl          # gNB rnti->5G-S-TMSI map (Option B: RRC-setup tap)
STARTFILE=/tmp/csi_capture_start.txt
REPO=/home/tud/OCUDU
RBS_PER_RU=${RBS_PER_RU:-16}
AMF_HOST=${AMF_HOST:-user@192.168.200.207}   # override per-call with --amf-host, or export AMF_HOST=

case "$1" in
  start)
    sudo rm -f "$RB" "$SW" "$GC" "$GL" "$RM" 2>/dev/null || true
    date +"%Y-%m-%d %H:%M:%S" > "$STARTFILE"
    echo "cleared logs — start the gNB (with 'script' for CQI) and run the experiment"
    ;;
  save)
    [ -z "$2" ] && { echo "usage: csi_run.sh save <name> [--mobile] [--speed V] [--amf-host user@host] [note]"; exit 1; }
    [ -s "$RB" ] || { echo "ERROR: $RB empty — did the gNB run with SRS enabled?"; exit 1; }
    NAME="$2"; shift 2
    SPEED=""; MOBILE=0
    while [ $# -gt 0 ]; do
      case "$1" in
        --mobile)   MOBILE=1; shift ;;
        --speed)    SPEED="$2"; shift 2 ;;
        --amf-host) AMF_HOST="$2"; shift 2 ;;
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

    # ---- rnti -> user correlation ----
    # Produces ue_sessions.json (user -> its rnti sessions this run). This is
    # what lets rb_to_ru.py / metrics label each record with its real user
    # instead of "user 0", and folds a phone's reconnection RNTIs into one user.
    #
    # OPTION B (preferred): if the gNB wrote /tmp/rnti_map.jsonl (RRC-setup tap
    # logging rnti->5G-S-TMSI), build sessions straight from it — no AMF query,
    # no clock skew, every reconnection captured. Users are keyed by 5G-S-TMSI;
    # drop a labels.json ({stmsi_hex: imsi}) at $REPO/tools/tmsi_labels.json to
    # relabel them to real IMSIs.
    # FALLBACK: the original AMF-journal correlation, if no rnti-map exists.
    SESSIONS=""
    LABELMAP="$REPO/tools/tmsi_labels.json"
    LABELARG=""
    [ -s "$LABELMAP" ] && LABELARG="--label-map $LABELMAP"
    if [ -s "$RM" ]; then
      sudo cp "$RM" "$D/$BASE.rnti_map.jsonl"; sudo chown "$USER" "$D/$BASE.rnti_map.jsonl"
      if python3 "$REPO/tools/correlate_rnti_imsi.py" \
            --rnti-map "$D/$BASE.rnti_map.jsonl" \
            $LABELARG \
            --group-out "$D/$BASE.ue_sessions.json"; then
        SESSIONS="$D/$BASE.ue_sessions.json"
        echo "correlation: Option B (gNB rnti-map, no AMF)"
      else
        echo "WARNING: rnti-map correlation failed — falling back to AMF" >&2
      fi
    fi
    if [ -z "$SESSIONS" ] && [ -s "$GL" ]; then
      AMF_SINCE=$(cat "$STARTFILE" 2>/dev/null || echo "")
      AMF_UNTIL=$(date +"%Y-%m-%d %H:%M:%S")
      if [ -n "$AMF_SINCE" ]; then
        if python3 "$REPO/tools/correlate_rnti_imsi.py" \
              --gnb-log "$GL" \
              --amf-since "$AMF_SINCE" --amf-until "$AMF_UNTIL" \
              --amf-host "$AMF_HOST" \
              -o "$D/$BASE.rnti_imsi_map.csv" \
              --group-out "$D/$BASE.ue_sessions.json"; then
          SESSIONS="$D/$BASE.ue_sessions.json"
          echo "correlation: AMF journal (fallback)"
        else
          echo "WARNING: rnti/IMSI correlation failed against $AMF_HOST — falling back to single-user (u=0) output" >&2
        fi
      else
        echo "WARNING: no capture-start timestamp in $STARTFILE — run 'csi_run.sh start' first for per-user correlation. Falling back to single-user (u=0) output." >&2
      fi
    elif [ -z "$SESSIONS" ]; then
      echo "WARNING: no rnti-map and $GL not captured — can't correlate. Falling back to single-user (u=0) output." >&2
    fi

    # per-RU view — split by real user (IMSI) whenever correlation succeeded,
    # otherwise same single-user (u=0) output as before
    zcat "$D/$BASE.rb.jsonl.gz" > /tmp/_rb.jsonl
    python3 "$REPO/tools/rb_to_ru.py" /tmp/_rb.jsonl -o "$D/$BASE.jsonl" \
            --rbs-per-ru "$RBS_PER_RU" --skip-rb0 \
            ${SESSIONS:+--sessions "$SESSIONS"}
    rm -f /tmp/_rb.jsonl

    # plots + analysis — plot_csi.py already draws one row per distinct "u",
    # so this is automatically multi-user once rb_to_ru.py resolved IMSIs;
    # no per-user loop needed here or anywhere else in this script.
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
              ${SESSIONS:+--sessions "$SESSIONS"} \
              ${SPEED:+--speed "$SPEED"} --avg-win 40 --trim-start 1.0 --trim-end 1.0 \
              --label "$BASE" \
              --mobile-csv "$REPO/datasets/mobility_sweep.csv" || true
    else
      python3 "$REPO/tools/metrics_suite.py" /tmp/_rbfull.jsonl -o "$P/${BASE}_metrics.png" \
              ${SESSIONS:+--sessions "$SESSIONS"} \
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
    # Keep large / raw / derivable artifacts LOCAL-ONLY. GitHub hard-rejects any
    # file >100 MB (the full PHY log), and the raw per-RB capture (rb.jsonl.gz)
    # plus its expanded per-RU .jsonl are bulky and regenerable/archival — so
    # none of them belong in git. Only the small products (plots, txt, sessions
    # json, rnti map, campaign/mobility csvs) stay staged.
    git reset -q -- "$D/$BASE.rb.jsonl.gz" \
                    "$D/$BASE.jsonl" \
                    "$D/$BASE.gnb_srs_test.log.gz" \
                    "$D/$BASE.gnb_console.txt.gz" \
                    "$D/$BASE.swap.jsonl" 2>/dev/null || true
    git commit -m "dataset: $BASE — $NOTE"
    # --autostash: reapply any unstaged edits (e.g. a tweaked configs/*.yaml)
    # around the rebase, so a dirty working tree no longer blocks the push.
    git pull --rebase --autostash && git push && echo "PUSH OK" || { echo "PUSH FAILED — commit is local only"; exit 1; }
    echo "saved + pushed: $BASE"
    ;;
  *) echo "usage: csi_run.sh start | csi_run.sh save <name> [note]"; exit 1 ;;
esac
