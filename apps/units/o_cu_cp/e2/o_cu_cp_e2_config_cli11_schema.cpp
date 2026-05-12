// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "o_cu_cp_e2_config_cli11_schema.h"
#include "apps/helpers/config/config_builder.h"
#include "apps/helpers/e2/e2_cli11_schema.h"
#include "o_cu_cp_e2_config.h"

using namespace ocudu;

static void declare_pcap_args(config::config_builder& b, o_cu_cp_e2_pcap_config& cfg)
{
  b.option("--e2ap_cu_cp_filename", cfg.filename, "E2AP PCAP file output path");
  b.option("--e2ap_enable", cfg.enabled, "Enable E2AP packet capture");
}

void ocudu::configure_cli11_with_o_cu_cp_e2_config_schema(config::config_builder& b, o_cu_cp_e2_config& unit_cfg)
{
  b.group("pcap", "PCAP configuration", [&](config::config_builder& pcap) { declare_pcap_args(pcap, unit_cfg.pcaps); });

  configure_cli11_with_e2_config_schema(b, unit_cfg.base_config, "--enable_cu_cp_e2", "Enable CU E2 agent");
}

void ocudu::configure_cli11_with_o_cu_cp_e2_config_schema(CLI::App& app, o_cu_cp_e2_config& unit_cfg)
{
  config::schema_node discard;
  discard.body = config::group_node{};
  config::config_builder b(app, discard);
  configure_cli11_with_o_cu_cp_e2_config_schema(b, unit_cfg);
}

void ocudu::autoderive_o_cu_cp_e2_parameters_after_parsing(o_cu_cp_e2_config& unit_cfg)
{
  // If CU CP E2 agent is disabled do not enable e2ap pcap for it.
  unit_cfg.pcaps.enabled = unit_cfg.base_config.enable_unit_e2 && unit_cfg.pcaps.enabled;
}
