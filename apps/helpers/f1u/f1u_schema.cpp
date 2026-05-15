// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "f1u_schema.h"
#include "apps/helpers/config/config_builder.h"
#include "apps/helpers/f1u/f1u_appconfig.h"
#include "apps/helpers/network/udp_schema.h"

using namespace ocudu;

static void declare_f1u_socket_args(config::config_builder& b, f1u_socket_appconfig& cfg)
{
  b.option("--bind_addr",
           cfg.bind_addr,
           "Default local IP address interfaces bind to, unless a specific bind address is specified")
      .note("must be a valid IPv4 address");
  b.option("--sst", cfg.sst, "Slice Service Type").range(0, 255);
  b.option("--sd", cfg.sd, "Service Differentiator").range(0, 0xfffffe);
  b.option("--five_qi", cfg.five_qi, "Assign this socket to a specific 5QI").range(0, 255);
  declare_udp_config_schema(b, cfg.udp_config);
}

void ocudu::declare_f1u_sockets_options(config::config_builder& b, f1u_sockets_appconfig& f1u_params)
{
  // Default port is 2152 as per TS 29.281 Sec. 4.4.2.3.
  b.option("--bind_port", f1u_params.bind_port, "F1-U bind port");
  b.option("--peer_port", f1u_params.peer_port, "F1-U peer port");

  b.array_of("--socket",
             f1u_params.f1u_socket_cfg,
             "Configures UDP/IP socket parameters of the F1-U interface",
             [](config::config_builder& el, f1u_socket_appconfig& sock) { declare_f1u_socket_args(el, sock); });
}

