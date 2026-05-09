// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "o_cu_up_e2_config_cli11_schema.h"
#include "apps/helpers/e2/e2_cli11_schema.h"
#include "o_cu_up_e2_config.h"
#include "ocudu/support/cli11_utils.h"

using namespace ocudu;

static void configure_cli11_pcap_args(CLI::App& app, o_cu_up_e2_config& config)
{
  add_option(app, "--e2ap_cu_up_filename", config.pcaps.filename, "E2AP PCAP file output path")->capture_default_str();
  add_option(app, "--e2ap_enable", config.pcaps.enabled, "Enable E2AP packet capture")->always_capture_default();
}

void ocudu::configure_cli11_with_o_cu_up_e2_config_schema(CLI::App& app, o_cu_up_e2_config& config)
{
  CLI::App* pcap_subcmd = add_subcommand(app, "pcap", "Logging configuration")->configurable();
  configure_cli11_pcap_args(*pcap_subcmd, config);
  configure_cli11_with_e2_config_schema(app, config.base_config, "--enable_cu_up_e2", "Enable CU-UP E2 agent");
}
