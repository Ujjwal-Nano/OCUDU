// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "schema_emitter.h"
#include "external/nlohmann/json.hpp"
#include <stdexcept>

namespace ocudu {
namespace config {

namespace {

using json = nlohmann::json;

const char* type_string(scalar_type t)
{
  switch (t) {
    case scalar_type::integer: return "integer";
    case scalar_type::number:  return "number";
    case scalar_type::string:  return "string";
    case scalar_type::boolean: return "boolean";
  }
  return "string";
}

/// Parses the leaf's default_str into a JSON value of the appropriate type.
/// Falls back to nullopt on parse failure; caller omits the "default" field.
std::optional<json> parse_default(const leaf_node& leaf)
{
  if (!leaf.default_str.has_value() || leaf.is_scalar_array) {
    return std::nullopt;
  }
  const std::string& s = *leaf.default_str;
  try {
    switch (leaf.type) {
      case scalar_type::boolean:
        if (s == "true") return json(true);
        if (s == "false") return json(false);
        return std::nullopt;
      case scalar_type::integer:
        return json(std::stoll(s));
      case scalar_type::number:
        return json(std::stod(s));
      case scalar_type::string:
        return json(s);
    }
  } catch (const std::exception&) {
    return std::nullopt;
  }
  return std::nullopt;
}

void apply_constraints(json& property, const std::vector<constraint>& constraints)
{
  for (const auto& c : constraints) {
    std::visit(
        [&](auto&& v) {
          using T = std::decay_t<decltype(v)>;
          if constexpr (std::is_same_v<T, range_constraint>) {
            property["minimum"] = v.min;
            property["maximum"] = v.max;
          } else if constexpr (std::is_same_v<T, min_value_constraint>) {
            property["minimum"] = v.min;
          } else if constexpr (std::is_same_v<T, max_value_constraint>) {
            property["maximum"] = v.max;
          } else if constexpr (std::is_same_v<T, length_constraint>) {
            property["minLength"] = v.min;
            property["maxLength"] = v.max;
          } else if constexpr (std::is_same_v<T, min_length_constraint>) {
            property["minLength"] = v.min;
          } else if constexpr (std::is_same_v<T, max_length_constraint>) {
            property["maxLength"] = v.max;
          } else if constexpr (std::is_same_v<T, items_constraint>) {
            property["minItems"] = v.min;
            property["maxItems"] = v.max;
          } else if constexpr (std::is_same_v<T, min_items_constraint>) {
            property["minItems"] = v.min;
          } else if constexpr (std::is_same_v<T, max_items_constraint>) {
            property["maxItems"] = v.max;
          } else if constexpr (std::is_same_v<T, enum_constraint>) {
            property["enum"] = v.values;
          } else if constexpr (std::is_same_v<T, pattern_constraint>) {
            property["pattern"] = v.regex;
          }
        },
        c);
  }
}

std::string compose_description(const std::string& description, const std::vector<std::string>& notes)
{
  if (notes.empty()) {
    return description;
  }
  std::string out = description;
  for (const auto& n : notes) {
    if (!out.empty()) {
      out += ' ';
    }
    out += '(';
    out += n;
    out += ')';
  }
  return out;
}

json emit_node(const schema_node& n, const json_schema_options& opts);

json emit_leaf(const schema_node& n, const leaf_node& leaf)
{
  json property;
  if (!n.description.empty() || !leaf.notes.empty()) {
    property["description"] = compose_description(n.description, leaf.notes);
  }
  if (leaf.is_scalar_array) {
    property["type"]  = "array";
    json items;
    items["type"]     = type_string(leaf.type);
    property["items"] = items;
  } else {
    property["type"] = type_string(leaf.type);
  }
  apply_constraints(property, leaf.constraints);
  if (auto def = parse_default(leaf)) {
    property["default"] = *def;
  }
  return property;
}

json emit_group_body(const group_node& group, const std::string& description, const json_schema_options& opts)
{
  json property;
  if (!description.empty()) {
    property["description"] = description;
  }
  property["type"]       = "object";
  property["properties"] = json::object();

  json required = json::array();
  for (const auto& child : group.children) {
    property["properties"][child.name] = emit_node(child, opts);
    if (child.required) {
      required.push_back(child.name);
    }
  }
  if (!required.empty()) {
    property["required"] = required;
  }
  if (opts.strict_additional_properties) {
    property["additionalProperties"] = false;
  }
  return property;
}

json emit_array(const schema_node& n, const array_node& arr, const json_schema_options& opts)
{
  json property;
  if (!n.description.empty()) {
    property["description"] = n.description;
  }
  property["type"]  = "array";
  property["items"] = emit_group_body(*arr.items_shape, "", opts);
  if (arr.min_items.has_value()) {
    property["minItems"] = *arr.min_items;
  }
  if (arr.max_items.has_value()) {
    property["maxItems"] = *arr.max_items;
  }
  return property;
}

json emit_node(const schema_node& n, const json_schema_options& opts)
{
  return std::visit(
      [&](auto&& body) -> json {
        using B = std::decay_t<decltype(body)>;
        if constexpr (std::is_same_v<B, leaf_node>) {
          return emit_leaf(n, body);
        } else if constexpr (std::is_same_v<B, group_node>) {
          return emit_group_body(body, n.description, opts);
        } else {
          return emit_array(n, body, opts);
        }
      },
      n.body);
}

} // namespace

std::string emit_json_schema(const schema_node& root, const json_schema_options& opts)
{
  json schema = emit_node(root, opts);
  schema["$schema"] = "https://json-schema.org/draft/2020-12/schema";
  if (!opts.id.empty()) {
    schema["$id"] = opts.id;
  }
  if (!opts.title.empty()) {
    schema["title"] = opts.title;
  }
  return schema.dump(2);
}

} // namespace config
} // namespace ocudu
