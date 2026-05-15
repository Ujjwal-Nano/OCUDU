// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

#include "CLI/CLI11.hpp"

namespace ocudu {

namespace config {
class config_builder;
}

struct ru_ofh_unit_config;
struct ru_ofh_unit_parsed_config;

/// Configures the given builder with the Open Fronthaul Radio Unit configuration schema.
void declare_ru_ofh_config_schema(config::config_builder& b, ru_ofh_unit_parsed_config& parsed_cfg);


/// Auto derive OFH Radio Unit parameters after the parsing.
void autoderive_ru_ofh_parameters_after_parsing(CLI::App& app, ru_ofh_unit_parsed_config& parsed_cfg);

} // namespace ocudu
