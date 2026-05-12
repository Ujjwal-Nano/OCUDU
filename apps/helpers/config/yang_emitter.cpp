// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "yang_emitter.h"
#include <fmt/format.h>
#include <algorithm>
#include <sstream>

namespace ocudu {
namespace config {

namespace {

std::string to_kebab(const std::string& s)
{
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    out += (c == '_') ? '-' : c;
  }
  return out;
}

std::string yang_name(const std::string& s, const yang_options& opts)
{
  return opts.kebab_case_names ? to_kebab(s) : s;
}

std::string indent(int level)
{
  return std::string(static_cast<std::size_t>(level * 2), ' ');
}

std::string quote(const std::string& s)
{
  std::string out;
  out.reserve(s.size() + 2);
  out += '"';
  for (char c : s) {
    if (c == '"' || c == '\\') {
      out += '\\';
    }
    out += c;
  }
  out += '"';
  return out;
}

/// Pick a YANG integer type from C++ bit-width and signedness. Defaults to
/// int32/uint32 when bit-width is unknown.
std::string yang_integer_type(int bits, bool is_signed)
{
  int b = bits > 0 ? bits : 32;
  if (b <= 8)  return is_signed ? "int8"  : "uint8";
  if (b <= 16) return is_signed ? "int16" : "uint16";
  if (b <= 32) return is_signed ? "int32" : "uint32";
  return is_signed ? "int64" : "uint64";
}

/// Emit the `type` statement (including any inline restrictions) for a leaf.
/// Returns the full multi-line block, indented from \p level.
std::string emit_type_block(const leaf_node& leaf, int level, const yang_options& opts)
{
  // enum_constraint overrides the base type with `type enumeration { ... }`.
  for (const auto& c : leaf.constraints) {
    if (auto* e = std::get_if<enum_constraint>(&c)) {
      std::string out = fmt::format("{}type enumeration {{\n", indent(level));
      for (const auto& v : e->values) {
        out += fmt::format("{}enum {};\n", indent(level + 1), quote(v));
      }
      out += fmt::format("{}}}\n", indent(level));
      return out;
    }
  }

  std::string base;
  switch (leaf.type) {
    case scalar_type::boolean: base = "boolean"; break;
    case scalar_type::integer: base = yang_integer_type(leaf.integer_bits, leaf.integer_signed); break;
    case scalar_type::number:  base = "decimal64"; break;
    case scalar_type::string:  base = "string"; break;
  }

  // Collect restrictions that go inside type { ... }.
  std::vector<std::string> restrictions;
  if (leaf.type == scalar_type::number) {
    restrictions.push_back(fmt::format("fraction-digits {};", opts.decimal_fraction_digits));
  }
  for (const auto& c : leaf.constraints) {
    std::visit(
        [&](auto&& v) {
          using T = std::decay_t<decltype(v)>;
          if constexpr (std::is_same_v<T, range_constraint>) {
            restrictions.push_back(fmt::format("range \"{}..{}\";", v.min, v.max));
          } else if constexpr (std::is_same_v<T, min_value_constraint>) {
            restrictions.push_back(fmt::format("range \"{}..max\";", v.min));
          } else if constexpr (std::is_same_v<T, max_value_constraint>) {
            restrictions.push_back(fmt::format("range \"min..{}\";", v.max));
          } else if constexpr (std::is_same_v<T, length_constraint>) {
            restrictions.push_back(fmt::format("length \"{}..{}\";", v.min, v.max));
          } else if constexpr (std::is_same_v<T, min_length_constraint>) {
            restrictions.push_back(fmt::format("length \"{}..max\";", v.min));
          } else if constexpr (std::is_same_v<T, max_length_constraint>) {
            restrictions.push_back(fmt::format("length \"min..{}\";", v.max));
          } else if constexpr (std::is_same_v<T, pattern_constraint>) {
            restrictions.push_back(fmt::format("pattern {};", quote(v.regex)));
          }
          // items/min/max-items belong on the parent leaf-list/list, not here.
          // enum handled above.
        },
        c);
  }

  if (restrictions.empty()) {
    return fmt::format("{}type {};\n", indent(level), base);
  }
  std::string out = fmt::format("{}type {} {{\n", indent(level), base);
  for (const auto& r : restrictions) {
    out += fmt::format("{}{}\n", indent(level + 1), r);
  }
  out += fmt::format("{}}}\n", indent(level));
  return out;
}

/// YANG-format default value literal.
std::string yang_default_literal(const leaf_node& leaf)
{
  if (!leaf.default_str.has_value()) {
    return {};
  }
  switch (leaf.type) {
    case scalar_type::boolean:
    case scalar_type::integer:
    case scalar_type::number:
      return *leaf.default_str;
    case scalar_type::string:
      return quote(*leaf.default_str);
  }
  return quote(*leaf.default_str);
}

std::string emit_description_stmt(const std::string&              desc,
                                  const std::vector<std::string>& notes,
                                  const std::string&              fallback_source,
                                  int                             level)
{
  std::string composed = desc;
  auto        append   = [&](const std::string& s) {
    if (!composed.empty()) {
      composed += ' ';
    }
    composed += '(';
    composed += s;
    composed += ')';
  };
  for (const auto& n : notes) {
    append(n);
  }
  if (!fallback_source.empty()) {
    append("falls back to " + fallback_source + " if unset");
  }
  if (composed.empty()) {
    return {};
  }
  return fmt::format("{}description {};\n", indent(level), quote(composed));
}

void emit_group_children(std::string& out, const group_node& group, int level, const yang_options& opts);

void emit_leaf(std::string& out, const schema_node& n, const leaf_node& leaf, int level, const yang_options& opts)
{
  const std::string name = yang_name(n.name, opts);
  if (leaf.is_scalar_array) {
    out += fmt::format("{}leaf-list {} {{\n", indent(level), name);
  } else {
    out += fmt::format("{}leaf {} {{\n", indent(level), name);
  }
  out += emit_type_block(leaf, level + 1, opts);
  if (n.required) {
    out += fmt::format("{}mandatory true;\n", indent(level + 1));
  }
  if (auto def = yang_default_literal(leaf); !def.empty() && !leaf.is_scalar_array) {
    out += fmt::format("{}default {};\n", indent(level + 1), def);
  }
  out += emit_description_stmt(n.description, leaf.notes, leaf.fallback_source, level + 1);
  out += fmt::format("{}}}\n", indent(level));
}

void emit_container(std::string& out, const schema_node& n, const group_node& group, int level, const yang_options& opts)
{
  out += fmt::format("{}container {} {{\n", indent(level), yang_name(n.name, opts));
  out += emit_description_stmt(n.description, {}, std::string{}, level + 1);
  emit_group_children(out, group, level + 1, opts);
  out += fmt::format("{}}}\n", indent(level));
}

void emit_list(std::string& out, const schema_node& n, const array_node& arr, int level, const yang_options& opts)
{
  out += fmt::format("{}list {} {{\n", indent(level), yang_name(n.name, opts));
  if (arr.key_name.empty()) {
    out += fmt::format("{}// WARNING: no key designated; YANG list requires `key`. Use .key(\"...\") at declaration.\n",
                       indent(level + 1));
  } else {
    out += fmt::format("{}key {};\n", indent(level + 1), quote(yang_name(arr.key_name, opts)));
  }
  if (arr.min_items.has_value()) {
    out += fmt::format("{}min-elements {};\n", indent(level + 1), *arr.min_items);
  }
  if (arr.max_items.has_value()) {
    out += fmt::format("{}max-elements {};\n", indent(level + 1), *arr.max_items);
  }
  out += emit_description_stmt(n.description, {}, std::string{}, level + 1);
  emit_group_children(out, *arr.items_shape, level + 1, opts);
  out += fmt::format("{}}}\n", indent(level));
}

void emit_group_children(std::string& out, const group_node& group, int level, const yang_options& opts)
{
  for (const auto& child : group.children) {
    std::visit(
        [&](auto&& body) {
          using B = std::decay_t<decltype(body)>;
          if constexpr (std::is_same_v<B, leaf_node>) {
            emit_leaf(out, child, body, level, opts);
          } else if constexpr (std::is_same_v<B, group_node>) {
            emit_container(out, child, body, level, opts);
          } else if constexpr (std::is_same_v<B, array_node>) {
            emit_list(out, child, body, level, opts);
          }
        },
        child.body);
  }
}

std::string sanitize_prefix(const std::string& module_name)
{
  std::string out;
  out.reserve(module_name.size());
  for (char c : module_name) {
    if (std::isalnum(static_cast<unsigned char>(c)) != 0) {
      out += c;
    } else if (out.empty() || out.back() != '-') {
      out += '-';
    }
  }
  while (!out.empty() && out.back() == '-') {
    out.pop_back();
  }
  return out.empty() ? std::string("mod") : out;
}

} // namespace

std::string emit_yang(const schema_node& root, const yang_options& opts)
{
  std::string out;
  const std::string prefix = opts.prefix.empty() ? sanitize_prefix(opts.module_name) : opts.prefix;

  out += fmt::format("module {} {{\n", opts.module_name);
  out += "  yang-version 1.1;\n";
  out += fmt::format("  namespace {};\n", quote(opts.namespace_uri));
  out += fmt::format("  prefix {};\n\n", prefix);

  if (!opts.organization.empty()) {
    out += fmt::format("  organization {};\n", quote(opts.organization));
  }
  if (!opts.contact.empty()) {
    out += fmt::format("  contact {};\n", quote(opts.contact));
  }
  if (!opts.description.empty()) {
    out += fmt::format("  description {};\n", quote(opts.description));
  }
  if (!opts.revision.empty()) {
    out += fmt::format("\n  revision {} {{\n    description \"Generated.\";\n  }}\n", opts.revision);
  }
  out += "\n";

  std::visit(
      [&](auto&& body) {
        using B = std::decay_t<decltype(body)>;
        if constexpr (std::is_same_v<B, group_node>) {
          emit_group_children(out, body, 1, opts);
        } else if constexpr (std::is_same_v<B, leaf_node>) {
          emit_leaf(out, root, body, 1, opts);
        } else if constexpr (std::is_same_v<B, array_node>) {
          emit_list(out, root, body, 1, opts);
        }
      },
      root.body);

  out += "}\n";
  return out;
}

} // namespace config
} // namespace ocudu
