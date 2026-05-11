// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

#include "CLI/CLI11.hpp"
#include "apps/helpers/config/config_builder.h"

namespace ocudu {

struct cu_up_appconfig;

/// Configures the given CLI11 application with the CU-UP application
/// configuration schema. Populates \p schema_out with the metadata tree of
/// every option registered. The metadata is intended to be merged with the
/// o_cu_up unit's schema (via ocudu::config::merge_into) before driving
/// schema/docs/YANG/YAML emitters.
void configure_cli11_with_cu_appconfig_schema(CLI::App&            app,
                                              cu_up_appconfig&     cu_up_cfg,
                                              config::schema_node& schema_out);

} // namespace ocudu
