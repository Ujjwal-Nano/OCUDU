#!/usr/bin/env bash
set -euo pipefail
cd ~/OCUDU/datasets

# --- Create target structure ---
mkdir -p Static
mkdir -p Mobility/rx_gain_40
mkdir -p Mobility/rx_gain_50
mkdir -p Mobility/uncontrolled

# --- Static ---
git mv E001 Static/E001
git mv E003 Static/E003
git mv E004 Static/E004
git mv 20260722_1124_E001 Static/20260722_1124_E001

# --- Mobility: rx_gain 40 ---
git mv E0001 Mobility/rx_gain_40/E0001
git mv E0002 Mobility/rx_gain_40/E0002
git mv E0003 Mobility/rx_gain_40/E0003
git mv E0004 Mobility/rx_gain_40/E0004
git mv E0005 Mobility/rx_gain_40/E0005

# --- Mobility: rx_gain 50 ---
git mv Experiment01 Mobility/rx_gain_50/Experiment01
git mv Experiment02 Mobility/rx_gain_50/Experiment02
# NOTE: Experiment02 contains mixed rx_gain 40/50 runs (three csi_run.sh save
# calls landed in one folder). Flagging this in a note rather than splitting,
# since splitting requires exact per-file timestamps we haven't verified.
echo "MIXED: contains rx_gain 40 (newest, ~1404) and rx_gain 50 (1135, 1321) runs — needs manual file-level split if precision matters." \
  >> Mobility/rx_gain_50/Experiment02/MIXED_GAIN_WARNING.txt
git add Mobility/rx_gain_50/Experiment02/MIXED_GAIN_WARNING.txt

# --- Mobility: uncontrolled ---
git mv E005-trial Mobility/uncontrolled/E005-trial

# --- Delete confirmed junk ---
git rm -r "E100-"
git rm -r "Mobility-001total aperture 1.0m around centre, speed 0f 0.05 m"
git rm -r "Mobility-0.05m"

# --- Commit ---
git add -A
git commit -m "Reorganize datasets/: split into Static/ and Mobility/{rx_gain_40,rx_gain_50,uncontrolled}/; remove empty junk folders"
