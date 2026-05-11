// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

#include "apps/helpers/config/config_builder.h"

namespace ocudu {

struct cu_up_appconfig;

/// Declares the CU-UP application configuration schema on the given builder.
/// CLI11 wiring and metadata-tree population happen as a side effect of the
/// builder's internals. The resulting tree is intended to be merged with the
/// o_cu_up unit's schema (via ocudu::config::merge_into) before driving the
/// schema/docs/YANG/YAML emitters.
void declare_cu_up_appconfig_schema(config::config_builder& b, cu_up_appconfig& cu_up_cfg);

} // namespace ocudu
