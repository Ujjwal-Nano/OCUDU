// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

#include "CLI/CLI11.hpp"

namespace ocudu {

struct du_high_parsed_config;

/// Configures the given CLI11 application with the du high configuration schema.
void configure_cli11_with_du_high_config_schema(CLI::App& app, du_high_parsed_config& config);

} // namespace ocudu
