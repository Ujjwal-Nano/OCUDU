// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "schema_emitter.h"
#include "external/nlohmann/json.hpp"
#include <map>
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

std::string compose_description(const std::string&              description,
                                const std::vector<std::string>& notes,
                                const std::string&              fallback_source)
{
  if (notes.empty() && fallback_source.empty()) {
    return description;
  }
  std::string out = description;
  auto        append = [&](const std::string& s) {
    if (!out.empty()) {
      out += ' ';
    }
    out += '(';
    out += s;
    out += ')';
  };
  for (const auto& n : notes) {
    append(n);
  }
  if (!fallback_source.empty()) {
    append("falls back to " + fallback_source + " if unset");
  }
  return out;
}

/// Builds the structural-only (type + constraints) JSON for a leaf, without
/// description / default / notes / fallback. This is what lives in $defs.
json emit_leaf_structure(const leaf_node& leaf)
{
  json structure;
  if (leaf.is_scalar_array) {
    structure["type"] = "array";
    json items;
    items["type"]     = type_string(leaf.type);
    structure["items"] = items;
  } else {
    structure["type"] = type_string(leaf.type);
  }
  apply_constraints(structure, leaf.constraints);
  return structure;
}

struct emission_state {
  /// Maps type name -> already-emitted structure (so subsequent occurrences
  /// just reference it). Used for both leaf type_name hoisting and
  /// shared-block (b.uses) hoisting. std::map (not unordered_map) so the
  /// $defs section emits in deterministic alphabetical order — keeps diffs
  /// reviewable when example artifacts are regenerated.
  std::map<std::string, json> defs;
};

json emit_group_body(const group_node&          group,
                     const std::string&         description,
                     const json_schema_options& opts,
                     emission_state&            st);

json emit_node(const schema_node& n, const json_schema_options& opts, emission_state& st);

json emit_leaf(const schema_node& n, const leaf_node& leaf, emission_state& st)
{
  json property;
  if (!leaf.type_name.empty()) {
    // Hoist the structural shape into $defs (first occurrence wins; later
    // occurrences must match or the developer has a bug).
    auto it = st.defs.find(leaf.type_name);
    if (it == st.defs.end()) {
      st.defs.emplace(leaf.type_name, emit_leaf_structure(leaf));
    }
    property["$ref"] = "#/$defs/" + leaf.type_name;
    // JSON Schema 2020-12 allows sibling keywords alongside $ref — per-site
    // description and default stay local.
    if (!n.description.empty() || !leaf.notes.empty() || !leaf.fallback_source.empty()) {
      property["description"] = compose_description(n.description, leaf.notes, leaf.fallback_source);
    }
    if (auto def = parse_default(leaf)) {
      property["default"] = *def;
    }
    return property;
  }

  // Non-hoisted leaf: emit inline.
  if (!n.description.empty() || !leaf.notes.empty() || !leaf.fallback_source.empty()) {
    property["description"] = compose_description(n.description, leaf.notes, leaf.fallback_source);
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

json emit_group_body(const group_node&          group,
                     const std::string&         description,
                     const json_schema_options& opts,
                     emission_state&            st)
{
  json property;
  if (!description.empty()) {
    property["description"] = description;
  }
  property["type"]       = "object";
  property["properties"] = json::object();

  json required = json::array();
  for (const auto& child : group.children) {
    property["properties"][child.name] = emit_node(child, opts, st);
    if (child.required) {
      required.push_back(child.name);
    }
  }

  // Shared blocks: register each as a $defs entry (first occurrence wins) and
  // add an allOf {$ref} clause to require the properties of the named type.
  // The properties themselves are NOT inlined here — they live in the $def.
  json all_of = json::array();
  for (const auto& sb : group.shared_blocks) {
    if (st.defs.find(sb.type_name) == st.defs.end()) {
      // Synthesize a group_node containing only sb.children and emit as a
      // standalone object schema.
      group_node tmp;
      tmp.children = sb.children;
      json shared  = emit_group_body(tmp, "", opts, st);
      st.defs.emplace(sb.type_name, shared);
    }
    json ref;
    ref["$ref"] = "#/$defs/" + sb.type_name;
    if (!sb.site_description.empty()) {
      ref["description"] = sb.site_description;
    }
    all_of.push_back(ref);
  }
  if (!all_of.empty()) {
    property["allOf"] = all_of;
    // additionalProperties:false + allOf would reject the shared properties.
    // The cleanest fix is to relax it on groups that use shared blocks.
    property["additionalProperties"] = true;
  } else if (opts.strict_additional_properties) {
    property["additionalProperties"] = false;
  }

  if (!required.empty()) {
    property["required"] = required;
  }
  return property;
}

json emit_array(const schema_node& n, const array_node& arr, const json_schema_options& opts, emission_state& st)
{
  json property;
  if (!n.description.empty()) {
    property["description"] = n.description;
  }
  property["type"]  = "array";
  property["items"] = emit_group_body(*arr.items_shape, "", opts, st);
  if (arr.min_items.has_value()) {
    property["minItems"] = *arr.min_items;
  }
  if (arr.max_items.has_value()) {
    property["maxItems"] = *arr.max_items;
  }
  return property;
}

json emit_node(const schema_node& n, const json_schema_options& opts, emission_state& st)
{
  return std::visit(
      [&](auto&& body) -> json {
        using B = std::decay_t<decltype(body)>;
        if constexpr (std::is_same_v<B, leaf_node>) {
          return emit_leaf(n, body, st);
        } else if constexpr (std::is_same_v<B, group_node>) {
          return emit_group_body(body, n.description, opts, st);
        } else {
          return emit_array(n, body, opts, st);
        }
      },
      n.body);
}

} // namespace

std::string emit_json_schema(const schema_node& root, const json_schema_options& opts)
{
  emission_state st;
  json           schema = emit_node(root, opts, st);
  schema["$schema"]     = "https://json-schema.org/draft/2020-12/schema";
  if (!opts.id.empty()) {
    schema["$id"] = opts.id;
  }
  if (!opts.title.empty()) {
    schema["title"] = opts.title;
  }
  if (!st.defs.empty()) {
    json defs = json::object();
    for (auto& [name, body] : st.defs) {
      defs[name] = body;
    }
    schema["$defs"] = defs;
  }
  return schema.dump(2);
}

} // namespace config
} // namespace ocudu
