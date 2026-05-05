#
# Copyright 2021-2026 Software Radio Systems Limited
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the distribution.
#

import time
import argparse
import numpy as np
from datetime import datetime, timedelta, timezone
from dataclasses import dataclass
from typing import Tuple, Union, List, Dict
import skyfield
from skyfield.api import EarthSatellite, load, wgs84, utc
from skyfield.data.spice import inertial_frames
from skyfield.framelib import itrs
from sgp4.api import Satrec, WGS84
import yaml

### Converters ###
class ReferenceFrameConverter:
    """Converter between ECEF and ECI reference frames.

    This class provides methods to convert state vectors (position and velocity) between
    Earth-Centered Earth-Fixed (ECEF) and Earth-Centered Inertial (ECI) reference frames,
    accounting for Earth's rotation and the resulting Coriolis effects on velocities.
    """

    # Earth's rotation rate in rad/s
    EARTH_ROTATION_RATE = 7.292115146706979e-5

    def __init__(self, epoch_time: datetime):
        """Initialize the coordinate converter.

        Args:
            epoch_time: Reference time point for coordinate conversions
        """
        self.epoch_time = epoch_time

    def ecef_to_eci(self, ecef_pos: Union[np.ndarray, List[float], Tuple[float, float, float]],
                    ecef_vel: Union[np.ndarray, List[float], Tuple[float, float, float]],
                    current_time: datetime) -> Dict[str, np.ndarray]:
        """Convert ECEF coordinates and velocities to ECI frame.

        Args:
            ecef_pos: Position vector in ECEF frame [x, y, z] in meters
            ecef_vel: Velocity vector in ECEF frame [vx, vy, vz] in meters/second
            current_time: Current time point for the conversion

        Returns:
            Dictionary containing:
            - 'position': Position vector in ECI frame [x, y, z] in meters
            - 'velocity': Velocity vector in ECI frame [vx, vy, vz] in meters/second
        """
        # Convert inputs to numpy arrays if needed
        ecef_pos = np.asarray(ecef_pos, dtype=float)
        ecef_vel = np.asarray(ecef_vel, dtype=float)

        # Calculate time difference in seconds
        dt = (current_time - self.epoch_time).total_seconds()

        # Calculate rotation angle
        angle = self.EARTH_ROTATION_RATE * dt

        # Rotation matrix components
        cos_angle = np.cos(angle)
        sin_angle = np.sin(angle)

        # Apply rotation to position
        eci_pos = np.zeros(3)
        eci_pos[0] = ecef_pos[0] * cos_angle - ecef_pos[1] * sin_angle
        eci_pos[1] = ecef_pos[0] * sin_angle + ecef_pos[1] * cos_angle
        eci_pos[2] = ecef_pos[2]

        # Apply rotation to velocity (including Coriolis effect)
        eci_vel = np.zeros(3)
        eci_vel[0] = (ecef_vel[0] * cos_angle - ecef_vel[1] * sin_angle - self.EARTH_ROTATION_RATE * ecef_pos[1] * cos_angle - self.EARTH_ROTATION_RATE * ecef_pos[0] * sin_angle)
        eci_vel[1] = (ecef_vel[0] * sin_angle + ecef_vel[1] * cos_angle + self.EARTH_ROTATION_RATE * ecef_pos[0] * cos_angle - self.EARTH_ROTATION_RATE * ecef_pos[1] * sin_angle)
        eci_vel[2] = ecef_vel[2]

        return {'position': eci_pos, 'velocity': eci_vel}

@dataclass
class OrbitalParams:
    """Orbital parameters for a satellite orbit.

    Attributes:
        semi_major_axis: Semi-major axis in meters
        eccentricity: Orbital eccentricity (dimensionless)
        inclination: Orbital inclination in radians
        longitude: Longitude of ascending node in radians
        periapsis: Argument of periapsis in radians
        mean_anomaly: Mean anomaly in radians
    """
    semi_major_axis: float
    eccentricity: float
    inclination: float
    longitude: float
    periapsis: float
    mean_anomaly: float

class EphemerisInfoConverter:
    """Converter between ECI state vectors and orbital elements.

    This class provides methods to convert between position and velocity vectors in the
    Earth-Centered Inertial (ECI) frame and classical orbital elements.
    """

    # Earth's gravitational parameter (GM) [m³/s²]
    MU = 3.986004418e14

    @staticmethod
    def rv_to_oe(position: Union[np.ndarray, List[float], Tuple[float, float, float]],
                 velocity: Union[np.ndarray, List[float], Tuple[float, float, float]]) -> OrbitalParams:
        """Convert ECI state vector to orbital elements.

        Args:
            position: ECI position vector [x, y, z] in meters
            velocity: ECI velocity vector [vx, vy, vz] in meters/second

        Returns:
            Orbital parameters object containing the classical orbital elements
        """
        # Convert inputs to numpy arrays if needed
        position = np.asarray(position, dtype=float)
        velocity = np.asarray(velocity, dtype=float)

        # Calculate specific angular momentum
        h = np.cross(position, velocity)

        # Calculate node vector
        n = np.array([-h[1], h[0], 0.0])

        # Calculate eccentricity vector
        v2 = np.dot(velocity, velocity)
        r = np.sqrt(np.dot(position, position))

        ev = np.array([
            (v2 - EphemerisInfoConverter.MU / r) * position[0] - np.dot(position, velocity) * velocity[0],
            (v2 - EphemerisInfoConverter.MU / r) * position[1] - np.dot(position, velocity) * velocity[1],
            (v2 - EphemerisInfoConverter.MU / r) * position[2] - np.dot(position, velocity) * velocity[2]
        ]) / EphemerisInfoConverter.MU

        # Calculate orbital parameters
        # Semi-major axis
        a = -EphemerisInfoConverter.MU / (2.0 * (v2 / 2.0 - EphemerisInfoConverter.MU / r))

        # Eccentricity
        eccentricity = np.sqrt(np.dot(ev, ev))

        # Inclination
        h_mag = np.sqrt(np.dot(h, h))
        inclination = np.arccos(h[2] / h_mag)

        # Longitude of ascending node
        n_mag = np.sqrt(np.dot(n, n))
        longitude = np.arctan2(n[1], n[0])
        if longitude < 0:
            longitude += 2.0 * np.pi

        # Argument of periapsis
        ev_mag = np.sqrt(np.dot(ev, ev))
        periapsis = np.arccos(np.dot(n, ev) / (n_mag * ev_mag))
        if ev[2] < 0:
            periapsis = 2.0 * np.pi - periapsis

        # True anomaly
        true_anomaly = np.arccos(np.dot(ev, position) / (ev_mag * r))
        if np.dot(position, velocity) < 0:
            true_anomaly = 2.0 * np.pi - true_anomaly

        # Eccentric anomaly
        E = 2.0 * np.arctan(np.sqrt((1.0 - eccentricity) / (1.0 + eccentricity)) * np.tan(true_anomaly / 2.0))

        # Mean anomaly
        mean_anomaly = E - eccentricity * np.sin(E)
        if mean_anomaly < 0:
            mean_anomaly += 2.0 * np.pi

        return OrbitalParams(
            semi_major_axis=a,
            eccentricity=eccentricity,
            inclination=inclination,
            longitude=longitude,
            periapsis=periapsis,
            mean_anomaly=mean_anomaly
        )

### Helper Functions ###
def str_to_bool(value):
    if isinstance(value, bool):
        return value
    if value.lower() in {'false', 'f', '0', 'no', 'n'}:
        return False
    elif value.lower() in {'true', 't', '1', 'yes', 'y'}:
        return True
    raise ValueError(f'{value} is not a valid boolean value')

def convert_numpy_types(obj):
    if isinstance(obj, dict):
        return {k: convert_numpy_types(v) for k, v in obj.items()}
    elif isinstance(obj, list):
        return [convert_numpy_types(i) for i in obj]
    elif isinstance(obj, np.generic):
        return obj.item()
    else:
        return obj

def save_ground_position_cfg(filename, loc):
    template = (
        "latitude: {latitude},\n"
        "longitude: {longitude},\n"
        "altitude: {altitude}\n"
    )
    config_content = template.format(latitude=round(loc.latitude.degrees,9), longitude=round(loc.longitude.degrees,9), altitude=int(loc.elevation.m))
    with open(filename, "w") as config_file:
        config_file.write(config_content)

def save_gnb_ntn_config(filename, ntn_config_dict):
    with open(filename, "w") as config_file:
        yaml.dump(convert_numpy_types(ntn_config_dict), config_file, default_flow_style=False)

def datetime_to_tle_epoch(dt):
    year = dt.year % 100  # Last two digits of the year
    day_of_year = dt.timetuple().tm_yday  # Day of the year
    fraction = (dt.hour * 3600 + dt.minute * 60 + dt.second) / 86400.0  # Fractional portion of the day

    formatted_fraction = "{:.8f}".format(fraction).lstrip('0').lstrip('.')
    epoch = "{:02d}{:03d}.{}".format(year, day_of_year, formatted_fraction)
    return epoch

def load_tle_from_file(filename):
    try:
        with open(filename, 'r') as file:
            lines = file.readlines()
            lines = [line.strip() for line in lines]
            if (len(lines) == 3):
                return lines[0], lines[1], lines[2]
            if (len(lines) == 2):
                return "No-name", lines[0], lines[1]
            return split_lines
    except FileNotFoundError:
        print(f"File '{filename}' not found.")
        return None,None,None

def save_tle_to_file(filename, tle_name, tle_line1, tle_line2):
    with open(filename, 'w') as file:
        file.write(f"{tle_name}\n")
        file.write(f"{tle_line1}\n")
        file.write(f"{tle_line2}\n")

def overwrite_tle_epoch(sat_name, line1, line2, epoch_dt):
    tle_epoch_time = datetime_to_tle_epoch(epoch_dt)
    line1 = line1[:18] + tle_epoch_time + line1[32:]  # TODO: TLE CRC update?
    return sat_name, line1, line2

def get_lla_at_dt(ts, satellite, timestamp=None):
    if timestamp is not None:
        if isinstance(timestamp, datetime):
            timestamp_ts = ts.from_datetime(timestamp)
        if isinstance(timestamp, skyfield.timelib.Time):
            timestamp_ts = timestamp
    else:
        timestamp_ts = satellite.epoch
    geocentric = satellite.at(timestamp_ts)
    latitude, longitude = wgs84.latlon_of(geocentric)
    position = wgs84.geographic_position_of(geocentric)
    altitude = wgs84.height_of(geocentric)
    return latitude, longitude, altitude

def find_pass_over_ground_station(ts, satellite, subpoint, timestamp_dt, time_window, min_elevation_degrees=0, debug=False):
    t0 = ts.from_datetime(timestamp_dt - time_window)
    t1 = ts.from_datetime(timestamp_dt + time_window)
    t, events = satellite.find_events(subpoint, t0, t1, altitude_degrees=min_elevation_degrees)
    event_names = 'rise above {}°'.format(min_elevation_degrees), 'culminate', 'set below {}°'.format(min_elevation_degrees)
    AOS_ts = None
    TCA_ts = None
    LOS_ts = None

    if (debug):
        print("Example Events above the UE position: ")
    for ti, event in zip(t, events):
        name = event_names[event]
        if AOS_ts is None and event == 0:
            AOS_ts = ti
        if TCA_ts is None and event == 1:
            TCA_ts = ti
        if LOS_ts is None and event == 2:
            LOS_ts = ti
        if (debug):
            print("  ",ti.utc_strftime('%Y %b %d %H:%M:%S'), name)

    # remove second frac part
    AOS_dt = AOS_ts.utc_datetime().replace(microsecond=0)
    TCA_dt = TCA_ts.utc_datetime().replace(microsecond=0)
    LOS_dt = LOS_ts.utc_datetime().replace(microsecond=0)
    return AOS_dt, TCA_dt, LOS_dt

def estimate_pass_duration(ts, satellite, min_elevation_deg):
    geocentric = satellite.at(satellite.epoch)
    latitude, longitude = wgs84.latlon_of(geocentric)
    subpoint = wgs84.latlon(latitude.degrees, longitude.degrees, 1)
    time_window = timedelta(minutes=30)
    AOS_dt, TCA_dt, LOS_dt = find_pass_over_ground_station(ts, satellite, subpoint, satellite.epoch.utc_datetime(), time_window, min_elevation_deg)
    return (LOS_dt - AOS_dt)

def find_real_time_ntn_scenario(ts, tle_fn, min_sat_elevation, pass_start_offset, start_time_str=None):
    # Load TLE data
    sat_name, line1, line2 = load_tle_from_file(tle_fn)

    # Reuse the TLE to generate a pass at current time (overwrite epoch)
    pass_start_dt = datetime.utcnow().replace(microsecond=0)

    if start_time_str is not None:
        pass_start_dt = datetime.strptime(start_time_str, "%Y-%m-%dT%H:%M:%S")

    print("Updated TLE epoch time: ", pass_start_dt)
    sat_name, line1, line2 = overwrite_tle_epoch(sat_name, line1, line2, pass_start_dt)
    # Save the updated TLE
    updated_tle_fn = "tle_updated.txt"
    save_tle_to_file(updated_tle_fn, sat_name, line1, line2)
    print("Saved updated TLE to file: ", updated_tle_fn)

    # Create Satellite object with the updated TLE.
    satrec = Satrec.twoline2rv(line1, line2, WGS84)
    satellite = EarthSatellite.from_satrec(satrec, ts)
    #satellite = EarthSatellite(line1, line2, sat_name, ts)

    geocentric = satellite.at(satellite.epoch)
    sat_latitude, sat_longitude = wgs84.latlon_of(geocentric)
    altitude = wgs84.height_of(geocentric)
    # TODO: need a better check
    if (altitude.km > 30000):
        orbit_type = "GEO"
    else:
        orbit_type = "LEO"

    # Estimate pass duration
    pass_duration_s = 0
    if (orbit_type == "LEO"):
        pass_duration_s = estimate_pass_duration(ts, satellite, min_sat_elevation).seconds

    # Compute the pass TCA timestamp as (pass_start_dt + offset + pass_duration/2)
    pass_tca_dt = pass_start_dt + timedelta(seconds=pass_start_offset) + timedelta(seconds=int(pass_duration_s / 2))
    pass_tca_dt = pass_tca_dt.replace(tzinfo=utc)

    # Compute satellite's LLA coordinates at the expected pass TCA timepoint
    latitude, longitude, altitude = get_lla_at_dt(ts, satellite, pass_tca_dt)
    elevation = skyfield.units.Distance(m=1)
    cell_center_subpoint = wgs84.latlon(latitude.degrees, longitude.degrees, elevation.m)
    
    if orbit_type == "GEO":
        AOS_dt = satellite.epoch.utc_datetime()
        TCA_dt = AOS_dt
        LOS_dt = AOS_dt + timedelta(hours=1)
    else:
        # For LEO find a pass over the UE, between TLE Epoch -0.5h and epoch +0.5h
        time_window = timedelta(minutes=30)
        AOS_dt, TCA_dt, LOS_dt = find_pass_over_ground_station(ts, satellite, cell_center_subpoint, pass_tca_dt, time_window, min_sat_elevation)

    # Calculate NTN link delay at AOS, TCA and LOS
    difference = satellite - cell_center_subpoint

    speed_of_light_km_per_s = 299792.458
    topocentric = difference.at(ts.from_datetime(AOS_dt))
    aos_alt, aos_az, aos_distance = topocentric.altaz()
    aos_propagation_delay_us = int(aos_distance.km / speed_of_light_km_per_s * 1e6)

    topocentric = difference.at(ts.from_datetime(TCA_dt))
    tca_alt, tca_az, tca_distance = topocentric.altaz()
    tca_propagation_delay_us = int(tca_distance.km / speed_of_light_km_per_s * 1e6)

    topocentric = difference.at(ts.from_datetime(LOS_dt))
    los_alt, los_az, los_distance = topocentric.altaz()
    los_propagation_delay_us = int(los_distance.km / speed_of_light_km_per_s * 1e6)

    print("")
    print("Satelite:")
    print("--TLE Epoch datetime: ", satellite.epoch.utc_datetime())
    print('--Satelite position [km]:', geocentric.position.km)
    print('--Satelite velocity [km/s]:', geocentric.velocity.km_per_s)
    print('--Satelite LLA coordinates')
    print('----Latitude [deg]:', latitude.degrees)
    print('----Longitude [deg]:', longitude.degrees)
    print('----Altitude [km]:' , altitude.km)
    print("")
    print("Cell Center Position: ")
    print('--Cell Type:', orbit_type)
    print('--Latitude [deg]:', latitude.degrees)
    print('--Longitude [deg]:', longitude.degrees)
    print('--Elevation [m]:' , elevation.m)
    if orbit_type == "GEO":
        print('--AOS at:', AOS_dt)
        print('----Altitude:',  round(aos_alt.degrees,2))
        print('----Azimuth:', round(aos_az.degrees,2))
        print('----Distance: {:.1f} km'.format(aos_distance.km))
        print('----Propagation Delay: {} us'.format(aos_propagation_delay_us))
    else:
        print('--Estimated Pass Duration [s]:', pass_duration_s)
        print('--AOS at:', AOS_dt)
        print('----Altitude:',  round(aos_alt.degrees,2))
        print('----Azimuth:', round(aos_az.degrees,2))
        print('----Distance: {:.1f} km'.format(aos_distance.km))
        print('----Propagation Delay: {} us'.format(aos_propagation_delay_us))
        print('--TCA at:', TCA_dt)
        print('----Altitude:',  round(tca_alt.degrees,2))
        print('----Azimuth:', round(tca_az.degrees,2))
        print('----Distance: {:.1f} km'.format(tca_distance.km))
        print('----Propagation Delay: {} us'.format(tca_propagation_delay_us))
        print('--LOS at:', LOS_dt)
        print('----Altitude:',  round(los_alt.degrees,2))
        print('----Azimuth:', round(los_az.degrees,2))
        print('----Distance: {:.1f} km'.format(los_distance.km))
        print('----Propagation Delay: {} us'.format(los_propagation_delay_us))
        print("")

    return satellite, cell_center_subpoint, AOS_dt, LOS_dt

def generate_configs(ts, start_dt, stop_dt, satellite, ue_position, gw_position=None, ephemeris_info_format="ecef"):
    # Timestamps
    time_resolution = timedelta(seconds=1)
    start_dt = start_dt.astimezone(timezone.utc).replace(tzinfo=None)
    stop_dt = stop_dt.astimezone(timezone.utc).replace(tzinfo=None)
    timestamps_dt = np.arange(start_dt, stop_dt, time_resolution)
    timestamps_dt = [dt.replace(tzinfo=utc) for dt in timestamps_dt.astype('datetime64[us]').astype(datetime).tolist()]   
    timestamps_ts = ts.from_datetimes(list(timestamps_dt))
    duration_dt = stop_dt - start_dt

    # Satellite position trace
    satellite_position = satellite.at(timestamps_ts)
    sat_position, sat_velocity = satellite_position.frame_xyz_and_velocity(itrs)

    # Satelite - UE location
    difference = satellite - ue_position
    topocentric = difference.at(timestamps_ts)
    altitude, azimuth, distance = topocentric.altaz()
    latitude, longitude, ue_slant_range, latitude_rate, longitude_rate, range_rate = topocentric.frame_latlon_and_rates(ue_position)
    max_ue_slant_range = np.max(ue_slant_range.km)

    # Satelite - Gateway location
    max_gw_slant_range = 0
    if (gw_position is not None):
        difference = satellite - gw_position
        topocentric = difference.at(timestamps_ts)
        altitude, azimuth, distance = topocentric.altaz()
        latitude, longitude, gw_slant_range, latitude_rate, longitude_rate, range_rate = topocentric.frame_latlon_and_rates(ue_position)
        max_gw_slant_range = np.max(gw_slant_range.km)

    ### Compute parameters for gNB NTN config.
    # epochTime - only place holders, gnb will fill them correctly
    epoch_sfn = 0
    epoch_subframe_number = 0
    epoch_timestamp = start_dt

    # ntn-UlSyncValidityDuration - currently fixed value
    ntn_ul_sync_validity_duration = 5

    # cellSpecificKoffset
    speed_of_light_km_per_s = 299792.458
    max_distance = max_ue_slant_range + max_gw_slant_range
    max_rtt = 2 * max_distance / speed_of_light_km_per_s
    max_propagation_delay_ms = int(np.ceil(max_rtt*1e3))
    cell_specific_koffset = max_propagation_delay_ms

    # kmac - TODO

    # ta-Info
    if (gw_position is not None):
        # only placeholders, gnb will fill it properly
        ta_common = 2.0 * max_gw_slant_range / speed_of_light_km_per_s * 1e6 # us
        ta_common_drift = 0.0
        ta_common_drift_variant = 0.0
    else:
        ta_common = 0.0
        ta_common_drift = 0.0
        ta_common_drift_variant = 0.0

    # ntn-PolarizationDL - TODO
    # ntn-PolarizationUL - TODO
    # ta-Report - TODO

    # ephemerisInfo
    ### ECEF state vector
    sat_pos_x = sat_position.m[0][0]
    sat_pos_y = sat_position.m[1][0]
    sat_pos_z = sat_position.m[2][0]
    sat_vel_x = sat_velocity.m_per_s[0][0]
    sat_vel_y = sat_velocity.m_per_s[1][0]
    sat_vel_z = sat_velocity.m_per_s[2][0]

    # Orbital parameters
    # Convert ECEF state vectors to orbital parameters.
    # Note: The ECI and ECEF coincide at epochTime, i.e., x,y,z axis in ECEF are aligned with x,y,z axis in ECI at epochTime.
    converter = ReferenceFrameConverter(timestamps_dt[0])
    ecef_pos = np.array([sat_pos_x, sat_pos_y, sat_pos_z])
    ecef_vel = np.array([sat_vel_x, sat_vel_y, sat_vel_z])
    eci_rv = converter.ecef_to_eci(ecef_pos, ecef_vel, timestamps_dt[0])
    oe = EphemerisInfoConverter.rv_to_oe(eci_rv['position'], eci_rv['velocity'])

    ### Create gNB NTN config.
    gnb_ntn_config_dict = {}
    gnb_ntn_config_dict["ntn"] = {}
    gnb_ntn_config_dict["ntn"]["cell_specific_koffset"] = cell_specific_koffset
    gnb_ntn_config_dict["ntn"]["epoch_timestamp"] = epoch_timestamp.strftime('%Y-%m-%dT%H:%M:%S.%f')[:-3]
    gnb_ntn_config_dict["ntn"]["epoch_time"] = {}
    gnb_ntn_config_dict["ntn"]["epoch_time"]["sfn"] = epoch_sfn
    gnb_ntn_config_dict["ntn"]["epoch_time"]["subframe_number"] = epoch_subframe_number
    gnb_ntn_config_dict["ntn"]["ntn_ul_sync_validity_dur"] = ntn_ul_sync_validity_duration
    gnb_ntn_config_dict["ntn"]["ta_info"] = {}
    gnb_ntn_config_dict["ntn"]["ta_info"]["ta_common"] = ta_common
    gnb_ntn_config_dict["ntn"]["ta_info"]["ta_common_drift"] = ta_common_drift
    gnb_ntn_config_dict["ntn"]["ta_info"]["ta_common_drift_variant"] = ta_common_drift_variant
    if (ephemeris_info_format == "ecef"):
        gnb_ntn_config_dict["ntn"]["ephemeris_info_ecef"] = {
                                    "pos_x": sat_pos_x,
                                    "pos_y": sat_pos_y,
                                    "pos_z": sat_pos_z,
                                    "vel_x": sat_vel_x,
                                    "vel_y": sat_vel_y,
                                    "vel_z": sat_vel_z
                                }
    else:
        gnb_ntn_config_dict["ntn"]["ephemeris_orbital"] = {
                                    "semi_major_axis": oe.semi_major_axis,
                                    "eccentricity": oe.eccentricity,
                                    "inclination": oe.inclination,
                                    "longitude": oe.longitude,
                                    "periapsis": oe.periapsis,
                                    "mean_anomaly": oe.mean_anomaly
                                }
    return gnb_ntn_config_dict


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='NTN Config Generator.')
    parser.add_argument("--tle-fn", type=str, default="example_tle.txt", help="TLE data file path.")
    parser.add_argument("--pass-start-time", type=str, default=None, help="Pass UTC start time, format 'Y-m-dTH:M:S' [e.g. 2025-03-14T09:16:00]")
    parser.add_argument("--pass-start-offset", type=int, default=0, help="Pass start offset [s].")
    parser.add_argument("--min-sat-elevation", type=int, default=20, help="Minimal satellite elevetion [degrees].")
    parser.add_argument("--feeder-link-enabled", type=str_to_bool, nargs='?', const=True, help="Whether to enable feeder link compensation.")
    parser.add_argument("--fl-dl-freq-hz", type=float, default=2185e6, help="DL Center Frequency [Hz] for the feeder link.")
    parser.add_argument("--fl-ul-freq-hz", type=float, default=1995e6, help="DL Center Frequency [Hz] for the feeder link.")
    parser.add_argument("--ephemeris-info-format", type=str, default="ecef", help="Format of the ephemeris info in NTN config [ecef, orbital].")
    parser.add_argument("--use-state-vector", type=str_to_bool, nargs='?', const=True, help="Optional parameter for the gnb ntn config. Whether the gnb broadcast EphemerisInfo as ECEF state vectors or ECI Orbital parameters.")
    parser.add_argument("--enable-sat-switch-with-resync", action="store_true", help="Include sat_switch_with_resync section in output config using LOS-derived values.")
    cfg = parser.parse_args()

    # Find real time NTN scenario.
    ts = load.timescale()
    satellite, cell_center, start_dt, stop_dt = find_real_time_ntn_scenario(ts, cfg.tle_fn, cfg.min_sat_elevation, cfg.pass_start_offset, cfg.pass_start_time)

    ue_location = wgs84.latlon(cell_center.latitude.degrees, cell_center.longitude.degrees, 1)
    print("UE Position: ")
    print('--Latitude [deg]:', ue_location.latitude.degrees)
    print('--Longitude [deg]:', ue_location.longitude.degrees)
    print('--Elevation [m]:' , ue_location.elevation.m)
    print("")

    gw_location = None
    if (cfg.feeder_link_enabled):
        gw_location = wgs84.latlon(cell_center.latitude.degrees, cell_center.longitude.degrees, 1)
        print("Gateway Position: ")
        print('--Latitude [deg]:', gw_location.latitude.degrees)
        print('--Longitude [deg]:', gw_location.longitude.degrees)
        print('--Elevation [m]:' , gw_location.elevation.m)
        print("")

    # Generate NTN configs.
    gnb_ntn_config_dict = generate_configs(ts, start_dt, stop_dt, satellite, ue_location, gw_location, cfg.ephemeris_info_format)

    # Add extra srsgnb NTN config parameters.
    gnb_ntn_config_dict["ntn"]["ntn_ul_sync_validity_dur"] = 5
    # Add extra gnb config parameters
    if (cfg.use_state_vector is not None):
        gnb_ntn_config_dict["ntn"]["use_state_vector"] = cfg.use_state_vector
    # Add Feeder Link related config.
    if (cfg.feeder_link_enabled):
        gnb_ntn_config_dict["ntn"]["feeder_link"] = {
                "enable_doppler_compensation": cfg.feeder_link_enabled,
                "dl_freq": cfg.fl_dl_freq_hz,
                "ul_freq": cfg.fl_ul_freq_hz
            }
        gnb_ntn_config_dict["ntn"]["gateway_location"] = {
                "latitude": gw_location.latitude.degrees,
                "longitude": gw_location.longitude.degrees,
                "altitude": gw_location.elevation.m
            }

    # Optionally add sat-switch section using values from serving cell (TODO: should be a different satellite).
    if cfg.enable_sat_switch_with_resync:
        sat_switch_cfg = {
            "epoch_timestamp": gnb_ntn_config_dict["ntn"]["epoch_timestamp"],
            "t_service_start": stop_dt.strftime('%Y-%m-%dT%H:%M:%S.%f')[:-3],
            "ntn_cfg": {
                "cell_specific_koffset": gnb_ntn_config_dict["ntn"]["cell_specific_koffset"],
                "ntn_ul_sync_validity_dur": gnb_ntn_config_dict["ntn"]["ntn_ul_sync_validity_dur"],
            }
        }

        if gw_location is not None:
            sat_switch_cfg["ntn_gateway_location"] = {
                "latitude": gw_location.latitude.degrees,
                "longitude": gw_location.longitude.degrees,
                "altitude": gw_location.elevation.m
            }
        if "ta_info" in gnb_ntn_config_dict["ntn"]:
            sat_switch_cfg["ntn_cfg"]["ta_info"] = dict(gnb_ntn_config_dict["ntn"]["ta_info"])
        if "ephemeris_info_ecef" in gnb_ntn_config_dict["ntn"]:
            sat_switch_cfg["ntn_cfg"]["ephemeris_info_ecef"] = dict(gnb_ntn_config_dict["ntn"]["ephemeris_info_ecef"])
        elif "ephemeris_orbital" in gnb_ntn_config_dict["ntn"]:
            sat_switch_cfg["ntn_cfg"]["ephemeris_orbital"] = dict(gnb_ntn_config_dict["ntn"]["ephemeris_orbital"])

        gnb_ntn_config_dict["ntn"]["sat_switch_with_resync"] = sat_switch_cfg

    # Add TA-CMD related config.
    ntn_cell_cfg = {}
    ntn_cell_cfg["cell_cfg"] = {
        "ta": {
            "ta_target": 0,
            "ta_measurement_slot_prohibit_period": gnb_ntn_config_dict["ntn"]["cell_specific_koffset"] + 10,
            "ta_measurement_slot_period": 1000,
            "ta_cmd_offset_threshold": 1,
            "ta_outlier_detection_zscore_threshold": 0.0,
        },
        "ntn": gnb_ntn_config_dict["ntn"]
    }

    gnb_ntn_cfg = "ntn.yml"
    save_gnb_ntn_config(gnb_ntn_cfg, ntn_cell_cfg)
    print("Saved NTN config to file:  ", gnb_ntn_cfg)

    ue_position_cfg_fn = "ue-position.cfg"
    save_ground_position_cfg(ue_position_cfg_fn, ue_location)
    print("Saved UE position to file: ", ue_position_cfg_fn)

    if (gw_location is not None):
        gw_position_cfg_fn = "gw-position.cfg"
        save_ground_position_cfg(gw_position_cfg_fn, gw_location)
        print("Saved GW position to file: ", gw_position_cfg_fn)
