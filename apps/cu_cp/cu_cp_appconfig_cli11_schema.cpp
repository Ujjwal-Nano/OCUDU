// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "cu_cp_appconfig_cli11_schema.h"
#include "apps/helpers/logger/logger_appconfig_cli11_schema.h"
#include "apps/helpers/network/sctp_cli11_schema.h"
#include "apps/helpers/tracing/tracer_appconfig_cli11_schema.h"
#include "apps/services/app_resource_usage/app_resource_usage_config_cli11_schema.h"
#include "apps/services/buffer_pool/buffer_pool_appconfig_cli11_schema.h"
#include "apps/services/metrics/metrics_config_cli11_schema.h"
#include "apps/services/remote_control/remote_control_appconfig_cli11_schema.h"
#include "apps/services/worker_manager/worker_manager_cli11_schema.h"
#include "cu_cp_appconfig.h"
#include "ocudu/support/cli11_utils.h"

using namespace ocudu;

static void configure_cli11_e1ap_args(CLI::App& app, ocucp::e1ap_appconfig& e1ap_params)
{
  add_option(
      app,
      "--bind_addrs,--bind_addr", // TODO: old name kept for backward compatibility, should be removed in the future
      e1ap_params.bind_addrs,
      "E1 bind addresses")
      ->capture_default_str();
  configure_cli11_sctp_socket_args(app, e1ap_params.sctp);
}

static void configure_cli11_f1ap_args(CLI::App& app, ocucp::f1ap_appconfig& f1ap_params)
{
  add_option(
      app,
      "--bind_addrs,--bind_addr", // TODO: old name kept for backward compatibility, should be removed in the future
      f1ap_params.bind_addrs,
      "CU-CP F1-C bind addresses. Multiple addresses can be specified for SCTP multi-homing")
      ->capture_default_str();
  configure_cli11_sctp_socket_args(app, f1ap_params.sctp);
}

void ocudu::configure_cli11_with_cu_cp_appconfig_schema(CLI::App& app, cu_cp_appconfig& config)
{
  app.add_flag("--dryrun", config.enable_dryrun, "Enable application dry run mode")->capture_default_str();
  configure_cli11_with_logger_appconfig_schema(app, config.log_cfg);
  configure_cli11_with_tracer_appconfig_schema(app, config.trace_cfg);
  configure_cli11_with_buffer_pool_appconfig_schema(app, config.buffer_pool_config);
  configure_cli11_with_worker_manager_appconfig_schema(app, config.expert_execution_cfg);
  configure_cli11_with_remote_control_appconfig_schema(app, config.remote_control_config);
  app_services::configure_cli11_with_app_resource_usage_config_schema(app, config.metrics_cfg.rusage_config);
  app_services::configure_cli11_with_metrics_appconfig_schema(app, config.metrics_cfg.metrics_service_cfg);
  CLI::App* cu_cp_subcmd = add_subcommand(app, "cu_cp", "CU-CP parameters")->configurable();
  CLI::App* e1ap_subcmd  = add_subcommand(*cu_cp_subcmd, "e1ap", "E1AP parameters")->configurable();
  configure_cli11_e1ap_args(*e1ap_subcmd, config.e1ap_cfg);
  CLI::App* f1ap_subcmd = add_subcommand(*cu_cp_subcmd, "f1ap", "F1AP parameters")->configurable();
  configure_cli11_f1ap_args(*f1ap_subcmd, config.f1ap_cfg);
}
