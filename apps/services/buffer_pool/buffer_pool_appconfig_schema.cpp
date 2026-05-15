// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "apps/services/buffer_pool/buffer_pool_appconfig_schema.h"
#include "apps/helpers/config/config_builder.h"
#include "apps/helpers/metrics/metrics_config_schema.h"
#include "apps/services/buffer_pool/buffer_pool_appconfig.h"

using namespace ocudu;
using namespace app_services;

void ocudu::app_services::declare_buffer_pool_appconfig_schema(config::config_builder& b,
                                                                            buffer_pool_appconfig&  config)
{
  b.group("buffer_pool", "Buffer pool configuration", [&](config::config_builder& bp) {
    bp.option("--nof_segments", config.nof_segments, "Number of segments allocated by the buffer pool");
    bp.option("--segment_size", config.segment_size, "Size of each buffer pool segment in bytes");
  });

  b.group("metrics", "Metrics configuration", [&](config::config_builder& m) {
    m.group("layers", "Layer basis metrics configuration", [&](config::config_builder& l) {
      l.option("--enable_app_usage", config.metrics_config.enable_metrics, "Enable application usage metrics");
    });
  });
  app_helpers::declare_metrics_appconfig_schema(b, config.metrics_config.common_metrics_cfg);
}

