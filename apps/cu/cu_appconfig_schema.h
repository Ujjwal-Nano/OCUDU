// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

#include "apps/helpers/config/config_builder.h"

namespace ocudu {

struct cu_appconfig;

/// Declares the CU application configuration schema on the given builder.
void declare_cu_appconfig_schema(config::config_builder& b, cu_appconfig& cu_cfg);

} // namespace ocudu
