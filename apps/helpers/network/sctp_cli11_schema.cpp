// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "sctp_cli11_schema.h"
#include "apps/helpers/config/config_builder.h"
#include "sctp_appconfig.h"

using namespace ocudu;

void ocudu::configure_cli11_sctp_socket_args(config::config_builder& b, sctp_appconfig& config)
{
  // Declare SCTP socket options as a shared structure block: every caller —
  // e1ap, ngap, xnap, f1c, e2 — gets the same factored type "sctp-socket" in
  // the emitted JSON Schema $defs / YANG grouping.
  b.uses("sctp-socket", "SCTP socket parameters", [&](config::config_builder& s) {
    s.option(
        "--sctp_rto_initial", config.rto_initial_ms, "SCTP initial RTO value in milliseconds (-1 to use system default)");
    s.option("--sctp_rto_min", config.rto_min_ms, "SCTP RTO min in milliseconds (-1 to use system default)");
    s.option("--sctp_rto_max", config.rto_max_ms, "SCTP RTO max in milliseconds (-1 to use system default)");
    s.option(
        "--sctp_init_max_attempts", config.init_max_attempts, "SCTP init max attempts (-1 to use system default)");
    s.option("--sctp_max_init_timeo",
             config.max_init_timeo_ms,
             "SCTP max init timeout in milliseconds (-1 to use system default)");
    s.option(
        "--sctp_hb_interval", config.hb_interval_ms, "SCTP heartbeat interval in milliseconds (-1 to use system default)");
    s.option("--sctp_assoc_max_retx",
             config.assoc_max_retx,
             "SCTP association max retransmissions (-1 to use system default)");
    s.option("--sctp_nodelay", config.nodelay, "Send SCTP messages as soon as possible without any Nagle-like algorithm");
  });
}

void ocudu::configure_cli11_sctp_socket_args(CLI::App& app, sctp_appconfig& config)
{
  config::schema_node discard;
  discard.body = config::group_node{};
  config::config_builder b(app, discard);
  configure_cli11_sctp_socket_args(b, config);
}
