// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "udp_schema.h"
#include "apps/helpers/config/config_builder.h"
#include "udp_appconfig.h"

using namespace ocudu;

void ocudu::declare_udp_config_schema(config::config_builder& b, udp_appconfig& config)
{
  // The original CLI11 path enforced ValidIPV4 | IsMember({"auto"}) on this
  // option — see the legacy wrapper below. Until the constraint taxonomy
  // gains an IP-address kind / anyOf, the builder-path version captures the
  // expectation as a .note() and accepts any string.
  b.option("--ext_addr", config.ext_addr, "External IP address that is advertised for receiving UDP packets.")
      .note("must be a valid IPv4 address or \"auto\"");

  b.group("udp", "UDP parameters", [&](config::config_builder& udp) {
    udp.option("--max_rx_msgs", config.rx_max_msgs, "Maximum amount of messages RX in a single syscall");
    udp.option("--tx_qsize", config.tx_qsize, "Size of TX queue used for batching SDUs.");
    udp.option("--max_tx_msgs", config.tx_max_msgs, "Maximum amount of messages TX in a single syscall");
    udp.option("--max_tx_segments", config.tx_max_segments, "Maximum amount of segments TX in a single SDU");
    udp.option("--pool_threshold", config.pool_threshold, "Pool accupancy threshold after which packets are dropped");
    udp.option("--reuse_addr", config.reuse_addr, "Allow multiple sockets to bind to the same port.");
    udp.option("--dscp", config.dscp, "Differentiated Services Code Point value.").range(0, 63);
  });
}

