# OCUDU gNB Operating Guide — Complete Runbook

Every command, in order, for running the testbed and capturing data.
Machines: [gNB] = Fujitsu Q957,  [core] = ThinkPad core laptop.

===============================================================================
## 0. PER-BOOT SETUP
===============================================================================

### [core] core + internet forwarding (RAM-only, re-apply every boot)
    systemctl status open5gs-amfd open5gs-upfd --no-pager | grep Active
    # if dead:
    sudo systemctl restart open5gs-nrfd open5gs-scpd open5gs-amfd open5gs-ausfd \
        open5gs-udmd open5gs-udrd open5gs-pcfd open5gs-bsfd open5gs-smfd open5gs-upfd
    # forwarding + NAT (<iface> = core internet-facing iface from `ip route`):
    sudo sysctl -w net.ipv4.ip_forward=1
    sudo iptables -t nat -C POSTROUTING -s 10.45.0.0/16 ! -o <iface> -j MASQUERADE 2>/dev/null || \
    sudo iptables -t nat -A POSTROUTING -s 10.45.0.0/16 ! -o <iface> -j MASQUERADE
    sudo iptables -C FORWARD -j ACCEPT 2>/dev/null || sudo iptables -I FORWARD 1 -j ACCEPT

### [gNB] pre-flight
    ping -c2 192.168.200.207                          # core reachable
    uhd_usrp_probe 2>&1 | grep -iE "ref|clock|mboard" # USRP + 10 MHz ref

===============================================================================
## 1. START THE gNB
===============================================================================
    sudo pkill -9 -f gnb ; sleep 2
    ps aux | grep -v grep | grep gnb                  # confirm none running
    cd ~/OCUDU/build/apps/gnb
    sudo script -f /tmp/gnb_console.txt -c "./gnb -c /home/tud/OCUDU/configs/gnb_srs_test.yaml"
    # wait for "==== gNB started ===", then press:  t

===============================================================================
## 2. ATTACH PHONE + KEEP-ALIVE (prevents ~3 min idle drop)
===============================================================================
    # [phone] charger in, stay-awake on, mobile data on, network 99942, load a page.
    # [core] find IP, run keep-alive for WHOLE capture:
    sudo grep -iE "10\.45\." /var/log/open5gs/smf.log | tail -1
    ping -i 0.3 10.45.0.X            # LEAVE RUNNING (X = phone IP)

===============================================================================
## 3. VERIFY HEALTHY (before a 20-min run)
===============================================================================
    wc -l /tmp/srs_rb.jsonl ; sleep 30 ; wc -l /tmp/srs_rb.jsonl   # +~1500 in 30s
    grep -oE "rnti=0x[0-9a-f]+" /tmp/gnb_srs_test.log | sort | uniq -c   # ONE rnti
    # link SNR:
    grep "SRS:" /tmp/gnb_srs_test.log | tail -50 | \
      grep -oE "epre=(-?[0-9.]+)dB.*noise_var=(-?[0-9.]+)dB" | \
      sed -E 's/epre=(-?[0-9.]+)dB.*noise_var=(-?[0-9.]+)dB/\1 \2/' | \
      awk '{d=$1-$2;s+=d;n++} END{print "epre-noise_var mean:",s/n,"dB"}'

===============================================================================
## 4. CAPTURE
===============================================================================
    # STEP 1: stop gNB, clear logs, restart gNB, re-attach phone+keepalive
    sudo pkill -9 -f gnb ; sleep 2
    ~/OCUDU/tools/csi_run.sh start
    # (relaunch gNB section 1, re-attach section 2)

    # ... run experiment ~20 min, keep-alive running, RNTI single ...

    # STEP 2: SAVE  (convert+plot+analyse+commit+push)
    # static -> datasets/Static/ :
    ~/OCUDU/tools/csi_run.sh save <ID> "distance, obstruction, room"
    # mobility -> datasets/Mobility/, 100ms window, appends mobility_sweep.csv:
    ~/OCUDU/tools/csi_run.sh save <ID> --mobile --speed 0.2 --duration 20 "0.2 m/s, 1m, rx50"
    # order:  save <ID> [--mobile] [--speed V] [--duration N] "note"

===============================================================================
## 5. STOP
===============================================================================
    # Ctrl+C in gNB terminal, then:
    sudo pkill -f gnb
    # Ctrl+C the keep-alive on [core]

===============================================================================
## 6. ANALYSIS (offline)
===============================================================================
### file -> tool:
###   per-RU .jsonl       -> plot_csi.py, analyze_position.py
###   raw .rb.jsonl.gz    -> metrics_suite.py, swap_value.py, mobility_tc.py
###   gnb_console.txt.gz  -> gnb_log_summary.py
###   gnb_srs_test.log.gz -> srs_snr.py, srs_snr_plot.py

    B=$(ls ~/OCUDU/datasets/Mobility/<ID>/*.rb.jsonl.gz | head -1)

    # plateau (window-independent) Tc:
    python3 ~/OCUDU/tools/mobility_tc.py "$B" --speed <V>

    # metrics suite (mobility):
    python3 ~/OCUDU/tools/metrics_suite.py "$B" -o out.png \
        --speed <V> --avg-win 40 --trim-start 1.0 --trim-end 1.0 --mobile-csv <csv>

    # window-stability check:
    for w in 20 30 40 60 80 100 150 200; do echo -n "win ${w}ms: "; \
      python3 ~/OCUDU/tools/metrics_suite.py "$B" -o /tmp/x.png --speed <V> --avg-win $w \
      | grep "Tc(0.5)"; done

    # truncate a too-long capture:
    python3 ~/OCUDU/tools/truncate_capture.py in.rb.jsonl.gz out.rb.jsonl.gz <cutoff_min>

    # sweep plots:
    python3 ~/OCUDU/tools/plot_mobility_sweep.py ~/OCUDU/datasets/mobility_sweep.csv \
        -o ~/OCUDU/datasets/plots/mobility_sweep.png

===============================================================================
## 7. TRAPS (learned the hard way)
===============================================================================
- sudo + ~ : ~ becomes /root under sudo. Use ABSOLUTE config path in `script -c`.
- Phone drops at ~3 min = idle RRC timeout. Fix: keep-alive ping + charger + stay-awake.
  Two RNTIs in log = it dropped/re-attached.
- NAT is RAM-only: re-apply section 0 every core boot.
- Reused capture ID = two captures in one folder. Use a UNIQUE ID each time.
- --mobile auto-places in Mobility/, plain save in Static/. Don't mkdir manually.
- Bracketed-paste (`[200~` in files): run  printf '\e[?2004l'  before pasting heredocs,
  or restore from git and patch instead of re-pasting long files.
- git divergence from browser uploads: git pull --rebase before push.
- Tc window bias: never fix a window blindly; at high speed 100ms inflates Tc.
  Use mobility_tc.py plateau or a window shorter than Tc.
- gNB won't start without the Meinberg 10 MHz external clock.

===============================================================================
## 8. CONFIG (gnb_srs_test.yaml)
===============================================================================
- fc 3.75 GHz (dl_arfcn 650000), band 78 TDD SA, 20 MHz, SCS 30 kHz, 1x1
- SRS periodic, tx_comb 2 (6 subcarriers/RB, 2 UEs), widest BW (48/51 RB)
- period_ms 20 (50Hz) or 10 (100Hz)
- tx_gain 85, rx_gain 40 or 50, clock external
- line 8 addrs = core IP (192.168.200.207), line 10 bind_addrs = gNB IP (.213)
- core amf.yaml ngap addr must match line 8
