// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "du_high_ntn_config_schema.h"
#include "apps/helpers/config/config_builder.h"
#include "du_high_unit_cell_ntn_config.h"

using namespace ocudu;

#ifndef OCUDU_HAS_ENTERPRISE_NTN

void ocudu::configure_cli11_advanced_ntn_args(config::config_builder& /*b*/, du_high_unit_cell_ntn_config& /*config*/)
{
  // Advanced NTN config parameters are not implemented.
}

void ocudu::configure_cli11_advanced_ntn_args(CLI::App& /*app*/, du_high_unit_cell_ntn_config& /*config*/)
{
  // Advanced NTN config parameters are not implemented.
}
#endif // OCUDU_HAS_ENTERPRISE_NTN

static void declare_epoch_time(config::config_builder& b, epoch_time_t& epoch_time)
{
  b.option("--sfn", epoch_time.sfn, "SFN Part").range(0, 1023);
  b.option("--subframe_number", epoch_time.subframe_number, "Sub-frame number Part").range(0, 9);
}

static void declare_ta_info(config::config_builder& b, ta_info_t& ta_info)
{
  b.option("--ta_common", ta_info.ta_common, "TA common").range(0.0, 270730.0);
  b.option("--ta_common_drift", ta_info.ta_common_drift, "Drift rate of the common TA").range(-51.4606, 51.4606);
  b.option("--ta_common_drift_variant", ta_info.ta_common_drift_variant, "Drift rate variation of the common TA")
      .range(0.0, 0.57898);
  b.option("--ta_common_offset", ta_info.ta_common_offset, "Constant offset added to TA common").range(0.0, 10000.0);
}

static void declare_ephemeris_info_ecef(config::config_builder& b, ecef_coordinates_t& ephemeris_info)
{
  b.option("--pos_x", ephemeris_info.position_x, "X Position of the satellite [m]").range(-43620761.6, 43620759.3);
  b.option("--pos_y", ephemeris_info.position_y, "Y Position of the satellite [m]").range(-43620761.6, 43620759.3);
  b.option("--pos_z", ephemeris_info.position_z, "Z Position of the satellite [m]").range(-43620761.6, 43620759.3);
  b.option("--vel_x", ephemeris_info.velocity_vx, "X Velocity of the satellite [m/s]").range(-7864.32, 7864.26);
  b.option("--vel_y", ephemeris_info.velocity_vy, "Y Velocity of the satellite [m/s]").range(-7864.32, 7864.26);
  b.option("--vel_z", ephemeris_info.velocity_vz, "Z Velocity of the satellite [m/s]").range(-7864.32, 7864.26);
}

static void declare_ephemeris_info_orbital(config::config_builder& b, orbital_coordinates_t& ephemeris_info)
{
  b.option("--semi_major_axis", ephemeris_info.semi_major_axis, "Semi-major axis of the satellite [m]")
      .range(6500000.0, 42998632.07);
  b.option("--eccentricity", ephemeris_info.eccentricity, "Eccentricity of the satellite [-]")
      .range(0.0, 0.01500510825);
  b.option("--periapsis", ephemeris_info.periapsis, "Periapsis of the satellite [rad]").range(0.0, 6.28407400155);
  b.option("--longitude",
           ephemeris_info.longitude,
           "Longitude of the satellites angle of ascending node [rad]")
      .range(0.0, 6.28407400155);
  b.option("--inclination", ephemeris_info.inclination, "Inclination of the satellite [rad]")
      .range(-1.57101850624, 1.57101848283);
  b.option("--mean_anomaly", ephemeris_info.mean_anomaly, "Mean anomaly of the satellite [rad]")
      .range(0.0, 6.28407400155);
}

static void declare_ntn_polarization(config::config_builder& b, ntn_polarization_t& polarization)
{
  // The optional<polarization_type> targets are set via string_action: the legacy CLI11 binding mapped "lhcp"/"rhcp"/
  // any other value to the enum. We preserve that behaviour by emplacing the optional in the setter and rendering the
  // current selection in the getter. Unknown values fall back to "linear" matching the previous logic.
  auto make_polarization_action =
      [](std::optional<ntn_polarization_t::polarization_type>& target) {
        return std::make_pair(
            std::function<void(const std::string&)>([&target](const std::string& value) {
              if (value == "lhcp") {
                target = ntn_polarization_t::polarization_type::lhcp;
              } else if (value == "rhcp") {
                target = ntn_polarization_t::polarization_type::rhcp;
              } else {
                target = ntn_polarization_t::polarization_type::linear;
              }
            }),
            std::function<std::string()>([&target]() -> std::string {
              if (!target.has_value()) {
                return {};
              }
              switch (*target) {
                case ntn_polarization_t::polarization_type::lhcp:
                  return "lhcp";
                case ntn_polarization_t::polarization_type::rhcp:
                  return "rhcp";
                case ntn_polarization_t::polarization_type::linear:
                  return "linear";
              }
              return {};
            }));
      };

  auto [dl_setter, dl_getter] = make_polarization_action(polarization.dl);
  b.string_action("--dl",
                  std::move(dl_setter),
                  std::move(dl_getter),
                  "Polarization information for downlink transmission on service link",
                  "legal values: lhcp, rhcp, linear");

  auto [ul_setter, ul_getter] = make_polarization_action(polarization.ul);
  b.string_action("--ul",
                  std::move(ul_setter),
                  std::move(ul_getter),
                  "Polarization information for downlink transmission on service link",
                  "legal values: lhcp, rhcp, linear");
}

void ocudu::configure_cli11_ntn_config_args(config::config_builder& b, ntn_config& config)
{
  // Epoch time: emplace at declaration so child options can bind to its fields. Presence-vs-absence
  // detection across the optional boundary is not yet expressed by the builder API; the optional will
  // therefore always be populated after parsing (TODO: port the legacy parse_complete_callback gating
  // to a runtime validator).
  if (!config.epoch_time.has_value()) {
    config.epoch_time.emplace();
  }
  b.group("epoch_time", "Epoch time for the NTN assistance info",
          [&](config::config_builder& g) { declare_epoch_time(g, *config.epoch_time); });

  // ntn_ul_sync_validity_dur is std::optional<unsigned>; the builder's option<optional<T>> handles it.
  b.option("--ntn_ul_sync_validity_dur", config.ntn_ul_sync_validity_dur, "An UL sync validity duration")
      .note("legal values: {5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 120, 180, 240, 900}");

  // cell_specific_koffset is std::optional<std::chrono::milliseconds>. The builder accepts integer input and stores it
  // as milliseconds via the chrono support in describe_scalar/format_default.
  b.option("--cell_specific_koffset",
           config.cell_specific_koffset,
           "Cell-specific k-offset to be used for NTN [ms].")
      .range(1, 1023);

  if (!config.ta_info.has_value()) {
    config.ta_info.emplace();
  }
  b.group("ta_info", "TA Info for the NTN assistance information",
          [&](config::config_builder& g) { declare_ta_info(g, *config.ta_info); });

  if (!config.polarization.has_value()) {
    config.polarization.emplace();
  }
  b.group("polarization",
          "Polarization information for downlink/uplink transmission",
          [&](config::config_builder& g) { declare_ntn_polarization(g, *config.polarization); });

  // The variant ephemeris_info is held as an optional in ntn_config; ensure one side is held so child options can
  // bind. Legacy CLI11 picked the alternative based on which subcommand was used, then emplaced the optional via
  // parse_complete_callback. Without that callback we declare both subcommands; the last one parsed wins because
  // the assignment in the configurator replaces the held alternative.
  // ECEF state vector branch:
  b.group("ephemeris_info_ecef",
          "Ephermeris information of the satellite in ecef coordinates",
          [&](config::config_builder& g) {
            if (!config.ephemeris_info.has_value() ||
                !std::holds_alternative<ecef_coordinates_t>(*config.ephemeris_info)) {
              config.ephemeris_info = ecef_coordinates_t{};
            }
            declare_ephemeris_info_ecef(g, std::get<ecef_coordinates_t>(*config.ephemeris_info));
          });

  b.group("ephemeris_orbital",
          "Ephermeris information of the satellite in orbital coordinates",
          [&](config::config_builder& g) {
            if (!config.ephemeris_info.has_value() ||
                !std::holds_alternative<orbital_coordinates_t>(*config.ephemeris_info)) {
              config.ephemeris_info = orbital_coordinates_t{};
            }
            declare_ephemeris_info_orbital(g, std::get<orbital_coordinates_t>(*config.ephemeris_info));
          });

  b.option("--ta_report",
           config.ta_report,
           "When this field is included in SIB19, it indicates reporting of timing advanced is enabled");
}

void ocudu::configure_cli11_ntn_config_args(CLI::App& app, ntn_config& config)
{
  config::schema_node    discard;
  discard.body = config::group_node{};
  config::config_builder b(app, discard);
  configure_cli11_ntn_config_args(b, config);
}

static void declare_ntn_cell_args(config::config_builder& b, du_high_unit_cell_ntn_config& config)
{
  // cell_specific_koffset on the per-cell config is std::chrono::milliseconds (not optional).
  b.option("--cell_specific_koffset",
           config.cell_specific_koffset,
           "Cell-specific k-offset to be used for NTN [ms].")
      .range(1, 1023);

  // ntn_ul_sync_validity_dur is std::optional<unsigned>.
  b.option("--ntn_ul_sync_validity_dur", config.ntn_ul_sync_validity_dur, "An UL sync validity duration")
      .note("legal values: {5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 120, 180, 240, 900}");

  if (!config.epoch_time.has_value()) {
    config.epoch_time.emplace();
  }
  b.group("epoch_time",
          "Epoch time for the NTN assistance information",
          [&](config::config_builder& g) { declare_epoch_time(g, *config.epoch_time); });

  if (!config.ta_info.has_value()) {
    config.ta_info.emplace();
  }
  b.group("ta_info",
          "TA Info for the NTN assistance information",
          [&](config::config_builder& g) { declare_ta_info(g, *config.ta_info); });

  // ephemeris_info is std::variant<ecef_coordinates_t, orbital_coordinates_t> (no optional wrapper).
  b.group("ephemeris_info_ecef",
          "Ephermeris information of the satellite in ecef coordinates",
          [&](config::config_builder& g) {
            if (!std::holds_alternative<ecef_coordinates_t>(config.ephemeris_info)) {
              config.ephemeris_info = ecef_coordinates_t{};
            }
            declare_ephemeris_info_ecef(g, std::get<ecef_coordinates_t>(config.ephemeris_info));
          });

  b.group("ephemeris_orbital",
          "Ephermeris information of the satellite in orbital coordinates",
          [&](config::config_builder& g) {
            if (!std::holds_alternative<orbital_coordinates_t>(config.ephemeris_info)) {
              config.ephemeris_info = orbital_coordinates_t{};
            }
            declare_ephemeris_info_orbital(g, std::get<orbital_coordinates_t>(config.ephemeris_info));
          });

  configure_cli11_advanced_ntn_args(b, config);
}

void ocudu::configure_cli11_cell_ntn_args(config::config_builder&                      b,
                                          std::optional<du_high_unit_cell_ntn_config>& cell_ntn_params)
{
  // The optional cell NTN config is emplaced at declaration so child options can bind. The legacy CLI11 binding gated
  // the emplace on whether the "ntn" subcommand was used (via parse_complete_callback); that gating is not yet
  // expressible through the builder API and must be re-introduced via a runtime validator. (TODO)
  if (!cell_ntn_params.has_value()) {
    cell_ntn_params.emplace();
  }
  b.group("ntn", "NTN configuration",
          [&](config::config_builder& g) { declare_ntn_cell_args(g, *cell_ntn_params); });
}

void ocudu::configure_cli11_cell_ntn_args(CLI::App&                                    app,
                                          std::optional<du_high_unit_cell_ntn_config>& cell_ntn_params)
{
  config::schema_node    discard;
  discard.body = config::group_node{};
  config::config_builder b(app, discard);
  configure_cli11_cell_ntn_args(b, cell_ntn_params);
}
