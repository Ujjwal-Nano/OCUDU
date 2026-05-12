// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "split6_o_du_low_unit_cli11_schema.h"
#include "apps/helpers/config/config_builder.h"
#include "apps/helpers/logger/logger_appconfig_cli11_utils.h"
#include "apps/units/flexible_o_du/o_du_low/du_low_config_cli11_schema.h"
#include "apps/units/flexible_o_du/split_7_2/helpers/ru_ofh_config_cli11_schema.h"
#include "apps/units/flexible_o_du/split_8/helpers/ru_sdr_config_cli11_schema.h"
#include "split6_constants.h"
#include "split6_o_du_low_unit_config.h"
#include "ocudu/ran/band_helper.h"
#include "ocudu/ran/slot_point_extended.h"
#include "ocudu/support/cli11_utils.h"

using namespace ocudu;

static ru_ofh_unit_parsed_config ofh_cfg;
static ru_sdr_unit_config        sdr_cfg;

void ocudu::configure_cli11_with_split6_o_du_low_unit_config_schema(config::config_builder&      b,
                                                                    split6_o_du_low_unit_config& config)
{
  configure_cli11_with_du_low_config_schema(b, config.du_low_cfg);
  configure_cli11_with_ru_ofh_config_schema(b, ofh_cfg);
  configure_cli11_with_ru_sdr_config_schema(b, sdr_cfg);

  b.option("--start_time_jitter",
           config.start_time_jitter_ms,
           "Start time jitter in milliseconds. A value of 0 disables the start time calculation and the session "
           "starts it as soon as possible")
      .range(0, 600);

  b.group("log", "Logger configuration", [&](config::config_builder& log_b) {
    app_helpers::add_log_option(log_b, config.fapi_level, "--fapi_level", "FAPI log level").fallback_from("--all_level");
  });

  b.group("metrics", "Metrics configuration", [&](config::config_builder& m) {
    m.group("periodicity", "Metrics periodicity configuration", [&](config::config_builder& p) {
      p.option(
           "--du_report_period", config.du_report_period, "DU statistics report period in milliseconds")
          .range(0U, static_cast<unsigned>(NOF_SUBFRAMES_PER_FRAME * NOF_SFNS * NOF_HYPER_SFNS));
    });
  });
}

void ocudu::configure_cli11_with_split6_o_du_low_unit_config_schema(CLI::App&                    app,
                                                                    split6_o_du_low_unit_config& config)
{
  config::schema_node discard;
  discard.body = config::group_node{};
  config::config_builder b(app, discard);
  configure_cli11_with_split6_o_du_low_unit_config_schema(b, config);
}

static void manage_ru(const CLI::App& app, split6_o_du_low_unit_config& config)
{
  // Manage the RU optionals
  auto     ofh_subcmd      = app.get_subcommand("ru_ofh");
  auto     sdr_subcmd      = app.get_subcommand("ru_sdr");
  unsigned nof_ofh_entries = ofh_subcmd->count_all();
  unsigned nof_sdr_entries = sdr_subcmd->count_all();

  // Count the number of RU types.
  unsigned nof_ru_types = (nof_ofh_entries != 0) ? 1 : 0;
  nof_ru_types += (nof_sdr_entries != 0) ? 1 : 0;

  if (nof_ru_types > 1) {
    ocudu_terminate(
        "Radio Unit configuration allows either a SDR or Open Fronthaul but not both types at the same time");
  }

  if (nof_ofh_entries != 0) {
    config.ru_cfg = ofh_cfg;
    sdr_subcmd->disabled();

    return;
  }

  config.ru_cfg = sdr_cfg;
  ofh_subcmd->disabled();
}

void ocudu::autoderive_split6_o_du_low_parameters_after_parsing(CLI::App& app, split6_o_du_low_unit_config& config)
{
  if (CLI::App* expert_cmd = app.get_subcommand("expert_phy");
      expert_cmd->count_all() == 0 || expert_cmd->count("--max_proc_delay") == 0) {
    report_error("'max_proc_delay' property is mandatory in this configuration");
  }

  // Auto derive SDR parameters.
  autoderive_ru_sdr_parameters_after_parsing(app, sdr_cfg, split6_du_low::NOF_CELLS_SUPPORTED);
  // Auto derive OFH parameters.
  autoderive_ru_ofh_parameters_after_parsing(app, ofh_cfg);

  // Set the parsed RU.
  manage_ru(app, config);

  // NOTE: TDD is hardcoded because it does not matter as max proc delay parameter is mandatory in this application
  // unit.
  autoderive_du_low_parameters_after_parsing(app, config.du_low_cfg, duplex_mode::TDD);
}
