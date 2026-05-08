// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

#include "CLI/CLI11.hpp"

namespace ocudu {

struct du_high_unit_config;

/// Auto derive DU high parameters after the parsing.
void autoderive_du_high_parameters_after_parsing(CLI::App& app, du_high_unit_config& unit_cfg);

} // namespace ocudu
