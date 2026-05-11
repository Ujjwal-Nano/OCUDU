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
void configure_cli11_with_metrics_appconfig_schema(config::config_builder& b, metrics_config& config);

/// Legacy CLI::App-based wrapper for callers that have not yet been migrated
/// to config_builder. Options registered through this path are wired into
/// CLI11 normally but stay invisible to schema/docs/YANG emitters.
void configure_cli11_with_metrics_appconfig_schema(CLI::App& app, metrics_config& config);

} // namespace app_helpers
} // namespace ocudu
