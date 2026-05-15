// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI

#pragma once

#include "apps/helpers/config/config_builder.h"
#include "ocudu/ocudulog/logger.h"

namespace ocudu {
namespace app_helpers {

/// Canonical name<->value mapping for ocudulog::basic_levels. Drives every
/// log-level option declared through add_log_option().
inline std::vector<std::pair<std::string, ocudulog::basic_levels>> basic_levels_mapping()
{
  return {
      {"none", ocudulog::basic_levels::none},
      {"error", ocudulog::basic_levels::error},
      {"warning", ocudulog::basic_levels::warning},
      {"info", ocudulog::basic_levels::info},
      {"debug", ocudulog::basic_levels::debug},
  };
}

/// Declares a single log-level option on the builder. The schema layer sees
/// it as a string-typed enum (none|error|warning|info|debug). When the option
/// has a fallback source (typically "--all_level") the post-parse cascade
/// copies the source's value to the dest at parse time if the user didn't
/// set the dest explicitly.
inline config::option_handle add_log_option(config::config_builder& b,
                                            ocudulog::basic_levels& target,
                                            const std::string&      name,
                                            const std::string&      description)
{
  return b.enum_option(name, target, description, basic_levels_mapping());
}

} // namespace app_helpers
} // namespace ocudu
