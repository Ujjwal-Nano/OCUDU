// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

#include "config_builder.h"
#include <string>

namespace ocudu {
namespace config {

/// YANG 1.1 module emission options. The emitter produces a single module
/// suitable for NETCONF / RESTCONF use (RFC 7950).
struct yang_options {
  /// Module name. Becomes `module <module_name>`. Required.
  std::string module_name;
  /// XML namespace URI. Required.
  std::string namespace_uri;
  /// Short module prefix used for referencing this module from others.
  /// Defaults to a sanitized form of module_name when empty.
  std::string prefix;
  /// Organization line (free text).
  std::string organization;
  /// Contact line (free text).
  std::string contact;
  /// Module description.
  std::string description;
  /// Revision date in YYYY-MM-DD form.
  std::string revision;
  /// Convert snake_case names to kebab-case (YANG idiom). On by default.
  bool        kebab_case_names = true;
  /// fraction-digits used for `decimal64` types (floats/doubles). YANG
  /// requires an explicit choice; 6 is a sensible default for config values.
  int         decimal_fraction_digits = 6;
};

/// Walk the metadata tree and emit a YANG 1.1 module document. The root group
/// becomes the module body (its leaves and sub-containers are top-level
/// children of the module). If the root is a leaf or array, it is emitted as
/// a single top-level node.
std::string emit_yang(const schema_node& root, const yang_options& opts);

} // namespace config
} // namespace ocudu
