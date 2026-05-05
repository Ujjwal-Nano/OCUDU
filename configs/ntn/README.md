# NTN Quickstart Guide

This document provides a minimal end-to-end workflow to run an NTN (Non-Terrestrial Network) scenario using the SRS gNB with an NTN configuration, and an Amarisoft UE with its built-in NTN channel emulator.

---

## Requirements

The script uses the following Python modules:

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install numpy skyfield pyyaml websocket-client
```

---

## 1. Start Open5gs core.

```bash
cd ./ocudu/docker
docker compose up 5gc
```

---

## 2. Generate NTN Scenario

First, generate the NTN configuration files from a TLE (Two-Line Element) file.

Example command:

```bash
python generate_ntn_configs.py --tle=./tle_example_leo.txt --feeder-link-enabled=true --ephemeris-info-format=ecef --use-state-vector=false
```

This will produce the required NTN configuration files (e.g., `ntn.yml`, `ue-position.cfg` and `gw-position.cfg`) based on the satellite orbit defined in the TLE file.

---

## 3. Start the SRS gNB

Launch the gNB using the generated NTN configuration together with the standard configuration files:

```bash
export GNB_PATH=/absolute/path/to/binary/srsgnb/build/apps/gnb/gnb  # absolute path to srsgnb binary
sudo $GNB_PATH -c ./gnb.yml -c ntn.yml -c zmq.yml
```

---

## 4. Start Amarisoft UE with Built-in NTN Channel Emulator

```bash
export UE_PATH=/absolute/path/to/binary/amarisoft/2025-11-24/lteue-linux-2025-11-24/lteue  # absolute path to amarisoft UE binary
sudo $UE_PATH ./ue-nr-ntn-emu.cfg
```

---

## 5. Update NTN Configuration (Optional)

The `ntn_config_updater.py` script sends periodic NTN configuration updates (SIB19) to the running gNB via WebSocket. This is useful for updating satellite ephemeris data and timing advance information during operation.

### Run in continuous mode (sends updates every 5 minutes):

```bash
python3 ntn_config_updater.py
```

### Send a single immediate update:

```bash
python3 ntn_config_updater.py --now
```

### Common options:

```bash
python3 ntn_config_updater.py \
          --config ntn.yml \          # Path to NTN config file (default: ntn.yml)
          --plmn 00101 \              # PLMN identity (default: 00101)
          --nci 6733824 \             # NR Cell Identity (default: 6733824)
          --host 127.0.0.1 \          # WebSocket host (default: 127.0.0.1)
          --port 8001 \               # WebSocket port (default: 8001)
          --log-level INFO            # Logging level: DEBUG, INFO, WARNING, ERROR, CRITICAL
```

For more options and examples, run:

```bash
python3 ntn_config_updater.py --help
```
