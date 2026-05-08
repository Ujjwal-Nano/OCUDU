// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

#include "apps/helpers/metrics/metrics_config.h"
#include <chrono>

namespace ocudu {
namespace app_services {

/// Executor metrics configuration.
struct executor_metrics_config {
  /// Common metrics config.
  app_helpers::metrics_config common_metrics_cfg;
  /// Whether to log application executors metrics.
  bool enable_executor_metrics = false;
  /// Executors metrics report period in milliseconds.
  std::chrono::milliseconds report_period_ms = std::chrono::milliseconds{1000};
};

} // namespace app_services
} // namespace ocudu
