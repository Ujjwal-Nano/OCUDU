// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

#include "CLI/CLI11.hpp"

namespace ocudu {

struct sctp_appconfig;

/// Configures the given CLI11 application with the common sctp socket option parameters shared across application
/// configurations schema.
void configure_cli11_sctp_socket_args(CLI::App& app, sctp_appconfig& config);

} // namespace ocudu
