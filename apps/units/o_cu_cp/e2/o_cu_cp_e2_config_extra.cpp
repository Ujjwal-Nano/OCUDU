// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "o_cu_cp_e2_config_extra.h"
#include "o_cu_cp_e2_config.h"

void ocudu::autoderive_o_cu_cp_e2_parameters_after_parsing(o_cu_cp_e2_config& unit_cfg)
{
  // If CU CP E2 agent is disabled do not enable e2ap pcap for it.
  unit_cfg.pcaps.enabled = unit_cfg.base_config.enable_unit_e2 && unit_cfg.pcaps.enabled;
}
