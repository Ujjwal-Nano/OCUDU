// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

#include "CLI/CLI11.hpp"

namespace ocudu {

namespace config {
class config_builder;
}

namespace app_helpers {

struct metrics_config;

/// Builder-based primary entry point. Records the metrics subcommand options
/// in the metadata tree so the schema/docs/YANG emitters can describe them.
void declare_metrics_appconfig_schema(config::config_builder& b, metrics_config& config);


} // namespace app_helpers
} // namespace ocudu
