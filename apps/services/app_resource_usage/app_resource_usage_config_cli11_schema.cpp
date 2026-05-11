// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "app_resource_usage_config_cli11_schema.h"
#include "app_resource_usage_config.h"
#include "apps/helpers/config/config_builder.h"
#include "apps/helpers/metrics/metrics_config_cli11_schema.h"

using namespace ocudu;
using namespace app_services;

void ocudu::app_services::configure_cli11_with_app_resource_usage_config_schema(config::config_builder&    b,
                                                                                app_resource_usage_config& config)
{
  b.group("metrics", "Metrics configuration", [&](config::config_builder& m) {
    m.group("layers", "Layer basis metrics configuration", [&](config::config_builder& l) {
      l.option("--enable_app_usage", config.enable_app_usage, "Enable application usage metrics");
    });
  });
  // Adds enable_json/enable_log/enable_verbose under the same "metrics" group
  // via the group-reuse behaviour of config_builder.
  app_helpers::configure_cli11_with_metrics_appconfig_schema(b, config.metrics_consumers_cfg);
}

void ocudu::app_services::configure_cli11_with_app_resource_usage_config_schema(CLI::App&                  app,
                                                                                app_resource_usage_config& config)
{
  config::schema_node discard;
  discard.body = config::group_node{};
  config::config_builder b(app, discard);
  configure_cli11_with_app_resource_usage_config_schema(b, config);
}
