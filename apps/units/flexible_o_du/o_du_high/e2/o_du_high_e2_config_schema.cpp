// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "o_du_high_e2_config_schema.h"
#include "apps/helpers/config/config_builder.h"
#include "apps/helpers/e2/e2_schema.h"
#include "o_du_high_e2_config.h"

using namespace ocudu;

static void declare_pcap_args(config::config_builder& b, o_du_high_e2_pcap_config& pcap_params)
{
  b.option("--e2ap_du_filename", pcap_params.filename, "E2AP PCAP file output path");
  b.option("--e2ap_enable", pcap_params.enabled, "Enable E2AP packet capture");
}

void ocudu::declare_o_du_high_e2_config_schema(config::config_builder& b, o_du_high_e2_config& config)
{
  // PCAP section.
  b.group("pcap", "PCAP configuration",
          [&](config::config_builder& p) { declare_pcap_args(p, config.pcaps); });

  // E2 section.
  declare_e2_config_schema(b, config.base_cfg, "--enable_du_e2", "Enable DU E2 agent");
}

void ocudu::autoderive_o_du_high_e2_parameters_after_parsing(o_du_high_e2_config& unit_cfg)
{
  // If O-DU E2 agent is disabled do not enable E2AP PCAP for it.
  unit_cfg.pcaps.enabled = unit_cfg.base_cfg.enable_unit_e2 && unit_cfg.pcaps.enabled;
}
