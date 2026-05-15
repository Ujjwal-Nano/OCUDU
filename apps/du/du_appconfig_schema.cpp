// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "du_appconfig_schema.h"
#include "apps/helpers/f1u/f1u_cli11_schema.h"
#include "apps/helpers/hal/hal_cli11_schema.h"
#include "apps/helpers/logger/logger_appconfig_cli11_schema.h"
#include "apps/helpers/network/sctp_cli11_schema.h"
#include "apps/helpers/tracing/tracer_appconfig_cli11_schema.h"
#include "apps/services/app_resource_usage/app_resource_usage_config_cli11_schema.h"
#include "apps/services/buffer_pool/buffer_pool_appconfig_cli11_schema.h"
#include "apps/services/metrics/metrics_config_cli11_schema.h"
#include "apps/services/remote_control/remote_control_appconfig_cli11_schema.h"
#include "apps/services/worker_manager/worker_manager_cli11_schema.h"
#include "du_appconfig.h"

using namespace ocudu;

#ifdef DPDK_FOUND
static void manage_hal_optional(CLI::App& app, du_appconfig& du_cfg)
{
  if (!is_hal_section_present(app)) {
    du_cfg.hal_config.reset();
  }
}
#endif

static void configure_default_f1u(du_appconfig& du_cfg)
{
  if (du_cfg.f1u_cfg.f1u_sockets.f1u_socket_cfg.empty()) {
    f1u_socket_appconfig default_f1u_cfg;
    default_f1u_cfg.bind_addr = "127.0.10.2";
    du_cfg.f1u_cfg.f1u_sockets.f1u_socket_cfg.push_back(default_f1u_cfg);
  }
}

void ocudu::declare_du_appconfig_schema(config::config_builder& root, du_appconfig& du_cfg)
{
  root.flag("--dryrun", du_cfg.enable_dryrun, "Enable application dry run mode");

  configure_cli11_with_logger_appconfig_schema(root, du_cfg.log_cfg);
  configure_cli11_with_tracer_appconfig_schema(root, du_cfg.trace_cfg);
  app_services::configure_cli11_with_buffer_pool_appconfig_schema(root, du_cfg.buffer_pool_config);
  configure_cli11_with_worker_manager_appconfig_schema(root, du_cfg.expert_execution_cfg);

  root.group("f1ap", "F1AP interface configuration", [&](config::config_builder& f1ap_b) {
    f1ap_b.option(
        "--addrs,--cu_cp_addr",
        du_cfg.f1ap_cfg.cu_cp_addresses,
        "CU-CP F1-C addresses to connect to. Multiple addresses can be specified for SCTP multi-homing");
    f1ap_b.option(
        "--bind_addrs,--bind_addr",
        du_cfg.f1ap_cfg.bind_addresses,
        "DU F1-C bind addresses. Multiple addresses can be specified for SCTP multi-homing. "
        "If left empty, implicit bind is performed");
    configure_cli11_sctp_socket_args(f1ap_b, du_cfg.f1ap_cfg.sctp);
  });

  root.group("f1u", "F1-U interface configuration", [&](config::config_builder& f1u_b) {
    f1u_b.option("--queue_size", du_cfg.f1u_cfg.pdu_queue_size, "F1-U PDU queue size");
    configure_cli11_f1u_sockets_args(f1u_b, du_cfg.f1u_cfg.f1u_sockets);
  });

  root.group("metrics", "Metrics configuration", [&](config::config_builder& m) {
    m.option("--autostart_stdout_metrics",
             du_cfg.metrics_cfg.autostart_stdout_metrics,
             "Autostart stdout metrics reporting");
  });
  app_services::configure_cli11_with_app_resource_usage_config_schema(root, du_cfg.metrics_cfg.rusage_config);
  app_services::configure_cli11_with_metrics_appconfig_schema(root, du_cfg.metrics_cfg.metrics_service_cfg);

#ifdef DPDK_FOUND
  du_cfg.hal_config.emplace();
  configure_cli11_with_hal_appconfig_schema(root, *du_cfg.hal_config);
#endif

  configure_cli11_with_remote_control_appconfig_schema(root, du_cfg.remote_control_config);
}

void ocudu::autoderive_du_parameters_after_parsing(CLI::App& app, du_appconfig& du_cfg)
{
#ifdef DPDK_FOUND
  manage_hal_optional(app, du_cfg);
#endif

  configure_default_f1u(du_cfg);
}
