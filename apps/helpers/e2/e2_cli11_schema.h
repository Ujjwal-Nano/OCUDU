// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

#include "CLI/CLI11.hpp"
#include <string>

namespace ocudu {

namespace config {
class config_builder;
}

struct e2_config;

/// Builder-based primary entry point.
void configure_cli11_with_e2_config_schema(config::config_builder& b,
                                           e2_config&              config,
                                           const std::string&      option_name,
                                           const std::string&      option_description);

/// Legacy CLI::App-based wrapper for unmigrated callers.
void configure_cli11_with_e2_config_schema(CLI::App&          app,
                                           e2_config&         config,
                                           const std::string& option_name,
                                           const std::string& option_description);

} // namespace ocudu
