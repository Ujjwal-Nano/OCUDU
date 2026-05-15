// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

#include "apps/helpers/config/config_builder.h"
#include "du_high/du_high_config_schema.h"
#include "e2/o_du_high_e2_config_schema.h"
#include "fapi/fapi_config_schema.h"
#include "o_du_high_unit_config.h"
#include "CLI/CLI11.hpp"

namespace ocudu {

inline void declare_o_du_high_config_schema(config::config_builder& b, o_du_high_unit_config& unit_cfg)
{
  declare_du_high_config_schema(b, unit_cfg.du_high_cfg);
  declare_fapi_config_schema(b, unit_cfg.fapi_cfg);
  declare_o_du_high_e2_config_schema(b, unit_cfg.e2_cfg);
}

/// Auto derive O-DU high parameters after the parsing.
inline void autoderive_o_du_high_parameters_after_parsing(CLI::App& app, o_du_high_unit_config& unit_cfg)
{
  autoderive_du_high_parameters_after_parsing(app, unit_cfg.du_high_cfg.config);
  autoderive_o_du_high_e2_parameters_after_parsing(unit_cfg.e2_cfg);
}

} // namespace ocudu
