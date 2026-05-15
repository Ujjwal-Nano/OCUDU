// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "cu_appconfig_schema.h"
#include "apps/helpers/f1u/f1u_cli11_schema.h"
#include "apps/helpers/logger/logger_appconfig_cli11_schema.h"
#include "apps/helpers/network/sctp_cli11_schema.h"
#include "apps/services/app_resource_usage/app_resource_usage_config_cli11_schema.h"
#include "apps/services/buffer_pool/buffer_pool_appconfig_cli11_schema.h"
#include "apps/services/metrics/metrics_config_cli11_schema.h"
#include "apps/services/remote_control/remote_control_appconfig_cli11_schema.h"
#include "apps/services/worker_manager/worker_manager_cli11_schema.h"
#include "cu_appconfig.h"

using namespace ocudu;

void ocudu::declare_cu_appconfig_schema(config::config_builder& root, cu_appconfig& cu_cfg)
{
  root.flag("--dryrun", cu_cfg.enable_dryrun, "Enable application dry run mode");

  configure_cli11_with_logger_appconfig_schema(root, cu_cfg.log_cfg);
  app_services::configure_cli11_with_buffer_pool_appconfig_schema(root, cu_cfg.buffer_pool_config);
  configure_cli11_with_worker_manager_appconfig_schema(root, cu_cfg.expert_execution_cfg);
  configure_cli11_with_remote_control_appconfig_schema(root, cu_cfg.remote_control_config);
  app_services::configure_cli11_with_app_resource_usage_config_schema(root, cu_cfg.metrics_cfg.rusage_config);
  app_services::configure_cli11_with_metrics_appconfig_schema(root, cu_cfg.metrics_cfg.metrics_service_cfg);

  root.group("cu_cp", "CU-CP parameters", [&](config::config_builder& cu_cp_b) {
    cu_cp_b.group("f1ap", "F1AP parameters", [&](config::config_builder& f1ap_b) {
      f1ap_b.option(
          "--bind_addrs,--bind_addr",
          cu_cfg.f1ap_cfg.bind_addrs,
          "CU F1-C bind addresses. Multiple addresses can be specified for SCTP multi-homing");
      configure_cli11_sctp_socket_args(f1ap_b, cu_cfg.f1ap_cfg.sctp);
    });
  });

  root.group("cu_up", "CU-UP parameters", [&](config::config_builder& cu_up_b) {
    cu_up_b.group("f1u", "F1-U parameters",
                  [&](config::config_builder& f1u_b) { configure_cli11_f1u_sockets_args(f1u_b, cu_cfg.f1u_cfg); });
  });
}
