// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "cu_cp_appconfig_schema.h"
#include "apps/helpers/logger/logger_appconfig_cli11_schema.h"
#include "apps/helpers/network/sctp_cli11_schema.h"
#include "apps/helpers/tracing/tracer_appconfig_cli11_schema.h"
#include "apps/services/app_resource_usage/app_resource_usage_config_cli11_schema.h"
#include "apps/services/buffer_pool/buffer_pool_appconfig_cli11_schema.h"
#include "apps/services/metrics/metrics_config_cli11_schema.h"
#include "apps/services/remote_control/remote_control_appconfig_cli11_schema.h"
#include "apps/services/worker_manager/worker_manager_cli11_schema.h"
#include "cu_cp_appconfig.h"

using namespace ocudu;

void ocudu::declare_cu_cp_appconfig_schema(config::config_builder& root, cu_cp_appconfig& cu_cp_cfg)
{
  root.flag("--dryrun", cu_cp_cfg.enable_dryrun, "Enable application dry run mode");

  configure_cli11_with_logger_appconfig_schema(root, cu_cp_cfg.log_cfg);
  configure_cli11_with_tracer_appconfig_schema(root, cu_cp_cfg.trace_cfg);
  app_services::configure_cli11_with_buffer_pool_appconfig_schema(root, cu_cp_cfg.buffer_pool_config);
  configure_cli11_with_worker_manager_appconfig_schema(root, cu_cp_cfg.expert_execution_cfg);
  configure_cli11_with_remote_control_appconfig_schema(root, cu_cp_cfg.remote_control_config);
  app_services::configure_cli11_with_app_resource_usage_config_schema(root, cu_cp_cfg.metrics_cfg.rusage_config);
  app_services::configure_cli11_with_metrics_appconfig_schema(root, cu_cp_cfg.metrics_cfg.metrics_service_cfg);

  root.group("cu_cp", "CU-CP parameters", [&](config::config_builder& cu_cp_b) {
    cu_cp_b.group("e1ap", "E1AP parameters", [&](config::config_builder& e1ap_b) {
      e1ap_b.option("--bind_addrs,--bind_addr", cu_cp_cfg.e1ap_cfg.bind_addrs, "E1 bind addresses");
      configure_cli11_sctp_socket_args(e1ap_b, cu_cp_cfg.e1ap_cfg.sctp);
    });
    cu_cp_b.group("f1ap", "F1AP parameters", [&](config::config_builder& f1ap_b) {
      f1ap_b.option(
          "--bind_addrs,--bind_addr",
          cu_cp_cfg.f1ap_cfg.bind_addrs,
          "CU-CP F1-C bind addresses. Multiple addresses can be specified for SCTP multi-homing");
      configure_cli11_sctp_socket_args(f1ap_b, cu_cp_cfg.f1ap_cfg.sctp);
    });
  });
}
