// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "udp_cli11_schema.h"
#include "udp_appconfig.h"
#include "ocudu/support/cli11_utils.h"

using namespace ocudu;

static void configure_cli11_udp_args(CLI::App& app, udp_appconfig& config)
{
  add_option(app, "--max_rx_msgs", config.rx_max_msgs, "Maximum amount of messages RX in a single syscall")
      ->capture_default_str();
  add_option(app, "--tx_qsize", config.tx_qsize, "Batched queue size")->capture_default_str();
  add_option(app, "--max_tx_msgs", config.tx_max_msgs, "Maximum amount of messages TX in a single syscall")
      ->capture_default_str();
  add_option(app, "--max_tx_segments", config.tx_max_segments, "Maximum amount of segments in a single TX SDU")
      ->capture_default_str();
  add_option(app, "--pool_threshold", config.pool_threshold, "Pool accupancy threshold after which packets are dropped")
      ->capture_default_str();
  add_option(app, "--reuse_addr", config.reuse_addr, "Allow multiple sockets to re-use the bind port")
      ->capture_default_str();
  add_option(app, "--dscp", config.dscp, "Differentiated Services Code Point value")
      ->capture_default_str()
      ->check(CLI::Range(0, 63));
}

void ocudu::configure_cli11_with_udp_config_schema(CLI::App& app, udp_appconfig& config)
{
  add_option(app, "--ext_addr", config.ext_addr, "External IP address that is advertised for receiving UDP packets.")
      ->check(CLI::ValidIPV4 | CLI::IsMember({"auto"}));
  CLI::App* udp_subcmd = add_subcommand(app, "udp", "UDP parameters")->configurable();
  configure_cli11_udp_args(*udp_subcmd, config);
}
