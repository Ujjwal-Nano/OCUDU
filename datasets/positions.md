# Measurement Positions Log — P1 Campaign

Testbed: OCUDU gNB (Q957 + B205mini-i) · 3750 MHz, band n78 TDD, 20 MHz (51 RB cell, 48 RB sounded) · SCS 30 kHz · SRS periodic 20 ms, tx_comb 4, widest bandwidth · 4 RUs x 12 RB · tx_gain 80 / rx_gain 40 · Open5GS core · UE: Huawei P40 (SA), streaming video throughout to keep RRC connected.

Config file for the whole campaign: `configs/gnb_srs_test.yaml` (unchanged between positions — record commit hash if edited).
Analysis: `tools/analyze_position.py` (trim: first 1.5 min, last 1.0 min; margin 1 dB) → `datasets/campaign.csv`.

---

## How to log a position (fill this block per capture)

```
### Exx — <short name>
- **File:** datasets/YYYYMMDD_HHMM_Exx_<name>.jsonl
- **Date/time:**
- **Distance / geometry to gNB antenna:**    (approx. metres, line-of-sight? through what?)
- **Enclosure / support:**                   (open air on stand / inside box / behind object / handheld)
- **Phone orientation:**                     (screen up / vertical, marked arrow direction)
- **Charging:**                              (yes/no — cable routing matters)
- **Room occupancy:**                        (people present, seated/moving, door open/closed)
- **Duration:**
- **Photo:**                                 (datasets/photos/Exx.jpg)
- **Results (from campaign.csv):** gap __ dB · sigma __ dB · sep __ · flip% __ · regret __ dB · noAdapt __ dB · genie __ dB · best RU __
- **Notes / anomalies:**
```

---

## Recorded positions

### E01 — behind metal box
- **File:** datasets/20260721_1107_E01_behind_metalbox.jsonl
- **Date/time:** 2026-07-21 11:07 (21.2 min raw, 18.5 min analysed)
- **Distance / geometry:** TO FILL (m; obstructed by metal enclosure)
- **Enclosure / support:** phone placed behind a metal box (not inside)
- **Phone orientation:** TO FILL
- **Charging:** TO FILL
- **Room occupancy:** TO FILL (operator left the room after start)
- **Photo:** TO ADD
- **Results:** gap **4.90** dB · sigma **0.29** · sep **16.6** · flip% **0.0** · regret **0.00** · noAdapt **0.00** · genie **2.30** · best RU **RU3** (argmax 100 %)
- **Notes:** Strongest, cleanest structure of the campaign so far. Zero rank changes in 18.5 min — decision lifetime censored by capture length. Monotonic rise RU0→RU3 (~5 dB tilt). Start/end transients = operator placing/retrieving phone (~4–5 dB body shadowing).

### E02 — inside paper box
- **File:** datasets/20260721_1135_E02_inside_paper_box.jsonl
- **Date/time:** 2026-07-21 11:35 (21.0 min raw, 18.4 min analysed)
- **Distance / geometry:** TO FILL
- **Enclosure / support:** inside a cardboard box (RF-transparent)
- **Phone orientation:** TO FILL
- **Charging:** TO FILL
- **Room occupancy:** TO FILL — **two level regimes observed** (step up ~4.5 min, back down ~12 min, ≈1.5 dB common-mode): likely someone present/absent in the room; NOT logged at capture time.
- **Photo:** TO ADD
- **Results:** gap **1.43** dB · sigma **0.95** · sep **1.5** · flip% **19.2** · regret **0.44** · noAdapt **0.27** · genie **0.62** · best fixed RU **RU3** (argmax 48 %)
- **Notes:** Flattest position — little exploitable structure; frequent flips are noise between near-equal RUs (genie only 0.62 dB). Sub-segment analysis available: regime A 1.5–4.3 min, B 5–11.8 min (sigma 0.3, flips 2.4 %), C 12.5–20 min (sigma 0.78, flips 28.5 %).

### E03 — open lab, UE charging
- **File:** datasets/20260721_1255_E03_UE_charging_open.jsonl
- **Date/time:** 2026-07-21 12:55 (19.2 min raw, 16.6 min analysed)
- **Distance / geometry:** open space in lab, TO FILL
- **Enclosure / support:** open, on surface
- **Phone orientation:** TO FILL
- **Charging:** **YES** — cable attached during whole capture
- **Room occupancy:** TO FILL (working hours, open area)
- **Photo:** TO ADD
- **Results:** gap **3.18** dB · sigma **1.45** · sep **2.2** · flip% **14.3** · regret **0.03** · noAdapt **0.03** · genie **1.50** · best RU **RU0** (argmax 88 %)
- **Notes:** Ordering stable (RU0/RU1 > RU2 > RU3, RU3 essentially never best) but levels swing ±2–3 dB. Two candidate causes not yet separated: charging cable/charger coupling, or people moving in open lab. **Control capture E04 planned (same spot, unplugged).**

---

## Planned positions

| ID | What | Purpose |
|---|---|---|
| E04 | E03 spot, **unplugged** | isolate charging-cable effect on sigma |
| E05 | E01 spot, **person walking** between UE and gNB | motion contrast — expect noAdapt to rise |
| E06 | E01 spot, **handheld** | user-held contrast (body + micro-motion) |
| E07–E15 | grid across room: 4x3 pattern ~1 m spacing + far corners still holding link | RQ1 distribution over positions |
| R-x | repeat of 3 positions on different days | repeatability of the fingerprint |
| G-x | 5 positions re-run with rbs_per_ru 6 and 24 | granularity sweep (RQ5) |

## Cross-position observations so far
- **Best RU differs by position** (RU3 at E01, RU3-ish/flat at E02, RU0 at E03) → the frequency-selective pattern is a fingerprint of geometry, not an instrument artifact.
- **Static → adaptation adds little:** noAdapt ≤ 0.27 dB everywhere; the opportunity (genie 0.6–2.3 dB) is captured by a single good decision.
- **Separation ratio (gap/sigma) orders the positions** consistently with visual impression: 16.6 (clean) → 2.2 → 1.5 (noise-dominated).
- **Obstruction seems to increase selectivity** (constrained aperture → fewer paths → sharper structure); open/scattered positions flatten it. To be tested across the grid.
- Log-normal check: meanP − geoM ≈ 0.115·sigma² holds for all three captures → per-RU fluctuations are log-normal, justifying dB-domain statistics.

## Protocol reminders
1. `~/OCUDU/tools/csi_run.sh start` → leave the room → ≥20 min → `csi_run.sh save Exx_<name> "<note incl. occupancy/door/charging>"`.
2. Note **wall-clock times** of any deliberate change (person enters, door opens, etc.).
3. Analyse with `--csv ~/OCUDU/datasets/campaign.csv`, then fill the block above.
4. Photograph the position; store as `datasets/photos/Exx.jpg`.
5. Never change gains/config mid-campaign.
