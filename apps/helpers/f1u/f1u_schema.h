// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

#include "CLI/CLI11.hpp"

namespace ocudu {

namespace config {
class config_builder;
}

struct f1u_sockets_appconfig;

/// Builder-based primary entry point.
void declare_f1u_sockets_options(config::config_builder& b, f1u_sockets_appconfig& f1u_params);


} // namespace ocudu
