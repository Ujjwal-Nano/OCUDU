// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

#include "CLI/CLI11.hpp"

namespace ocudu {

namespace config {
class config_builder;
}

struct du_high_parsed_config;
struct du_high_unit_config;

/// Builder-based primary entry point.
void configure_cli11_with_du_high_config_schema(config::config_builder& b, du_high_parsed_config& parsed_cfg);

/// Legacy CLI::App-based wrapper for unmigrated callers.
void configure_cli11_with_du_high_config_schema(CLI::App& app, du_high_parsed_config& parsed_cfg);

/// Auto derive DU high parameters after the parsing.
void autoderive_du_high_parameters_after_parsing(CLI::App& app, du_high_unit_config& unit_cfg);

} // namespace ocudu
