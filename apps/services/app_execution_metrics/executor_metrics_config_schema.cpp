// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "executor_metrics_config_schema.h"
#include "apps/helpers/config/config_builder.h"
#include "apps/helpers/metrics/metrics_config_schema.h"
#include "executor_metrics_config.h"
#include "ocudu/ran/slot_point.h"
#include "ocudu/ran/slot_point_extended.h"

using namespace ocudu;
using namespace app_services;

void ocudu::app_services::declare_executor_metrics_appconfig_schema(config::config_builder&  b,
                                                                                 executor_metrics_config& config)
{
  b.group("metrics", "Metrics configuration", [&](config::config_builder& m) {
    m.group("layers", "Layer basis metrics configuration", [&](config::config_builder& l) {
      l.option("--enable_executor", config.enable_executor_metrics, "Whether to log application executors metrics");
    });
    m.group("periodicity", "Metrics periodicity configuration", [&](config::config_builder& p) {
      p.option("--executors_report_period", config.report_period_ms, "Executors metrics report period in milliseconds")
          .range(0, static_cast<int>(NOF_SUBFRAMES_PER_FRAME * NOF_SFNS * NOF_HYPER_SFNS));
    });
  });
  app_helpers::declare_metrics_appconfig_schema(b, config.common_metrics_cfg);
}

