// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "metrics_config_cli11_schema.h"
#include "metrics_config.h"
#include "ocudu/support/cli11_utils.h"

using namespace ocudu;
using namespace app_helpers;

static void configure_cli11_metrics_args(CLI::App& app, metrics_config& config)
{
  add_option(app, "--enable_json", config.enable_json_metrics, "Enables the metrics in JSON format")
      ->always_capture_default();
  add_option(app, "--enable_log", config.enable_log_metrics, "Enables the metrics in the log")
      ->always_capture_default();
  add_option(app, "--enable_verbose", config.enable_verbose, "Enable extended detail metrics reporting")
      ->always_capture_default();
}

void ocudu::app_helpers::configure_cli11_with_metrics_appconfig_schema(CLI::App& app, metrics_config& config)
{
  CLI::App* metrics_subcmd = add_subcommand(app, "metrics", "Metrics configuration")->configurable();
  configure_cli11_metrics_args(*metrics_subcmd, config);
}
