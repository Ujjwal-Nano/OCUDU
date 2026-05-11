// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "metrics_config_cli11_schema.h"
#include "apps/helpers/config/config_builder.h"
#include "metrics_config.h"

using namespace ocudu;
using namespace app_helpers;

void ocudu::app_helpers::configure_cli11_with_metrics_appconfig_schema(config::config_builder& b, metrics_config& config)
{
  b.group("metrics", "Metrics configuration", [&](config::config_builder& m) {
    m.option("--enable_json", config.enable_json_metrics, "Enable JSON metrics reporting");
    m.option("--enable_log", config.enable_log_metrics, "Enable log metrics reporting");
    m.option("--enable_verbose", config.enable_verbose, "Enable extended detail metrics reporting");
  });
}

void ocudu::app_helpers::configure_cli11_with_metrics_appconfig_schema(CLI::App& app, metrics_config& config)
{
  config::schema_node discard;
  discard.body = config::group_node{};
  config::config_builder b(app, discard);
  configure_cli11_with_metrics_appconfig_schema(b, config);
}
