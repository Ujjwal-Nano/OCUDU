// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

#include "CLI/CLI11.hpp"

namespace ocudu {

namespace config {
class config_builder;
}

struct sctp_appconfig;

/// Adds SCTP socket options flat (no subcommand) into the builder's current group.
void declare_sctp_socket_options(config::config_builder& b, sctp_appconfig& config);


} // namespace ocudu
