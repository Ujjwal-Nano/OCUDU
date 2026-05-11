// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

#include "CLI/CLI11.hpp"

namespace ocudu {

namespace config {
class config_builder;
}

struct o_cu_up_e2_config;

/// Builder-based primary entry point.
void configure_cli11_with_o_cu_up_e2_config_schema(config::config_builder& b, o_cu_up_e2_config& unit_cfg);

/// Legacy CLI::App-based wrapper for unmigrated callers.
void configure_cli11_with_o_cu_up_e2_config_schema(CLI::App& app, o_cu_up_e2_config& unit_cfg);

/// Auto derive O-RAN CU-UP E2 parameters after the parsing.
void autoderive_o_cu_up_e2_parameters_after_parsing(o_cu_up_e2_config& unit_cfg);

} // namespace ocudu
