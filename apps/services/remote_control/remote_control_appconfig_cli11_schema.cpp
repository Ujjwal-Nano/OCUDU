// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "remote_control_appconfig_cli11_schema.h"
#include "remote_control_appconfig.h"
#include "ocudu/support/cli11_utils.h"

using namespace ocudu;

static void configure_cli11_remote_control_args(CLI::App& app, remote_control_appconfig& config)
{
  add_option(app, "--enabled", config.enabled, "Enables the Remote Control Server")->always_capture_default();
  add_option(app, "--bind_addr", config.bind_addr, "Remote Control Server bind address")->capture_default_str();
  add_option(app, "--port", config.port, "Port where the remote control server listens for incoming connections")
      ->capture_default_str()
      ->check(CLI::Range(0, 65535));
}

static void configure_cli11_metrics_args(CLI::App& app, remote_control_appconfig& config)
{
  add_option(app, "--enable_json", config.enable_metrics_subscription, "Enable JSON metrics reporting")
      ->always_capture_default();
}

void ocudu::configure_cli11_with_remote_control_appconfig_schema(CLI::App& app, remote_control_appconfig& config)
{
  CLI::App* remote_control_subcmd =
      add_subcommand(app, "remote_control", "Remote control configuration")->configurable();
  configure_cli11_remote_control_args(*remote_control_subcmd, config);
  CLI::App* metrics_subcmd = add_subcommand(app, "metrics", "Metrics configuration")->configurable();
  configure_cli11_metrics_args(*metrics_subcmd, config);
}
