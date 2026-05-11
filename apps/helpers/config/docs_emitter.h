// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

#include "config_builder.h"
#include <string>

namespace ocudu {
namespace config {

/// Markdown reference-emission options.
struct markdown_options {
  /// Document title written as a single H1 at the top. Optional.
  std::string title;
  /// Heading depth at which the root group's children start (root itself is
  /// emitted at heading_level_start, its children at +1, etc.).
  int  heading_level_start = 1;
  /// Include "Default" column in option tables.
  bool include_defaults = true;
  /// Include "Constraints" column in option tables.
  bool include_constraints = true;
};

/// Walk the metadata tree and emit a Markdown configuration reference suitable
/// for inclusion in user-facing docs.
std::string emit_markdown(const schema_node& root, const markdown_options& opts = {});

} // namespace config
} // namespace ocudu
