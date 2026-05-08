// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "app_resource_usage_config_cli11_schema.h"
#include "app_resource_usage_config.h"
#include "apps/helpers/metrics/metrics_config_cli11_schema.h"
#include "ocudu/support/cli11_utils.h"

using namespace ocudu;
using namespace app_services;

static void configure_cli11_metrics_args(CLI::App& app, app_resource_usage_config& config)
{
  CLI::App* layers_subcmd = add_subcommand(app, "layers", "Layer basis metrics configuration")->configurable();
  add_option(*layers_subcmd, "--enable_app_usage", config.enable_app_usage, "Enable application usage metrics")
      ->capture_default_str();
}

void ocudu::app_services::configure_cli11_with_app_resource_usage_config_schema(CLI::App&                  app,
                                                                                app_resource_usage_config& config)
{
  CLI::App* metrics_subcmd = add_subcommand(app, "metrics", "Metrics configuration")->configurable();
  configure_cli11_metrics_args(*metrics_subcmd, config);
  app_helpers::configure_cli11_with_metrics_appconfig_schema(app, config.metrics_consumers_cfg);
}
