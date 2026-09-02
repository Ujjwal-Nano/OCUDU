# OCUDU Mobility Project — Status & Handoff

Last updated: end of mobility-sweep session. Use this to resume in a fresh chat.

## What this project is
TU Dresden / Murrelektronik PoC implementing Zhang/Schulz/Fettweis radio-resource
swapping (max-min fairness, lift the weakest user by trading frequency sub-bands) on a
real open-source 5G base station (OCUDU, a fork of srsRAN). Private 5G cell, COTS Huawei
P40 UE, USRP B205mini-i, Open5GS core.

## Testbed (current, working)
- gNB: Fujitsu Q957, OCUDU, ~/OCUDU/configs/gnb_srs_test.yaml
- Cell: 3.75 GHz (band n78), 20 MHz, SCS 30 kHz, TDD SA, PLMN 99942
- SRS: periodic, **comb 2** (6 subcarriers/RB, 2 UEs can sound simultaneously),
  widest bandwidth (48 of 51 RBs sounded). Two SRS periods tested: 20 ms (50 Hz) and 10 ms (100 Hz).
- **rx_gain: currently under discussion 40 vs 50** — 50 gives +1.6 dB SNR, clean (no clipping,
  only benign "late" timing messages). Link SNR (epre-noise_var) ~6-8 dB at 3.2 m LOS.
- UE: Huawei P40 ANA-NX9, Kirin 990 5G, C432 firmware. Meinberg 10 MHz ext clock required.
- Robot arm: controlled mobility, 1 m stroke, **max speed 0.5 m/s** (hardware limit).
- **Keep-alive ping from core required during capture** or phone drops (idle timeout) at ~3 min.

## Measurement pipeline
- Raw: /tmp/srs_rb.jsonl, per-RB power, ~50/s (or ~100/s at 10ms period). Traffic-independent.
- gNB log: /tmp/gnb_srs_test.log has SRS lines with epre/rsrp/noise_var per occasion.
- Capture workflow: `csi_run.sh start` (clears logs, gNB stopped first), then run gNB with
  `sudo script -f /tmp/gnb_console.txt -c "./gnb -c /home/tud/OCUDU/configs/gnb_srs_test.yaml"`,
  press t, phone streaming + keep-alive ping, then `csi_run.sh save <ID> [--mobile] [--speed V] [--duration MIN] "note"`.
- `--mobile` saves auto-land in datasets/Mobility/, static in datasets/Static/.
- `--duration N` auto-truncates the raw at N minutes (avoids stopped-arm tails).

## Tools (all in ~/OCUDU/tools/, committed)
- metrics_suite.py — 6-panel: A regret-vs-T→T*, B lifetime CCDF, C freq-corr→Bc, D granularity,
  E temporal-autocorr→Tc, F context. Flags: --speed, --avg-win MS, --mobile, --trim-start/end,
  --re-attach-guard, --mobile-csv PATH. Draws 0.42λ/v theory line on E when --speed given.
- mobility_tc.py — **plateau-based window-independent Tc**. Sweeps averaging windows, finds the
  flat region (Tc stable = noise-suppressed but not smoothing-biased), reports that Tc or flags
  NO_PLATEAU (resolution-limited). --speed, --csv.
- plot_mobility_sweep.py — Tc-vs-speed and T*-vs-speed, both SRS periods overlaid (auto-detects
  period from label: E00xx=10ms, E0xx=20ms).
- rb_to_ru.py, plot_csi.py, analyze_position.py, swap_value.py, gnb_log_summary.py, srs_snr.py, srs_snr_plot.py.

## KEY METHODOLOGY (hard-won, don't lose)
1. **Windowing:** raw per-occasion SRS is noisy (few subcarriers, 1 symbol). Noise is
   independent between samples → destroys autocorrelation → falsely tiny Tc. Time-averaging
   (moving window) suppresses noise (√N). BUT too-long window smears real channel → inflates Tc.
2. **Plateau method:** the correct Tc is the window-independent value found by sweeping windows
   and taking the FLAT region. Window too short → noise-shortened. Too long → smoothing-inflated.
   The plateau between is the truth. NEVER pick a window to match theory (circular). NEVER use a
   fixed window blindly (biases fast-speed points). For these captures the plateau starts at 20 ms
   (comb-2 low noise) and 40 ms is a robust fixed choice (2 samples @ 50Hz, 4 @ 100Hz).
3. **Tc resolution = ±1 sample** (±20 ms at 50 Hz, ±10 ms at 100 Hz). Fast points (0.4-0.5 m/s,
   Tc~60ms) are coarse at 50 Hz.
4. **SRS SNR three views:** slow/fast split (metrics_suite) = INFLATED artifact under motion
   (fading counted as noise, drops with speed spuriously). rsrp-noise_var = true instantaneous
   (low median under motion = real fading nulls). epre-noise_var = link SNR without fade (~6-8 dB,
   STABLE across speed). Report epre-based as link quality; the low rsrp-median is a fading result,
   not a problem. Link is fine at 3.2 m LOS.
5. **Bc misreads tilts** — the 0.5-crossing only valid for hump/monotonic-decay profiles; tilt
   makes far-RBs anti-correlate → spurious tiny Bc. Use G/separation for exploitability on tilts.
6. **Arm speed profile:** back-and-forth motion means speed oscillates (0 at turnarounds, peak
   mid-stroke), so effective Doppler < setpoint, especially at slow speeds → measured Tc/theory
   ratio rises with speed (0.44 at 0.02 → ~0.85 at 0.3-0.5).
7. **Arm cycle self-validates speed:** autocorrelation shows periodic peaks at 2·stroke/v; can
   read speed off the peak spacing.

## RESULTS (mobility sweep, both SRS periods, plateau Tc)
| speed | Tc(20ms) | Tc(10ms) | theory | ratio | T*(20ms) |
|-------|----------|----------|--------|-------|----------|
| 0.02  | 0.74     | 0.97/0.84| 1.68   | 0.44  | 0.94     | (UNRELIABLE — few cycles, drop/flag)
| 0.05  | 0.38     | 0.41     | 0.672  | 0.57  | 0.48     |
| 0.1   | 0.20     | 0.18     | 0.336  | 0.60  | 0.31     |
| 0.2   | 0.12     | 0.11     | 0.168  | 0.71  | 0.16     |
| 0.3   | 0.10     | 0.08     | 0.112  | 0.89  | 0.12     |
| 0.4   | 0.06     | 0.07     | 0.084  | 0.71  | 0.08     |
| 0.5   | 0.06     | 0.055    | 0.067  | 0.89  | 0.06     |

**Findings:**
- Tc ∝ 1/v, ~0.7× isotropic-theory constant (0.42), consistent across both sounding rates.
- **50 Hz and 100 Hz agree** → 20 ms SRS period sufficient to resolve Tc up to 0.5 m/s.
- **T* decreases with speed but is period-independent** — it reflects channel dynamics, not
  measurement rate. This is the correct/expected result (not a missing pattern).
- Static regret ~0.05 dB vs mobile ~1.5-2 dB = the mobility story (adaptation matters under motion).

## STATIC RESULTS (earlier, gain 40, July)
Positions E001 (metal shelf, hump, RU3 best), E004 (open, tilt, RU0 best, 3.1 dB monotonic).
CQI flat 12-13 (99%) while sub-bands span 2-3 dB = RQ3 result (wideband CQI blind to sub-band
structure). Bc ~6.5 MHz (hump), G rises as K shrinks (granularity), separation 4-6.

## PUBLISHABILITY (web-searched, be honest)
- NOT novel: COTS+open-source testbed (crowded), SRS→sub-band-CSI→freq-selective-sched
  (3GPP-defined, patented, MATLAB does it), coherence BW at 3.5 GHz (mature).
- NOVEL/defensible (thin in literature): MEASURED decision-lifetime/Tc/T* on real COTS UE at
  CONTROLLED speeds (robot arm); noise-aware plateau methodology; the per-RB dataset.
- **Reframe: measurement/requirement study** ("how often must scheduler re-measure vs speed, incl.
  where sub-band scheduling stops being worth it") NOT a "gain" paper (gain is small ~1-3 dB, patented).
- Venue: WiNTECH / experimental-track + dataset paper, NOT top-tier. MUST read last 2-3 yr WiNTECH
  proceedings before writing.
- Reciprocity caveat always: measure UL SRS, swap targets DL; TDD same-freq-diff-time reciprocal;
  hardware chains not, but offsets cancel in per-RU power comparison (no calibration needed for
  scheduling decisions).

## PENDING / NEXT
1. Decide rx_gain (40 vs 50) and lock for consistency.
2. Finalize mobility plots: use plateau Tc (not mixed-window), drop/flag 0.02 m/s, maybe add
   ratio-vs-speed 3rd panel.
3. Fix swap_value.py order-dependent two-user baseline bug (0.5*(bwA+bwB)) before trusting
   complementarity numbers.
4. Second P40 (ANA-NX9) + programmable SIMs + PC/SC reader on order → enables 2-UE swap experiment
   (comb 2 already supports 2 simultaneous UEs on offsets 0,1).
5. 2-UE swap = separate stronger experimental-track paper: "first OTA max-min sub-band swap in
   open-source gNB with COTS UEs" — gated behind enforcement (constrain PRB grants to assigned RUs)
   + 2nd SIM + measured weakest-user throughput.
6. Verify OCUDU has NO sub-band CQI (grep scheduler) for the enabling-contribution claim.
7. Future project (documented, not built): per-subcarrier COMPLEX CSI available at `mean_lse` in
   srs_estimator_generic_impl.cpp (~line 210) BEFORE ocuduvec::mean averages it away at line 240.
   result.channel_matrix is wideband-only. To capture: retain mean_lse before averaging, log to
   binary → .npy. Comb 1 would give 12 subcarriers/RB (full resolution). ~288 complex/occasion at comb 2.

## Repo: github.com/Ujjwal-Nano/OCUDU (fork of srsRAN)
Datasets: datasets/Static/, datasets/Mobility/, campaign.csv, mobility_sweep.csv,
mobility_tc_plateau.csv. Reference docs: Analysis_Theory_Reference.md, Metrics_Suite_Explained.md,
The_Whole_Story.md, Operating_Guide.md.
