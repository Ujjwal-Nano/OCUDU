// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "du_low_appconfig_schema.h"
#include "apps/helpers/hal/hal_schema.h"
#include "apps/helpers/logger/logger_appconfig_schema.h"
#include "apps/helpers/tracing/tracer_appconfig_schema.h"
#include "apps/services/app_execution_metrics/executor_metrics_config_schema.h"
#include "apps/services/app_resource_usage/app_resource_usage_config_schema.h"
#include "apps/services/metrics/metrics_config_schema.h"
#include "apps/services/remote_control/remote_control_appconfig_schema.h"
#include "apps/services/worker_manager/worker_manager_schema.h"
#include "du_low_appconfig.h"

using namespace ocudu;

#ifdef DPDK_FOUND
static void manage_hal_optional(CLI::App& app, du_low_appconfig& gnb_cfg)
{
  if (!is_hal_section_present(app)) {
    gnb_cfg.hal_config.reset();
  }
}
#endif

void ocudu::declare_du_low_appconfig_schema(config::config_builder& root, du_low_appconfig& config)
{
  root.flag("--dryrun", config.enable_dryrun, "Enable application dry run mode");

  declare_logger_appconfig_schema(root, config.log_cfg);
  declare_tracer_appconfig_schema(root, config.trace_cfg);
  declare_worker_manager_appconfig_schema(root, config.expert_execution_cfg);

  app_services::declare_executor_metrics_appconfig_schema(root, config.metrics_cfg.executors_metrics_cfg);
  app_services::declare_app_resource_usage_config_schema(root, config.metrics_cfg.rusage_config);
  app_services::declare_metrics_appconfig_schema(root, config.metrics_cfg.metrics_service_cfg);

  declare_remote_control_appconfig_schema(root, config.remote_control_config);

#ifdef DPDK_FOUND
  config.hal_config.emplace();
  declare_hal_appconfig_schema(root, *config.hal_config);
#endif
}

void ocudu::autoderive_du_low_parameters_after_parsing(CLI::App& app, du_low_appconfig& config)
{
#ifdef DPDK_FOUND
  manage_hal_optional(app, config);
#endif
}
