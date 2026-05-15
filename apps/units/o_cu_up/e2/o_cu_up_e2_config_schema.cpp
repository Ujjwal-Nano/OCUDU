// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "o_cu_up_e2_config_schema.h"
#include "apps/helpers/config/config_builder.h"
#include "apps/helpers/e2/e2_schema.h"
#include "o_cu_up_e2_config.h"

using namespace ocudu;

void ocudu::declare_o_cu_up_e2_config_schema(config::config_builder& b, o_cu_up_e2_config& unit_cfg)
{
  declare_e2_config_schema(b, unit_cfg.base_config, "--enable_cu_up_e2", "Enable CU-UP E2 agent");

  b.group("pcap", "Logging configuration", [&](config::config_builder& pcap) {
    pcap.option("--e2ap_cu_up_filename", unit_cfg.pcaps.filename, "E2AP PCAP file output path");
    pcap.option("--e2ap_enable", unit_cfg.pcaps.enabled, "Enable E2AP packet capture");
  });
}

void ocudu::autoderive_o_cu_up_e2_parameters_after_parsing(o_cu_up_e2_config& unit_cfg)
{
  unit_cfg.pcaps.enabled = unit_cfg.base_config.enable_unit_e2 && unit_cfg.pcaps.enabled;
}
