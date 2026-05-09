// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "split_8_o_du_unit_cli11_schema.h"
#include "apps/units/flexible_o_du/o_du_high/o_du_high_unit_config_cli11_schema.h"
#include "apps/units/flexible_o_du/o_du_low/du_low_config_cli11_schema.h"
#include "apps/units/flexible_o_du/split_8/helpers/ru_sdr_config_cli11_schema.h"
#include "split_8_o_du_unit_config.h"
#include "ocudu/support/cli11_utils.h"

using namespace ocudu;

void ocudu::configure_cli11_with_split_8_o_du_unit_config_schema(CLI::App& app, split_8_o_du_unit_config& config)
{
  configure_cli11_with_o_du_high_config_schema(app, config.odu_high_cfg);
  configure_cli11_with_du_low_config_schema(app, config.du_low_cfg);
  configure_cli11_with_ru_sdr_config_schema(app, config.ru_cfg);
}
