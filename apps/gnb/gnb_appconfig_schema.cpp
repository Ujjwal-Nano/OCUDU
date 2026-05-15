// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "gnb_appconfig_schema.h"
#include "apps/helpers/config/config_builder.h"
#include "apps/helpers/hal/hal_schema.h"
#include "apps/helpers/logger/logger_appconfig_schema.h"
#include "apps/helpers/tracing/tracer_appconfig_schema.h"
#include "apps/services/app_execution_metrics/executor_metrics_config_schema.h"
#include "apps/services/app_resource_usage/app_resource_usage_config_schema.h"
#include "apps/services/buffer_pool/buffer_pool_appconfig_schema.h"
#include "apps/services/metrics/metrics_config_schema.h"
#include "apps/services/remote_control/remote_control_appconfig_schema.h"
#include "apps/services/worker_manager/worker_manager_schema.h"
#include "apps/units/flexible_o_du/o_du_high/du_high/du_high_config.h"
#include "apps/units/o_cu_cp/cu_cp/cu_cp_unit_config.h"
#include "gnb_appconfig.h"
#include "ocudu/ocudulog/ocudulog.h"

using namespace ocudu;

#ifdef DPDK_FOUND
static void manage_hal_optional(CLI::App& app, gnb_appconfig& gnb_cfg)
{
  if (!is_hal_section_present(app)) {
    gnb_cfg.hal_config.reset();
  }
}
#endif

void ocudu::declare_gnb_appconfig_schema(config::config_builder& root, gnb_appconfig& gnb_parsed_cfg)
{
  gnb_appconfig& gnb_cfg = gnb_parsed_cfg;

  root.flag("--dryrun", gnb_cfg.enable_dryrun, "Enable application dry run mode");

  root.option("--gnb_id", gnb_cfg.gnb_id.id, "gNodeB identifier");
  root.option("--gnb_id_bit_length", gnb_cfg.gnb_id.bit_length, "gNodeB identifier length in bits").range(22, 32);
  root.option("--ran_node_name", gnb_cfg.ran_node_name, "RAN node name");

  declare_logger_appconfig_schema(root, gnb_cfg.log_cfg);
  declare_tracer_appconfig_schema(root, gnb_cfg.trace_cfg);
  app_services::declare_buffer_pool_appconfig_schema(root, gnb_cfg.buffer_pool_config);
  declare_worker_manager_appconfig_schema(root, gnb_cfg.expert_execution_cfg);

  // Metrics section.
  root.group("metrics", "Metrics configuration", [&](config::config_builder& m) {
    m.option("--autostart_stdout_metrics",
             gnb_cfg.metrics_cfg.autostart_stdout_metrics,
             "Autostart stdout metrics reporting");
  });
  app_services::declare_executor_metrics_appconfig_schema(root, gnb_cfg.metrics_cfg.executors_metrics_cfg);
  app_services::declare_app_resource_usage_config_schema(root, gnb_cfg.metrics_cfg.rusage_config);
  app_services::declare_metrics_appconfig_schema(root, gnb_cfg.metrics_cfg.metrics_service_cfg);

#ifdef DPDK_FOUND
  gnb_cfg.hal_config.emplace();
  declare_hal_appconfig_schema(root, *gnb_cfg.hal_config);
#else
  // When built without DPDK, the hal section is not declared at all — the
  // CLI11 parse-error message used to be customized in the legacy path; with
  // the builder we drop the custom failure_message and rely on CLI11's
  // default behaviour. Re-introducing it would need a builder-level hook.
#endif

  declare_remote_control_appconfig_schema(root, gnb_cfg.remote_control_config);
}

void ocudu::autoderive_gnb_parameters_after_parsing(CLI::App& app, gnb_appconfig& config)
{
#ifdef DPDK_FOUND
  manage_hal_optional(app, config);
#endif
}

void ocudu::autoderive_supported_tas_for_amf_from_du_cells(const du_high_unit_config& du_hi_cfg,
                                                           cu_cp_unit_config&         cu_cp_cfg)
{
  // If no cells are found in DU configuration.
  if (du_hi_cfg.cells_cfg.empty()) {
    return;
  }

  // Clear supported TAs.
  cu_cp_cfg.amf_config.amf.supported_tas.clear();
  cu_cp_cfg.amf_config.amf.is_default_supported_tas = false;

  // Derive supported TAs from DU cell configuration.
  for (const auto& cell : du_hi_cfg.cells_cfg) {
    cu_cp_unit_supported_ta_item supported_ta;
    supported_ta.tac = cell.cell.tac;
    supported_ta.plmn_list.push_back({cell.cell.plmn, {cu_cp_unit_plmn_item::tai_slice_t{1}}});
    cu_cp_cfg.amf_config.amf.supported_tas.push_back(supported_ta);
  }
}
