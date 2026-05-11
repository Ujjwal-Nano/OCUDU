// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

#include "config_builder.h"
#include <yaml-cpp/yaml.h>

namespace ocudu {
namespace config {

/// Walk the metadata tree and populate \p node with the live values of every
/// registered option. Replaces the hand-written fill_*_appconfig_in_yaml_schema
/// pattern: leaves fire emit_value, groups recurse, arrays iterate per element.
void emit_yaml(YAML::Node& node, const schema_node& root);

} // namespace config
} // namespace ocudu
