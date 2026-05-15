// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

#include "ocudu/ran/duplex_mode.h"
#include "CLI/CLI11.hpp"

namespace ocudu {

namespace config {
class config_builder;
}

struct du_low_unit_config;

/// Configures the given builder with the DU low configuration schema.
void declare_du_low_config_schema(config::config_builder& b, du_low_unit_config& parsed_cfg);


/// Auto derive DU low parameters after the parsing.
void autoderive_du_low_parameters_after_parsing(CLI::App& app, du_low_unit_config& parsed_cfg, duplex_mode mode);

} // namespace ocudu
