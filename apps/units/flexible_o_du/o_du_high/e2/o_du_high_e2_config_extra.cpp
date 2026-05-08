// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "o_du_high_e2_config_extra.h"
#include "o_du_high_e2_config.h"

void ocudu::autoderive_o_du_high_e2_parameters_after_parsing(o_du_high_e2_config& unit_cfg)
{
  // If O-DU E2 agent is disabled do not enable E2AP PCAP for it.
  unit_cfg.pcaps.enabled = unit_cfg.base_cfg.enable_unit_e2 && unit_cfg.pcaps.enabled;
}
