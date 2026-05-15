// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

#include "CLI/CLI11.hpp"

namespace ocudu {

namespace config {
class config_builder;
}

struct dynamic_o_du_unit_config;

/// Builder-based primary entry point.
void configure_cli11_with_dynamic_o_du_unit_config_schema(config::config_builder& b, dynamic_o_du_unit_config& parsed_cfg);

/// Legacy CLI::App-based wrapper for unmigrated callers.
void configure_cli11_with_dynamic_o_du_unit_config_schema(CLI::App& app, dynamic_o_du_unit_config& parsed_cfg);

/// Auto derive dynamic O-RAN DU parameters after the parsing.
void autoderive_dynamic_o_du_parameters_after_parsing(CLI::App& app, dynamic_o_du_unit_config& parsed_cfg);

} // namespace ocudu
