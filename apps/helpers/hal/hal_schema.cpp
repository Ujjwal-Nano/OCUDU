// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "hal_schema.h"
#include "apps/helpers/config/config_builder.h"
#include "hal_appconfig.h"

using namespace ocudu;

void ocudu::declare_hal_appconfig_schema(config::config_builder& b, hal_appconfig& config)
{
  b.group("hal", "HAL configuration", [&](config::config_builder& hal) {
    hal.option("--eal_args", config.eal_args, "EAL configuration parameters used to initialize DPDK");
  });
}

bool ocudu::is_hal_section_present(CLI::App& app)
{
  auto subcmd = app.get_subcommand("hal");
  return subcmd->count_all() != 0;
}
