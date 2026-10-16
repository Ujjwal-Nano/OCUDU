#!/bin/bash
# csi_run_swap.sh — SWAP-TEST pipeline: capture -> correlate -> convert ->
#                   per-user CSI + swapping analyses -> git.
# Same format as csi_run.sh, trimmed to the swap study:
#   KEEP : plot_csi (CSI power per user), plot_outage, plot_outage_epsilon,
#          swap_decision_quality, compare_algorithms (paper baselines vs swap).
#   DROP : metrics_suite, swap_analysis, plot_weakest, plot_rbg_power,
#          and the swap.jsonl CSI plot.
# Datasets land in datasets/Swap_test/<name> (not Static/Mobility).
#   csi_run_swap.sh start                 # clear logs BEFORE launching the gNB
#   csi_run_swap.sh mark                  # after gNB up + all phones attached: reset data logs
#   csi_run_swap.sh save <name> [--amf-host user@host] ["note"]
set -e
RB=/tmp/srs_rb.jsonl            # per-RB producer log (~50/s)
SW=/tmp/swap_metrics.jsonl      # per-RU scheduler log (traffic-gated)
GL=/tmp/gnb_srs_test.log        # gNB detail log (SRS/PHY/events, from yaml log sink)
RM=/tmp/rnti_map.jsonl          # gNB rnti->5G-S-TMSI map (Option B: RRC-setup tap)
STARTFILE=/tmp/csi_capture_start.txt
REPO=/home/tud/OCUDU
GNB_YAML="$REPO/configs/gnb_srs_test.yaml"
# Single source of truth: the gNB yaml decides the RU layout. Everything
# downstream (rb_to_ru, plots) inherits it, so you set it in ONE place.
RBS_PER_RU=${RBS_PER_RU:-$(grep -oP 'rbs_per_ru:\s*\K[0-9]+' "$GNB_YAML" 2>/dev/null | head -1)}
RBS_PER_RU=${RBS_PER_RU:-16}       # fallback if the yaml read fails
NUM_USERS=${NUM_USERS:-$(grep -oP 'num_users:\s*\K[0-9]+' "$GNB_YAML" 2>/dev/null | head -1)}
NUM_USERS=${NUM_USERS:-3}          # expected user count (for weakest-user min-users)
RUS_PER_USER=${RUS_PER_USER:-$(grep -oP 'rus_per_user:\s*\K[0-9]+' "$GNB_YAML" 2>/dev/null | head -1)}
RUS_PER_USER=${RUS_PER_USER:-1}    # RUs per user (R). Tools handle R>=1 (per-user power =
                                   # sum of CSI over the user's RUs). Power valid at any R.
AMF_HOST=${AMF_HOST:-user@192.168.200.207}   # override per-call with --amf-host, or export AMF_HOST=

case "$1" in
  start)
    sudo rm -f "$RB" "$SW" "$GL" "$RM" 2>/dev/null || true
    date +"%Y-%m-%d %H:%M:%S" > "$STARTFILE"
    echo "cleared logs — now start the gNB and run the swap experiment"
    ;;

  mark)
    # Run AFTER the gNB is up and ALL phones have attached. Truncate in place
    # (the gNB holds the fds). Only the DATA logs (per-RB + swap) are reset;
    # rnti_map + gNB log are kept for the IMSI correlation.
    for f in "$RB" "$SW"; do
      [ -e "$f" ] && sudo truncate -s 0 "$f"
    done
    date +"%Y-%m-%d %H:%M:%S" > "$STARTFILE"
    echo "marked: data logs (per-RB + swap) reset at all-attached point."
    echo "        rnti_map + gnb log kept for correlation. record the experiment now."
    ;;

  save)
    [ -z "$2" ] && { echo "usage: csi_run_swap.sh save <name> [--amf-host user@host] [note]"; exit 1; }
    [ -s "$RB" ] || { echo "ERROR: $RB empty — did the gNB run with SRS enabled?"; exit 1; }
    [ -s "$SW" ] || echo "WARNING: $SW empty — swap log not produced (did the swap run WITH traffic?). Swapping plots will be skipped." >&2
    NAME="$2"; shift 2
    while [ $# -gt 0 ]; do
      case "$1" in
        --amf-host) AMF_HOST="$2"; shift 2 ;;
        *) break ;;
      esac
    done
    NOTE="${1:-no note}"
    EXPECT_RUS=$(( NUM_USERS * RUS_PER_USER ))
    echo "RU layout (from $GNB_YAML): rbs_per_ru=$RBS_PER_RU  num_users=$NUM_USERS  rus_per_user=$RUS_PER_USER  -> expect $EXPECT_RUS RU(s)"
    STAMP=$(date +%Y%m%d_%H%M); BASE="${STAMP}_$NAME"
    SUB="Swap_test"                                   # swap study -> dedicated folder
    D="$REPO/datasets/$SUB/$NAME"; P="$D/plots"; mkdir -p "$P"
    echo "$NOTE" > "$D/$BASE.txt"

    # raw per-RB (archival, local-only)
    sudo cp "$RB" "$D/$BASE.rb.jsonl"; sudo chown "$USER" "$D/$BASE.rb.jsonl"; gzip -9f "$D/$BASE.rb.jsonl"

    # gNB detail log (SRS/PHY events), if captured — archived + used for AMF fallback
    if [ -s "$GL" ]; then
      n="$D/$BASE.$(basename "$GL")"
      sudo cp "$GL" "$n"; sudo chown "$USER" "$n"; gzip -9f "$n"
    fi

    # ---- rnti -> user correlation (for per-user CSI labels) ----
    SESSIONS=""
    LABELMAP="$REPO/tools/tmsi_labels.json"
    LABELARG=""
    [ -s "$LABELMAP" ] && LABELARG="--label-map $LABELMAP"
    # Rebuild rnti->TMSI from the gNB LOG (captures every RRC setup, incl. the
    # random-identity re-attaches). Falls back to the raw RRC-setup tap.
    GLGZ="$D/$BASE.gnb_srs_test.log.gz"
    if [ -s "$GLGZ" ]; then
      python3 "$REPO/tools/rnti_map_from_gnblog.py" "$GLGZ" \
              -o "$D/$BASE.rnti_map.jsonl" 2>&1 | sed 's/^/  [rnti-map] /' || true
    fi
    if [ ! -s "$D/$BASE.rnti_map.jsonl" ] && [ -s "$RM" ]; then
      echo "  [rnti-map] log rebuild empty — falling back to RRC-setup tap" >&2
      sudo cp "$RM" "$D/$BASE.rnti_map.jsonl"; sudo chown "$USER" "$D/$BASE.rnti_map.jsonl"
    fi
    if [ -s "$D/$BASE.rnti_map.jsonl" ]; then
      AMF_SINCE=$(cat "$STARTFILE" 2>/dev/null || echo "")
      if [ -n "$AMF_SINCE" ]; then
        _st_epoch=$(date -d "$AMF_SINCE" +%s 2>/dev/null || echo "")
        if [ -n "$_st_epoch" ]; then
          AMF_LOOKBACK=$(date -d "@$((_st_epoch - 900))" +"%Y-%m-%d %H:%M:%S")
        else
          AMF_LOOKBACK=$(date -d "30 minutes ago" +"%Y-%m-%d %H:%M:%S")
        fi
      else
        AMF_LOOKBACK=$(date -d "30 minutes ago" +"%Y-%m-%d %H:%M:%S")
      fi
      AMF_UNTIL=$(date +"%Y-%m-%d %H:%M:%S")
      if python3 "$REPO/tools/correlate_rnti_imsi.py" \
            --rnti-map "$D/$BASE.rnti_map.jsonl" \
            --amf-since "$AMF_LOOKBACK" --amf-until "$AMF_UNTIL" \
            --amf-host "$AMF_HOST" \
            $LABELARG \
            --group-out "$D/$BASE.ue_sessions.json"; then
        SESSIONS="$D/$BASE.ue_sessions.json"
        echo "correlation: Option B (rnti->TMSI gNB, TMSI->IMSI via AMF, grouped by IMSI)"
      else
        echo "WARNING: rnti-map+AMF correlation failed — retrying rnti-map alone (TMSI keys)" >&2
        if python3 "$REPO/tools/correlate_rnti_imsi.py" \
              --rnti-map "$D/$BASE.rnti_map.jsonl" \
              $LABELARG \
              --group-out "$D/$BASE.ue_sessions.json"; then
          SESSIONS="$D/$BASE.ue_sessions.json"
          echo "correlation: Option B (rnti->TMSI only, AMF unreachable — TMSI keys)"
        fi
      fi
    fi
    if [ -z "$SESSIONS" ]; then
      echo "WARNING: no correlation — per-user CSI will fall back to single-user (u=0) labels." >&2
    fi

    # Drop unresolved (non-IMSI) user keys so only real 15-digit IMSI users remain.
    if [ -n "$SESSIONS" ]; then
      python3 -c "
import json,sys
d=json.load(open('$SESSIONS'))
users=d['users'] if isinstance(d,dict) and isinstance(d.get('users'),dict) else d
clean={k:v for k,v in users.items() if str(k).isdigit() and len(str(k))==15}
dropped=set(users)-set(clean)
if dropped: print('dropped unresolved (non-IMSI) user key(s):', ', '.join(sorted(dropped)), file=sys.stderr)
if clean: json.dump(clean, open('$SESSIONS','w'), indent=2)
else: print('WARNING: no IMSI users after filter; keeping original sessions', file=sys.stderr)
" || true
    fi

    # per-RU view (for the CSI plot), split by real user when correlation succeeded
    zcat "$D/$BASE.rb.jsonl.gz" > /tmp/_rb.jsonl
    python3 "$REPO/tools/rb_to_ru.py" /tmp/_rb.jsonl -o "$D/$BASE.jsonl" \
            --rbs-per-ru "$RBS_PER_RU" --skip-rb0 \
            ${SESSIONS:+--sessions "$SESSIONS"}
    rm -f /tmp/_rb.jsonl

    # ---- (1) CSI power per user ----
    python3 "$REPO/tools/plot_csi.py" "$D/$BASE.jsonl" -o "$P/$BASE.png" --rbs-per-ru "$RBS_PER_RU"

    # copy the swap log — input to every swapping analysis below
    if [ -s "$SW" ]; then
      sudo cp "$SW" "$D/$BASE.swap.jsonl"; sudo chown "$USER" "$D/$BASE.swap.jsonl"
    fi

    # ---- (2-5) swapping analyses (swap.jsonl only; auto-read policy/rus_per_user/epsilon) ----
    if [ -s "$D/$BASE.swap.jsonl" ]; then
      # (2) weakest-user outage CDF (best/worst fixed, random, swap, optimal)
      python3 "$REPO/tools/plot_outage.py" "$D/$BASE.swap.jsonl" \
              -o "$P/${BASE}_outage.png" --min-users "${NUM_USERS:-3}" \
              --rus-per-user "$RUS_PER_USER" --label "$BASE" || true
      # (3) outage vs swap depth epsilon (offline replay eps=1..4; needs swap_policies.py)
      python3 "$REPO/tools/plot_outage_epsilon.py" "$D/$BASE.swap.jsonl" \
              -o "$P/${BASE}_outage_epssweep.png" --min-users "${NUM_USERS:-3}" \
              --rus-per-user "$RUS_PER_USER" --eps-list 1,2,3,4 --label "$BASE" || true
      # (4) decision quality: optimality (vs per-slot max-min) + alloc-time cost
      python3 "$REPO/tools/swap_decision_quality.py" "$D/$BASE.swap.jsonl" \
              -o "$P/${BASE}_quality.png" --min-users "${NUM_USERS:-3}" \
              --rus-per-user "$RUS_PER_USER" --label "$BASE" || true
      # (5) paper baselines vs swap: floors (offline) + decision cost (gNB C++ time)
      python3 "$REPO/tools/compare_algorithms.py" "$D/$BASE.swap.jsonl" \
              -o "$P/${BASE}_algocmp" --min-users "${NUM_USERS:-3}" --label "$BASE" || true
    fi

    cd "$REPO"
    git add "$D" tools/
    # Keep large / raw / derivable artifacts LOCAL-ONLY (GitHub rejects >100 MB;
    # the raw captures and per-slot jsonls are bulky and regenerable). Only the
    # small products (plots, txt, sessions json, rnti map) stay staged.
    git reset -q -- "$D/$BASE.rb.jsonl.gz" \
                    "$D/$BASE.jsonl" \
                    "$D/$BASE.gnb_srs_test.log.gz" \
                    "$D/$BASE.swap.jsonl" \
                    "$P/${BASE}_algocmp.jsonl" 2>/dev/null || true
    git commit -m "swap dataset: $BASE — $NOTE"
    git pull --rebase --autostash && git push && echo "PUSH OK" || { echo "PUSH FAILED — commit is local only"; exit 1; }
    echo "saved + pushed: $BASE  (datasets/$SUB/$NAME)"
    ;;

  *) echo "usage: csi_run_swap.sh start | csi_run_swap.sh mark | csi_run_swap.sh save <name> [note]"; exit 1 ;;
esac
