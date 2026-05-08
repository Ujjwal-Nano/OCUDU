// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "metrics_config_cli11_schema.h"
#include "metrics_appconfig.h"
#include "ocudu/support/cli11_utils.h"

using namespace ocudu;
using namespace app_services;

static void configure_cli11_metrics_args(CLI::App& app, metrics_appconfig& config)
{
  CLI::App* periodicity_subcmd =
      add_subcommand(app, "periodicity", "Metrics periodicity configuration")->configurable();
  add_option(*periodicity_subcmd,
             "--app_usage_report_period",
             config.app_usage_report_period,
             "Application usage report period in milliseconds")
      ->capture_default_str();
}

void ocudu::app_services::configure_cli11_with_metrics_appconfig_schema(CLI::App& app, metrics_appconfig& config)
{
  CLI::App* metrics_subcmd = add_subcommand(app, "metrics", "Metrics configuration")->configurable();
  configure_cli11_metrics_args(*metrics_subcmd, config);
}
