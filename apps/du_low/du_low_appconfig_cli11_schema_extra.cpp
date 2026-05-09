// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "du_low_appconfig_cli11_schema_extra.h"
#include "apps/helpers/hal/hal_appconfig_extra.h"
#include "du_low_appconfig.h"

#ifdef DPDK_FOUND
static void manage_hal_optional(CLI::App& app, ocudu::du_low_appconfig& config)
{
  if (!is_hal_section_present(app)) {
    config.hal_config.reset();
  }
}
#endif

void ocudu::autoderive_du_low_parameters_after_parsing(CLI::App& app, du_low_appconfig& config)
{
#ifdef DPDK_FOUND
  manage_hal_optional(app, config);
#endif
}
