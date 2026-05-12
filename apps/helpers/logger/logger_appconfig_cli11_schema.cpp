// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "logger_appconfig_cli11_schema.h"
#include "apps/helpers/config/config_builder.h"
#include "logger_appconfig.h"
#include "logger_appconfig_cli11_utils.h"

using namespace ocudu;

static void declare_log_args(config::config_builder& b, logger_appconfig& cfg)
{
  b.option("--filename", cfg.filename, "Log file output path");

  // The "all_level" knob sets the cascading default for every other level
  // option declared elsewhere in the tree (per-unit, per-layer). The
  // post-parse cascade is driven by .fallback_from("--all_level") on each
  // dependent option's declaration site.
  app_helpers::add_log_option(
      b, cfg.all_level, "--all_level", "Default log level for PHY, MAC, RLC, PDCP, RRC, SDAP, NGAP and GTPU");

  // --lib_level is intentionally NOT cascaded from --all_level — generic
  // library code stays at its own configured level.
  app_helpers::add_log_option(b, cfg.lib_level, "--lib_level", "Generic log level");

  app_helpers::add_log_option(b, cfg.e2ap_level, "--e2ap_level", "E2AP log level").fallback_from("--all_level");
  app_helpers::add_log_option(b, cfg.config_level, "--config_level", "Config log level").fallback_from("--all_level");

  b.option("--hex_max_size",
           cfg.hex_max_size,
           "Maximum number of bytes to print in hex (zero for no hex dumps, -1 for unlimited bytes)")
      .range(-1, 1024);
}

void ocudu::configure_cli11_with_logger_appconfig_schema(config::config_builder& b, logger_appconfig& config)
{
  b.group("log", "Logging configuration", [&](config::config_builder& log_b) { declare_log_args(log_b, config); });
}

void ocudu::configure_cli11_with_logger_appconfig_schema(CLI::App& app, logger_appconfig& config)
{
  config::schema_node discard;
  discard.body = config::group_node{};
  config::config_builder b(app, discard);
  configure_cli11_with_logger_appconfig_schema(b, config);
}
