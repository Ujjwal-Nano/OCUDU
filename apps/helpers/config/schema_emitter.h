// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

#include "config_builder.h"
#include <string>

namespace ocudu {
namespace config {

/// JSON Schema Draft 2020-12 emission options.
struct json_schema_options {
  /// Optional schema title (top-level "title" field).
  std::string title;
  /// Optional schema "$id". Conventionally a stable URI.
  std::string id;
  /// If true, every object schema declares additionalProperties:false.
  /// Recommended — catches typos in config files at parse time.
  bool strict_additional_properties = true;
};

/// Walk the metadata tree and emit a JSON Schema 2020-12 document as a string.
/// The result is suitable for yaml-language-server or jsonschema-validator.
std::string emit_json_schema(const schema_node& root, const json_schema_options& opts = {});

} // namespace config
} // namespace ocudu
