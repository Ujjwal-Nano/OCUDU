// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "fapi_config_cli11_schema.h"
#include "apps/helpers/config/config_builder.h"
#include "apps/helpers/logger/logger_appconfig_cli11_utils.h"
#include "fapi_config.h"

using namespace ocudu;

void ocudu::configure_cli11_with_fapi_config_schema(config::config_builder& b, fapi_unit_config& parsed_cfg)
{
  b.group("log", "Logging configuration", [&](config::config_builder& log_b) {
    app_helpers::add_log_option(log_b, parsed_cfg.fapi_level, "--fapi_level", "FAPI log level")
        .fallback_from("--all_level");
  });
}

void ocudu::configure_cli11_with_fapi_config_schema(CLI::App& app, fapi_unit_config& parsed_cfg)
{
  config::schema_node discard;
  discard.body = config::group_node{};
  config::config_builder b(app, discard);
  configure_cli11_with_fapi_config_schema(b, parsed_cfg);
}
