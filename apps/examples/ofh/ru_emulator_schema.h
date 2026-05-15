// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

#include "apps/helpers/config/config_builder.h"

namespace ocudu {

struct ru_emulator_appconfig;

/// Declares the RU emulator application configuration schema on the given builder.
void declare_ru_emulator_appconfig_schema(config::config_builder& b, ru_emulator_appconfig& ru_emu_parsed_cfg);

} // namespace ocudu
