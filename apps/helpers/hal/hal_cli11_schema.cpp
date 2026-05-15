// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "hal_cli11_schema.h"
#include "apps/helpers/config/config_builder.h"
#include "hal_appconfig.h"

using namespace ocudu;

void ocudu::configure_cli11_with_hal_appconfig_schema(config::config_builder& b, hal_appconfig& config)
{
  b.group("hal", "HAL configuration", [&](config::config_builder& hal) {
    hal.option("--eal_args", config.eal_args, "EAL configuration parameters used to initialize DPDK");
  });
}

void ocudu::configure_cli11_with_hal_appconfig_schema(CLI::App& app, hal_appconfig& config)
{
  config::schema_node discard;
  discard.body = config::group_node{};
  config::config_builder b(app, discard);
  configure_cli11_with_hal_appconfig_schema(b, config);
}

bool ocudu::is_hal_section_present(CLI::App& app)
{
  auto subcmd = app.get_subcommand("hal");
  return subcmd->count_all() != 0;
}
