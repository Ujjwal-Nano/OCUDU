// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "cu_up_appconfig_schema.h"
#include "apps/helpers/f1u/f1u_schema.h"
#include "apps/helpers/logger/logger_appconfig_schema.h"
#include "apps/helpers/network/sctp_schema.h"
#include "apps/helpers/tracing/tracer_appconfig_schema.h"
#include "apps/services/app_resource_usage/app_resource_usage_config_schema.h"
#include "apps/services/buffer_pool/buffer_pool_appconfig_schema.h"
#include "apps/services/metrics/metrics_config_schema.h"
#include "apps/services/remote_control/remote_control_appconfig_schema.h"
#include "apps/services/worker_manager/worker_manager_schema.h"
#include "cu_up_appconfig.h"

using namespace ocudu;

void ocudu::declare_cu_up_appconfig_schema(config::config_builder& root, cu_up_appconfig& cu_up_cfg)
{
  root.flag("--dryrun", cu_up_cfg.enable_dryrun, "Enable application dry run mode");

  declare_logger_appconfig_schema(root, cu_up_cfg.log_cfg);
  declare_tracer_appconfig_schema(root, cu_up_cfg.trace_cfg);
  app_services::declare_buffer_pool_appconfig_schema(root, cu_up_cfg.buffer_pool_config);
  declare_worker_manager_appconfig_schema(root, cu_up_cfg.expert_execution_cfg);
  declare_remote_control_appconfig_schema(root, cu_up_cfg.remote_control_config);
  app_services::declare_app_resource_usage_config_schema(root, cu_up_cfg.metrics_cfg.rusage_config);
  app_services::declare_metrics_appconfig_schema(root, cu_up_cfg.metrics_cfg.metrics_service_cfg);

  root.group("cu_up", "CU-UP parameters", [&](config::config_builder& cu_up_b) {
    cu_up_b.group("e1ap", "E1AP parameters", [&](config::config_builder& e1ap_b) {
      e1ap_b.option("--addrs,--cu_cp_addr",
                    cu_up_cfg.e1ap_cfg.cu_cp_addresses,
                    "CU-CP addresses to be used for E1 interface. Multiple addresses can be specified for SCTP multi-homing");
      e1ap_b.option(
          "--bind_addrs,--bind_addr",
          cu_up_cfg.e1ap_cfg.bind_addresses,
          "CU-UP bind addresses to be used for E1 interface. Multiple addresses can be specified for SCTP multi-homing. "
          "If left empty, implicit bind is performed");
      declare_sctp_socket_options(e1ap_b, cu_up_cfg.e1ap_cfg.sctp);
    });
    cu_up_b.group("f1u", "F1-U parameters",
                  [&](config::config_builder& f1u_b) { declare_f1u_sockets_options(f1u_b, cu_up_cfg.f1u_cfg); });
  });
}
