#!/usr/bin/env python3
#
# Copyright 2021-2026 Software Radio Systems Limited
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the distribution.
#

import yaml
import time
import json
import argparse
import os
import sys
import signal
import logging
from pprint import pformat
from datetime import datetime, timedelta, timezone
from contextlib import closing
from typing import Any, Dict, Optional

from websocket import (
    create_connection,
    getdefaulttimeout,
    WebSocket,
    WebSocketConnectionClosedException,
    WebSocketTimeoutException,
)


# pylint: disable=too-few-public-methods
class WsRemoteCommands:
    """
    A class to handle remote commands via WebSocket.

    Attributes:
        host (str): The WebSocket server host. Defaults to "127.0.0.1".
        port (str): The WebSocket server port. Defaults to "8001".
    """

    _EXCEPTION_ARRAY = (
        ConnectionError,
        ConnectionRefusedError,
        TimeoutError,
        OSError,
        WebSocketConnectionClosedException,
        WebSocketTimeoutException,
    )

    def __init__(self, host="127.0.0.1", port="8001"):
        self.host = host
        self.port = port
        # websocket.enableTrace(True)

    def send_ssb_command(self, ssb_cell_config) -> Any:
        """
        Sends an SSB (Synchronization Signal Block) configuration command.

        Args:
            ssb_cell_config (list): A list of cell configurations for the SSB.
        """
        return self._send_ws_command({"cmd": "ssb_set", "cells": ssb_cell_config})

    def send_quit_command(self, timeout=None) -> bool:
        """
        Sends a quit command to the server and waits for connection close.

        Returns:
            bool: True if quit command was successful and connection closed properly,
        """

        conn = self._create_connection(timeout)
        if conn is None:
            return False

        response = self._send_and_receive(conn, {"cmd": "quit"})
        if response and response.get("cmd", "") == "quit" and "error" not in response:
            # Wait for the server to close the connection
            try:
                conn.recv()
            except WebSocketConnectionClosedException:
                return True
            except self._EXCEPTION_ARRAY:
                pass
        return False

    def is_connected(self) -> bool:
        """
        Checks if the WebSocket server is connected.

        Returns:
            bool: True if quit command was successful and connection closed properly,
        """
        conn = self._create_connection()
        if conn is None:
            return False

        try:
            conn.close()
            return True
        except self._EXCEPTION_ARRAY:
            return False

    def _create_connection(self, timeout=None) -> Optional[WebSocket]:
        try:
            return create_connection(
                f"ws://{self.host}:{self.port}", timeout=timeout if timeout is not None else getdefaulttimeout()
            )
        except self._EXCEPTION_ARRAY as e:
            logging.error(f"Failed to connect to WebSocket server at ws://{self.host}:{self.port}: {e}")
            return None

    def _send_and_receive(self, conn: WebSocket, command: Dict) -> Any:
        msg = json.dumps(command, indent=2)
        logging.debug(f"WebSocket request: {msg}")

        try:
            conn.send(msg)
            response = conn.recv()
            response_obj = json.loads(response)
            logging.debug(f"WebSocket raw response: {response}")
            return response_obj
        except self._EXCEPTION_ARRAY as e:
            logging.error(f"WebSocket error during communication: {e}")
            return None

    def _send_ws_command(self, command: Dict) -> Any:
        conn = self._create_connection()
        if conn is not None:
            with closing(conn):
                return self._send_and_receive(conn, command)
        return None

    def send_ntn_update_command(self, command):
        # Send command
        result = self._send_ws_command(command)
        if result is None:
            raise ConnectionError("Failed to send NTN update command")
        return result


def next_5min_timestamp_ms():
    now = datetime.now(timezone.utc)
    # Round minutes up to the next multiple of 5
    next_minute = (now.minute // 5 + 1) * 5
    if next_minute == 60:
        # Wrap around the hour
        next_time = now.replace(minute=0, second=0, microsecond=0) + timedelta(hours=1)
    else:
        next_time = now.replace(minute=next_minute, second=0, microsecond=0)

    # Convert to epoch in milliseconds
    epoch_ms = int(next_time.timestamp() * 1_000)
    return next_time, epoch_ms


def next_run_time():
    """Return datetime to run (1 min before the next 5-min mark)."""
    next_time, _ = next_5min_timestamp_ms()
    return next_time - timedelta(minutes=1)


def parse_iso_timestamp(iso_string):
    """
    Parse ISO format timestamp string to epoch milliseconds (assumes UTC).

    Args:
        iso_string: ISO format timestamp in UTC (e.g., "2026-02-10T11:08:45" or "2026-02-10T11:08:45.000")

    Returns:
        int: Epoch timestamp in milliseconds
    """
    # Handle both with and without milliseconds
    try:
        # Try parsing with milliseconds first
        dt = datetime.strptime(iso_string, "%Y-%m-%dT%H:%M:%S.%f")
    except ValueError:
        # Fall back to parsing without milliseconds
        dt = datetime.strptime(iso_string, "%Y-%m-%dT%H:%M:%S")

    # Make timezone-aware as UTC (strptime returns naive datetime)
    dt = dt.replace(tzinfo=timezone.utc)

    # Convert to epoch milliseconds
    epoch_ms = int(dt.timestamp() * 1_000)
    return epoch_ms


def build_ntn_update_command(config_file, plmn, nci, epoch_timestamp_ms):
    """
    Build NTN update command from YAML configuration file.

    Args:
        config_file: Path to YAML configuration file
        plmn: PLMN identity string
        nci: NR Cell Identity integer
        epoch_timestamp_ms: Epoch timestamp in milliseconds

    Returns:
        dict: NTN update command ready to send via WebSocket
    """
    # Load the YAML content
    try:
        with open(config_file, "r") as f:
            data = yaml.safe_load(f)
    except yaml.YAMLError as e:
        raise ValueError(f"Invalid YAML format in {config_file}: {e}")

    # Extract NTN config from the cell_cfg structure
    try:
        ntn_cfg = data["cell_cfg"]["ntn"]
    except (KeyError, TypeError) as e:
        raise ValueError(f"Invalid config structure: missing 'cell_cfg.ntn' section: {e}")

    # Determine ephemeris format
    sib19_updates_format = "ecef"
    if "ephemeris_orbital" in ntn_cfg:
        sib19_updates_format = "orbital"

    # Build cell configuration
    cell_config = {
        'plmn': plmn,
        'nci': nci,
        'epoch_timestamp': epoch_timestamp_ms,
        'ntn_ul_sync_validity_duration': ntn_cfg["ntn_ul_sync_validity_dur"],
        'ta_info': {
            'ta_common': ntn_cfg["ta_info"]["ta_common"],
            'ta_common_drift': ntn_cfg["ta_info"]["ta_common_drift"],
            'ta_common_drift_variant': ntn_cfg["ta_info"]["ta_common_drift_variant"]
        }
    }

    # Add ephemeris info
    if sib19_updates_format == "ecef":
        cell_config['ephemeris_info'] = {
            'ecef': {
                'position_x': ntn_cfg["ephemeris_info_ecef"]["pos_x"],
                'position_y': ntn_cfg["ephemeris_info_ecef"]["pos_y"],
                'position_z': ntn_cfg["ephemeris_info_ecef"]["pos_z"],
                'velocity_vx': ntn_cfg["ephemeris_info_ecef"]["vel_x"],
                'velocity_vy': ntn_cfg["ephemeris_info_ecef"]["vel_y"],
                'velocity_vz': ntn_cfg["ephemeris_info_ecef"]["vel_z"],
            }
        }
    else:
        cell_config['ephemeris_info'] = {
            'orbital': {
                'semi_major_axis': int(ntn_cfg["ephemeris_orbital"]["semi_major_axis"]),
                'eccentricity': ntn_cfg["ephemeris_orbital"]["eccentricity"],
                'periapsis': ntn_cfg["ephemeris_orbital"]["periapsis"],
                'longitude': ntn_cfg["ephemeris_orbital"]["longitude"],
                'inclination': ntn_cfg["ephemeris_orbital"]["inclination"],
                'mean_anomaly': ntn_cfg["ephemeris_orbital"]["mean_anomaly"]
            }
        }

    # Add optional feeder link info if present
    if "feeder_link" in ntn_cfg:
        cell_config['feeder_link_info'] = {
            'enable_doppler_compensation': ntn_cfg["feeder_link"]["enable_doppler_compensation"],
            'dl_freq': int(ntn_cfg["feeder_link"]["dl_freq"]),
            'ul_freq': int(ntn_cfg["feeder_link"]["ul_freq"])
        }

    # Add optional gateway location if present
    if "gateway_location" in ntn_cfg:
        cell_config['ntn_gateway_location'] = {
            'latitude': ntn_cfg["gateway_location"]["latitude"],
            'longitude': ntn_cfg["gateway_location"]["longitude"],
            'altitude': ntn_cfg["gateway_location"]["altitude"]
        }

    # Add optional SIB19 value-tag-tracked fields if present in YAML.
    if "reference_location" in ntn_cfg:
        cell_config['reference_location'] = {
            'latitude': ntn_cfg["reference_location"]["latitude"],
            'longitude': ntn_cfg["reference_location"]["longitude"],
        }
    if "distance_threshold" in ntn_cfg:
        cell_config['distance_threshold'] = ntn_cfg["distance_threshold"]
    if "t_service" in ntn_cfg:
        cell_config['t_service'] = ntn_cfg["t_service"]
    if "polarization" in ntn_cfg:
        pol = {}
        if "dl" in ntn_cfg["polarization"]:
            pol['dl'] = ntn_cfg["polarization"]["dl"]
        if "ul" in ntn_cfg["polarization"]:
            pol['ul'] = ntn_cfg["polarization"]["ul"]
        cell_config['polarization'] = pol
    if "ta_report" in ntn_cfg:
        cell_config['ta_report'] = ntn_cfg["ta_report"]
    if "sat_switch_with_resync" in ntn_cfg:
        sat = ntn_cfg["sat_switch_with_resync"]
        sat_config = {}
        if "t_service_start" in sat:
            sat_config['t_service_start'] = sat["t_service_start"]
        if "ssb_time_offset_sf" in sat:
            sat_config['ssb_time_offset_sf'] = sat["ssb_time_offset_sf"]
        if "ntn_cfg" in sat:
            inner = sat["ntn_cfg"]
            sat_ntn = {}
            if "cell_specific_koffset" in inner:
                sat_ntn['cell_specific_koffset'] = inner["cell_specific_koffset"]
            if "k_mac" in inner:
                sat_ntn['k_mac'] = inner["k_mac"]
            if "ntn_ul_sync_validity_dur" in inner:
                sat_ntn['ntn_ul_sync_validity_dur'] = inner["ntn_ul_sync_validity_dur"]
            if "ta_info" in inner:
                sat_ntn['ta_info'] = {
                    'ta_common': inner["ta_info"]["ta_common"],
                    'ta_common_drift': inner["ta_info"]["ta_common_drift"],
                    'ta_common_drift_variant': inner["ta_info"]["ta_common_drift_variant"],
                }
            if "ta_report" in inner:
                sat_ntn['ta_report'] = inner["ta_report"]
            if "polarization" in inner:
                pol = {}
                if "dl" in inner["polarization"]:
                    pol['dl'] = inner["polarization"]["dl"]
                if "ul" in inner["polarization"]:
                    pol['ul'] = inner["polarization"]["ul"]
                sat_ntn['polarization'] = pol
            if "ephemeris_info_ecef" in inner:
                sat_ntn['ephemeris_info'] = {
                    'ecef': {
                        'position_x': inner["ephemeris_info_ecef"]["pos_x"],
                        'position_y': inner["ephemeris_info_ecef"]["pos_y"],
                        'position_z': inner["ephemeris_info_ecef"]["pos_z"],
                        'velocity_vx': inner["ephemeris_info_ecef"]["vel_x"],
                        'velocity_vy': inner["ephemeris_info_ecef"]["vel_y"],
                        'velocity_vz': inner["ephemeris_info_ecef"]["vel_z"],
                    }
                }
            elif "ephemeris_orbital" in inner:
                sat_ntn['ephemeris_info'] = {
                    'orbital': {
                        'semi_major_axis': int(inner["ephemeris_orbital"]["semi_major_axis"]),
                        'eccentricity': inner["ephemeris_orbital"]["eccentricity"],
                        'periapsis': inner["ephemeris_orbital"]["periapsis"],
                        'longitude': inner["ephemeris_orbital"]["longitude"],
                        'inclination': inner["ephemeris_orbital"]["inclination"],
                        'mean_anomaly': inner["ephemeris_orbital"]["mean_anomaly"],
                    }
                }
            if sat_ntn:
                sat_config['ntn_cfg'] = sat_ntn
        cell_config['sat_switch_with_resync'] = sat_config

    if "moving_ref_location" in ntn_cfg:
        cell_config['moving_ref_location'] = {
            'latitude': ntn_cfg["moving_ref_location"]["latitude"],
            'longitude': ntn_cfg["moving_ref_location"]["longitude"],
        }

    # Build the command with cells array
    command = {
        'cmd': "ntn_config_update",
        'cells': [cell_config]
    }

    return command


def send_ntn_update(config_file=None, plmn=None, nci=None, epoch_timestamp_ms=None, host="127.0.0.1", port="8001"):
    """
    Send NTN configuration update via WebSocket.

    Args:
        config_file: Path to YAML config file (default: ntn.yml in script directory)
        plmn: PLMN identity (default: "00101")
        nci: NR Cell Identity (default: 6733824)
        epoch_timestamp_ms: Epoch timestamp in milliseconds (default: auto-calculated next 5-min)
        host: WebSocket server host (default: "127.0.0.1")
        port: WebSocket server port (default: "8001")
    """
    # Use provided epoch timestamp or calculate next 5-minute epoch time
    if epoch_timestamp_ms is None:
        t, ts_ms = next_5min_timestamp_ms()
        logging.info(f"Send SIB19 update with epoch time (UTC): {t}")
    else:
        ts_ms = epoch_timestamp_ms
        t = datetime.fromtimestamp(ts_ms / 1_000, tz=timezone.utc)
        logging.info(f"Send SIB19 update with custom epoch time (UTC): {t}")

    # Resolve config file path
    if config_file is None:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        config_file = os.path.join(script_dir, "ntn.yml")

    # Check if config file exists
    if not os.path.isfile(config_file):
        logging.error(f"Config file not found: {config_file}")
        sys.exit(1)

    logging.debug(f"Using config file: {config_file}")
    logging.debug(f"PLMN: {plmn}, NCI: {nci}")

    # Use default values if not provided
    if plmn is None:
        plmn = "00101"
    if nci is None:
        nci = 6733824

    # Build command from config file
    try:
        command = build_ntn_update_command(config_file, plmn, nci, ts_ms)
    except Exception as e:
        logging.error(f"Failed to build NTN command from config: {e}")
        sys.exit(1)

    logging.info("Sending NTN config update:")
    logging.debug(f"Command details:\n{pformat(command)}")

    # Send via WebSocket
    try:
        response = WsRemoteCommands(host=host, port=port).send_ntn_update_command(command)

        # Log the response
        if response:
            logging.info("NTN update sent successfully")
            logging.info(f"Server response: {pformat(response)}")

            # Check for error in response
            if "error" in response:
                logging.warning(f"Server reported error: {response['error']}")
        else:
            logging.warning("No response received from server")
    except Exception as e:
        logging.error(f"Failed to send NTN update: {e}")
        sys.exit(1)

# Global shutdown flag
shutdown_requested = False


def signal_handler(signum, frame):
    """Handle shutdown signals gracefully."""
    global shutdown_requested
    signal_name = signal.Signals(signum).name
    logging.info(f"\n\nReceived {signal_name}, shutting down gracefully...")
    shutdown_requested = True


def show_progress_bar(current, total, bar_length=50, prefix="Progress"):
    """
    Display a progress bar with time remaining.

    Args:
        current: Current elapsed time in seconds
        total: Total time in seconds
        bar_length: Length of the progress bar in characters
        prefix: Prefix text for the progress bar
    """
    if total <= 0:
        return

    percent = min(100, (current / total) * 100)
    filled_length = int(bar_length * current // total)
    bar = '█' * filled_length + '░' * (bar_length - filled_length)

    remaining = max(0, total - current)
    remaining_str = f"{int(remaining)}s"

    # Print progress bar (use \r to overwrite the same line)
    print(f"\r{prefix}: |{bar}| {percent:.1f}% - {remaining_str} remaining", end='', flush=True)


if __name__ == "__main__":
    # Register signal handlers for graceful shutdown
    signal.signal(signal.SIGINT, signal_handler)   # Ctrl+C
    signal.signal(signal.SIGTERM, signal_handler)  # termination signal

    parser = argparse.ArgumentParser(
        description="Send NTN configuration updates via WebSocket",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Run continuously with 5-minute intervals (default behavior)
  %(prog)s

  # Send update immediately with custom config
  %(prog)s --now --config custom_ntn.yml --plmn 00101 --nci 12345

  # Send update with custom epoch timestamp in UTC
  %(prog)s --now --epoch-time-utc "2026-02-10T11:08:45.000"

  # Run continuously with custom parameters and debug logging
  %(prog)s --config /path/to/ntn.yml --plmn 00102 --nci 67890 --log-level DEBUG

  # Send immediately with minimal output
  %(prog)s --now --log-level WARNING
        """
    )

    parser.add_argument("-c", "--config", type=str, default=None, help="Path to NTN YAML config file (default: ntn.yml in script directory)")
    parser.add_argument("-p", "--plmn", type=str, default=None, help="PLMN identity (default: 00101)")
    parser.add_argument("-n", "--nci", type=int, default=None, help="NR Cell Identity (default: 6733824)")
    parser.add_argument("-e", "--epoch-time-utc", type=str, default=None, help="Epoch time in UTC (ISO format: YYYY-MM-DDTHH:MM:SS or YYYY-MM-DDTHH:MM:SS.mmm)")
    parser.add_argument("--now", action="store_true", help="Send update immediately without waiting for 5-minute interval")
    parser.add_argument("--host", type=str, default="127.0.0.1", help="WebSocket server host (default: 127.0.0.1)")
    parser.add_argument("--port", type=str, default="8001", help="WebSocket server port (default: 8001)")
    parser.add_argument("--log-level", type=str, default="INFO", choices=["DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"], help="Set logging level (default: INFO)")

    args = parser.parse_args()

    # Configure logging
    log_format = '%(asctime)s - %(levelname)s - %(message)s'
    logging.basicConfig(
        level=getattr(logging, args.log_level),
        format=log_format,
        datefmt='%Y-%m-%d %H:%M:%S'
    )

    # Parse epoch timestamp if provided
    epoch_timestamp_ms = None
    if args.epoch_time_utc:
        try:
            epoch_timestamp_ms = parse_iso_timestamp(args.epoch_time_utc)
            logging.info(f"Using custom epoch time (UTC): {args.epoch_time_utc} ({epoch_timestamp_ms} ms)")
        except ValueError as e:
            logging.error(f"Invalid epoch time format: {e}")
            logging.error("Expected format (UTC): YYYY-MM-DDTHH:MM:SS or YYYY-MM-DDTHH:MM:SS.mmm")
            sys.exit(1)

    # If --now flag is set, send update immediately and exit
    if args.now:
        logging.info("Sending NTN update immediately...")
        send_ntn_update(config_file=args.config, plmn=args.plmn, nci=args.nci,
                       epoch_timestamp_ms=epoch_timestamp_ms, host=args.host, port=args.port)
    else:
        # Run continuously with 5-minute intervals
        logging.info("Running in continuous mode (5-minute intervals)")
        logging.info(f"Config: {args.config or 'ntn.yml (default)'}")
        logging.info(f"PLMN: {args.plmn or '00101 (default)'}")
        logging.info(f"NCI: {args.nci or '6733824 (default)'}")
        logging.info(f"WebSocket: ws://{args.host}:{args.port}")
        logging.info("Press Ctrl+C at any time for graceful shutdown\n")

        # Align to the first run (1 min before next 5-min mark)
        run_time = next_run_time()

        try:
            while not shutdown_requested:
                now = datetime.now(timezone.utc)
                sleep_seconds = (run_time - now).total_seconds()

                if sleep_seconds > 0:
                    # Calculate what the epoch time will be (next 5-min mark from run_time)
                    epoch_time = run_time + timedelta(minutes=1)  # run_time is 1 min before epoch
                    logging.info(f"Next update scheduled at {run_time} (UTC), epoch time: {epoch_time.isoformat()}")
                    # Sleep in small increments with progress bar
                    sleep_start = time.time()
                    while time.time() - sleep_start < sleep_seconds:
                        if shutdown_requested:
                            print()  # New line after progress bar
                            break
                        elapsed = time.time() - sleep_start
                        show_progress_bar(elapsed, sleep_seconds, prefix="Waiting")
                        time.sleep(min(0.5, sleep_seconds - elapsed))

                    # Complete the progress bar
                    if not shutdown_requested:
                        show_progress_bar(sleep_seconds, sleep_seconds, prefix="Waiting")
                        print()  # New line after progress bar

                if shutdown_requested:
                    break

                # Run once
                send_ntn_update(config_file=args.config, plmn=args.plmn, nci=args.nci,
                               epoch_timestamp_ms=epoch_timestamp_ms, host=args.host, port=args.port)
                logging.info("=" * 80)  # Separator between updates

                # Schedule next run = +5 minutes from the previous run_time
                run_time = run_time + timedelta(minutes=5)

        except KeyboardInterrupt:
            # Fallback in case signal handler doesn't catch it
            logging.info("\n\nKeyboard interrupt received, shutting down gracefully...")

        logging.info("Shutdown complete. Goodbye!")
        sys.exit(0)
