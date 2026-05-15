// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

#include "CLI/CLI11.hpp"

namespace ocudu {

namespace config {
class config_builder;
}

struct hal_appconfig;

/// Builder-based primary entry point.
void configure_cli11_with_hal_appconfig_schema(config::config_builder& b, hal_appconfig& config);

/// Legacy CLI::App-based wrapper for unmigrated callers.
void configure_cli11_with_hal_appconfig_schema(CLI::App& app, hal_appconfig& config);

/// Returns true if the HAL section is present in the given CLI11 application, otherwise false.
bool is_hal_section_present(CLI::App& app);

} // namespace ocudu
