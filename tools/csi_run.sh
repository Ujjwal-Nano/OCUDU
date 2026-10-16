#!/bin/bash
# csi_run.sh — capture -> correlate (rnti->IMSI) -> convert -> plot -> analyse -> git
#   csi_run.sh start                 # clear logs BEFORE launching the gNB
#   csi_run.sh mark                  # after gNB up + all phones attached: reset data logs to "now"
#   csi_run.sh save <name> [--mobile] [--speed V] [--amf-host user@host] ["note"]
set -e
RB=/tmp/srs_rb.jsonl            # per-RB producer log (~50/s)
SW=/tmp/swap_metrics.jsonl      # per-RU scheduler log (traffic-gated)
GL=/tmp/gnb_srs_test.log        # gNB detail log (SRS/PHY/events, from yaml log sink)
RM=/tmp/rnti_map.jsonl          # gNB rnti->5G-S-TMSI map (Option B: RRC-setup tap)
STARTFILE=/tmp/csi_capture_start.txt
REPO=/home/tud/OCUDU
GNB_YAML="$REPO/configs/gnb_srs_test.yaml"
# Single source of truth: the gNB yaml decides the RU layout. Everything
# downstream (rb_to_ru, plots, metrics) inherits it, so you set it in ONE place.
# Override any of these with RBS_PER_RU=.. / NUM_USERS=.. / RUS_PER_USER=.. if needed.
RBS_PER_RU=${RBS_PER_RU:-$(grep -oP 'rbs_per_ru:\s*\K[0-9]+' "$GNB_YAML" 2>/dev/null | head -1)}
RBS_PER_RU=${RBS_PER_RU:-16}       # fallback if the yaml read fails
NUM_USERS=${NUM_USERS:-$(grep -oP 'num_users:\s*\K[0-9]+' "$GNB_YAML" 2>/dev/null | head -1)}
NUM_USERS=${NUM_USERS:-3}          # expected user count (for weakest-user min-users)
RUS_PER_USER=${RUS_PER_USER:-$(grep -oP 'rus_per_user:\s*\K[0-9]+' "$GNB_YAML" 2>/dev/null | head -1)}
RUS_PER_USER=${RUS_PER_USER:-1}    # RUs per user (R). Passed to the plots below, which handle R>=2.
                                   # Power metrics valid at any R; DL grants are contiguous, so
                                   # throughput at R>=2 is not claimed.
AMF_HOST=${AMF_HOST:-user@192.168.200.207}   # override per-call with --amf-host, or export AMF_HOST=

case "$1" in
  start)
    sudo rm -f "$RB" "$SW" "$GL" "$RM" 2>/dev/null || true
    date +"%Y-%m-%d %H:%M:%S" > "$STARTFILE"
    echo "cleared logs — now start the gNB and run the experiment"
    ;;

  mark)
    # Run AFTER the gNB is up and ALL phones have attached. The gNB already
    # holds the logs open, so we TRUNCATE in place (rm would orphan the fd).
    # Only the DATA logs (per-RB + swap) are reset, so the capture begins at
    # the all-attached moment with no startup transient. The rnti_map and gNB
    # log are KEPT on purpose — they hold the RRC-setup rnti->TMSI bindings
    # recorded DURING attach, which the IMSI correlation needs.
    for f in "$RB" "$SW"; do
      [ -e "$f" ] && sudo truncate -s 0 "$f"
    done
    date +"%Y-%m-%d %H:%M:%S" > "$STARTFILE"
    echo "marked: data logs (per-RB + swap) reset at all-attached point."
    echo "        rnti_map + gnb log kept for correlation. record the experiment now."
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
    EXPECT_RUS=$(( NUM_USERS * RUS_PER_USER ))
    echo "RU layout (from $GNB_YAML): rbs_per_ru=$RBS_PER_RU  num_users=$NUM_USERS  rus_per_user=$RUS_PER_USER  -> expect $EXPECT_RUS RU(s)"
    [ "$RUS_PER_USER" != "1" ] && echo "NOTE: rus_per_user=$RUS_PER_USER — power metrics (outage/quality/weakest) valid; DL grants are contiguous, so throughput at R>=2 is not claimed." >&2
    STAMP=$(date +%Y%m%d_%H%M); BASE="${STAMP}_$NAME"
    if [ "$MOBILE" = "1" ]; then SUB="Mobility"; else SUB="Static"; fi
    D="$REPO/datasets/$SUB/$NAME"; P="$D/plots"; mkdir -p "$P"
    echo "$NOTE" > "$D/$BASE.txt"

    # raw per-RB (archival)
    sudo cp "$RB" "$D/$BASE.rb.jsonl"; sudo chown "$USER" "$D/$BASE.rb.jsonl"; gzip -9f "$D/$BASE.rb.jsonl"

    # gNB detail log (SRS/PHY events), if captured — archived + used for AMF fallback
    if [ -s "$GL" ]; then
      n="$D/$BASE.$(basename "$GL")"
      sudo cp "$GL" "$n"; sudo chown "$USER" "$n"; gzip -9f "$n"
    fi

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
    # FALLBACK: the AMF-journal correlation, if no rnti-map exists.
    SESSIONS=""
    LABELMAP="$REPO/tools/tmsi_labels.json"
    LABELARG=""
    [ -s "$LABELMAP" ] && LABELARG="--label-map $LABELMAP"
    # Build the rnti->TMSI map from the gNB LOG, not the RRC-setup tap. The tap
    # reads the identity from the rrcSetupRequest, which is a RANDOM value when a
    # UE re-attaches after its registration lapsed (the RLF-churn case) — so those
    # c-rntis get no TMSI and all their samples are dropped. The rrcSetupComplete
    # NAS always carries the real M-TMSI, so rebuilding from the log captures every
    # setup. Falls back to the raw tap if the log rebuild yields nothing (e.g. RRC
    # debug logging was off).
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
      # AMF lookback: start the journal query a margin BEFORE capture-start so
      # a phone that registered early (before we cleared logs) still has its
      # TMSI->IMSI binding in the window. journalctl ignores extra history, so
      # erring early is free. 15 min margin by default.
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
      # RNTI -> TMSI (gNB, folds RRC reconnections) THEN TMSI -> IMSI (one AMF
      # lookup per distinct TMSI, folds re-registrations) -> group by IMSI.
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
        # graceful degrade: group by TMSI only (still folds reconnections)
        if python3 "$REPO/tools/correlate_rnti_imsi.py" \
              --rnti-map "$D/$BASE.rnti_map.jsonl" \
              $LABELARG \
              --group-out "$D/$BASE.ue_sessions.json"; then
          SESSIONS="$D/$BASE.ue_sessions.json"
          echo "correlation: Option B (rnti->TMSI only, AMF unreachable — TMSI keys)"
        fi
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

    # Drop unresolved user keys: a real user is a 15-digit IMSI. Any TMSI-hex
    # key (e.g. "40c000021a" from a brief blip whose IMSI the AMF didn't bind)
    # is removed so it is never plotted or counted. Runs only when AMF
    # resolution produced IMSI keys; if nothing survives, keeps the original.
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

    # per-RU view — split by real user (IMSI) whenever correlation succeeded,
    # otherwise same single-user (u=0) output as before
    zcat "$D/$BASE.rb.jsonl.gz" > /tmp/_rb.jsonl
    python3 "$REPO/tools/rb_to_ru.py" /tmp/_rb.jsonl -o "$D/$BASE.jsonl" \
            --rbs-per-ru "$RBS_PER_RU" --skip-rb0 \
            ${SESSIONS:+--sessions "$SESSIONS"}
    rm -f /tmp/_rb.jsonl

    # CSI plot — plot_csi.py draws one row per distinct "u", so it's
    # automatically multi-user once rb_to_ru.py resolved IMSIs.
    python3 "$REPO/tools/plot_csi.py" "$D/$BASE.jsonl" -o "$P/$BASE.png" --rbs-per-ru "$RBS_PER_RU"
    if [ -s "$SW" ]; then
      sudo cp "$SW" "$D/$BASE.swap.jsonl"; sudo chown "$USER" "$D/$BASE.swap.jsonl"
      python3 "$REPO/tools/plot_csi.py" "$D/$BASE.swap.jsonl" -o "$P/${BASE}_swap.png" --rbs-per-ru "$RBS_PER_RU" || true
    fi

    # metrics suite (per-user via --sessions, yaml RU size)
    zcat "$D/$BASE.rb.jsonl.gz" > /tmp/_rbfull.jsonl
    if [ "$MOBILE" = "1" ]; then
      python3 "$REPO/tools/metrics_suite.py" /tmp/_rbfull.jsonl -o "$P/${BASE}_metrics.png" \
              ${SESSIONS:+--sessions "$SESSIONS"} \
              --K "$RBS_PER_RU" \
              ${SPEED:+--speed "$SPEED"} --avg-win 40 --trim-start 1.0 --trim-end 1.0 \
              --label "$BASE" \
              --mobile-csv "$REPO/datasets/mobility_sweep.csv" || true
    else
      python3 "$REPO/tools/metrics_suite.py" /tmp/_rbfull.jsonl -o "$P/${BASE}_metrics.png" \
              ${SESSIONS:+--sessions "$SESSIONS"} \
              --K "$RBS_PER_RU" \
              --trim-start 1.5 --trim-end 1.0 || true
    fi
    rm -f /tmp/_rbfull.jsonl

    # ---- swap-benefit, weakest-user, outage, decision-quality, RBG-power figures ----
    # swap_analysis + plot_weakest + plot_outage + swap_decision_quality read the
    # per-RU scheduler log (swap.jsonl, only present when the swap ran with traffic);
    # plot_rbg_power reads the raw per-RB log. outage/quality are aggregate
    # weakest-user metrics (no per-user labels), so they take --min-users but not
    # --sessions. Each also writes a .txt sidecar with the numbers.
    if [ -s "$D/$BASE.swap.jsonl" ]; then

      # weakest-user outage CDF (the paper's benefit metric, eq 13)
      python3 "$REPO/tools/plot_outage.py" "$D/$BASE.swap.jsonl" \
              -o "$P/${BASE}_outage.png" --min-users "${NUM_USERS:-3}" \
              --rus-per-user "$RUS_PER_USER" --label "$BASE" || true
      # decision quality: optimality (vs per-slot max-min) + alloc-time cost
      python3 "$REPO/tools/swap_decision_quality.py" "$D/$BASE.swap.jsonl" \
              -o "$P/${BASE}_quality.png" --min-users "${NUM_USERS:-3}" \
              --rus-per-user "$RUS_PER_USER" --label "$BASE" || true
              
      # weakest-user outage vs swap depth epsilon (offline replay, eps=1..4)
      python3 "$REPO/tools/plot_outage_epsilon.py" "$D/$BASE.swap.jsonl" \
              -o "$P/${BASE}_outage_epssweep.png" --min-users "${NUM_USERS:-3}" \
              --eps-list 1,2,3,4 --label "$BASE" || true
              
      python3 "$REPO/tools/compare_algorithms.py" "$D/$BASE.swap.jsonl" \
        -o "$P/${BASE}_algocmp" --min-users "${NUM_USERS:-3}" --label "$BASE" || true
        
    fi
    python3 "$REPO/tools/plot_rbg_power.py" "$D/$BASE.rb.jsonl.gz" \
            -o "$P/${BASE}_rbgpower.png" --K "$RBS_PER_RU" \
            ${SESSIONS:+--sessions "$SESSIONS"} || true

    cd "$REPO"
    # Keep large / raw / derivable artifacts LOCAL-ONLY. GitHub hard-rejects any
    # file >100 MB (the full PHY log), and the raw per-RB capture (rb.jsonl.gz)
    # plus its expanded per-RU .jsonl are bulky and regenerable/archival — so
    # none of them belong in git. Only the small products (plots, txt, sessions
    # json, rnti map, mobility csv) stay staged.
    git reset -q -- "$D/$BASE.rb.jsonl.gz" \
                    "$D/$BASE.jsonl" \
                    "$D/$BASE.gnb_srs_test.log.gz" \
                    "$D/$BASE.swap.jsonl" 2>/dev/null || true
    git commit -m "dataset: $BASE — $NOTE"
    # --autostash: reapply any unstaged edits (e.g. a tweaked configs/*.yaml)
    # around the rebase, so a dirty working tree no longer blocks the push.
    git pull --rebase --autostash && git push && echo "PUSH OK" || { echo "PUSH FAILED — commit is local only"; exit 1; }
    echo "saved + pushed: $BASE"
    ;;

  *) echo "usage: csi_run.sh start | csi_run.sh mark | csi_run.sh save <name> [note]"; exit 1 ;;
esac
