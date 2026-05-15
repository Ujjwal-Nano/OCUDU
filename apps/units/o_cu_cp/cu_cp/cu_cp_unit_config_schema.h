// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

#include "ocudu/adt/span.h"
#include "CLI/CLI11.hpp"

namespace ocudu {

namespace config {
class config_builder;
}

struct cu_cp_unit_config;
struct cu_cp_unit_supported_ta_item;

/// Builder-based primary entry point.
void declare_cu_cp_unit_config_schema(config::config_builder& b, cu_cp_unit_config& unit_cfg);


/// Auto derive CU-CP parameters after the parsing.
void autoderive_cu_cp_parameters_after_parsing(CLI::App& app, cu_cp_unit_config& unit_cfg);

} // namespace ocudu
