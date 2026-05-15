// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "fapi_config_schema.h"
#include "apps/helpers/config/config_builder.h"
#include "apps/helpers/logger/logger_appconfig_cli11_utils.h"
#include "fapi_config.h"

using namespace ocudu;

void ocudu::declare_fapi_config_schema(config::config_builder& b, fapi_unit_config& parsed_cfg)
{
  b.group("log", "Logging configuration", [&](config::config_builder& log_b) {
    app_helpers::add_log_option(log_b, parsed_cfg.fapi_level, "--fapi_level", "FAPI log level")
        .fallback_from("--all_level");
  });
}

