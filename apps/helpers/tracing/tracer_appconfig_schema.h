// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

#include "CLI/CLI11.hpp"

namespace ocudu {

namespace config {
class config_builder;
}

struct tracer_appconfig;

/// Builder-based primary entry point.
void declare_tracer_appconfig_schema(config::config_builder& b, tracer_appconfig& config);


} // namespace ocudu
