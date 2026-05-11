// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "metrics_config_cli11_schema.h"
#include "apps/helpers/config/config_builder.h"
#include "metrics_appconfig.h"

using namespace ocudu;
using namespace app_services;

void ocudu::app_services::configure_cli11_with_metrics_appconfig_schema(config::config_builder& b,
                                                                        metrics_appconfig&      config)
{
  b.group("metrics", "Metrics configuration", [&](config::config_builder& m) {
    m.group("periodicity", "Metrics periodicity configuration", [&](config::config_builder& p) {
      p.option("--app_usage_report_period",
               config.app_usage_report_period,
               "Application resource usage metrics report period (in milliseconds)");
    });
  });
}

void ocudu::app_services::configure_cli11_with_metrics_appconfig_schema(CLI::App& app, metrics_appconfig& config)
{
  config::schema_node discard;
  discard.body = config::group_node{};
  config::config_builder b(app, discard);
  configure_cli11_with_metrics_appconfig_schema(b, config);
}
