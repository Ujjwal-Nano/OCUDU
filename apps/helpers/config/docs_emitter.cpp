// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "docs_emitter.h"
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <algorithm>
#include <map>

namespace ocudu {
namespace config {

namespace {

const char* type_label(scalar_type t)
{
  switch (t) {
    case scalar_type::integer: return "integer";
    case scalar_type::number:  return "number";
    case scalar_type::string:  return "string";
    case scalar_type::boolean: return "boolean";
  }
  return "string";
}

std::string render_type(const leaf_node& leaf)
{
  if (!leaf.type_name.empty()) {
    return fmt::format("[`{}`](#types-{})", leaf.type_name, leaf.type_name);
  }
  if (leaf.is_scalar_array) {
    return fmt::format("array of {}", type_label(leaf.type));
  }
  return type_label(leaf.type);
}

std::string escape_pipes(std::string s)
{
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    if (c == '|') {
      out += "\\|";
    } else {
      out += c;
    }
  }
  return out;
}

std::string render_constraints(const leaf_node& leaf)
{
  std::vector<std::string> parts;
  for (const auto& c : leaf.constraints) {
    std::visit(
        [&](auto&& v) {
          using T = std::decay_t<decltype(v)>;
          if constexpr (std::is_same_v<T, range_constraint>) {
            parts.push_back(fmt::format("{}..{}", v.min, v.max));
          } else if constexpr (std::is_same_v<T, min_value_constraint>) {
            parts.push_back(fmt::format("≥ {}", v.min));
          } else if constexpr (std::is_same_v<T, max_value_constraint>) {
            parts.push_back(fmt::format("≤ {}", v.max));
          } else if constexpr (std::is_same_v<T, length_constraint>) {
            parts.push_back(fmt::format("length {}..{}", v.min, v.max));
          } else if constexpr (std::is_same_v<T, min_length_constraint>) {
            parts.push_back(fmt::format("length ≥ {}", v.min));
          } else if constexpr (std::is_same_v<T, max_length_constraint>) {
            parts.push_back(fmt::format("length ≤ {}", v.max));
          } else if constexpr (std::is_same_v<T, items_constraint>) {
            parts.push_back(fmt::format("items {}..{}", v.min, v.max));
          } else if constexpr (std::is_same_v<T, min_items_constraint>) {
            parts.push_back(fmt::format("items ≥ {}", v.min));
          } else if constexpr (std::is_same_v<T, max_items_constraint>) {
            parts.push_back(fmt::format("items ≤ {}", v.max));
          } else if constexpr (std::is_same_v<T, enum_constraint>) {
            parts.push_back(fmt::format("enum: {}", fmt::join(v.values, ", ")));
          } else if constexpr (std::is_same_v<T, pattern_constraint>) {
            parts.push_back(fmt::format("pattern: `{}`", v.regex));
          }
        },
        c);
  }
  for (const auto& n : leaf.notes) {
    parts.push_back(n);
  }
  if (!leaf.fallback_source.empty()) {
    parts.push_back(fmt::format("falls back to {} if unset", leaf.fallback_source));
  }
  return fmt::format("{}", fmt::join(parts, "; "));
}

std::string render_default(const leaf_node& leaf)
{
  if (!leaf.default_str.has_value()) {
    return {};
  }
  return fmt::format("`{}`", escape_pipes(*leaf.default_str));
}

std::string heading_prefix(int level)
{
  level = std::clamp(level, 1, 6);
  return std::string(static_cast<std::size_t>(level), '#');
}

void emit_group(std::string& out, const schema_node& n, const group_node& group, int level, const markdown_options& opts);

void emit_array(std::string& out, const schema_node& n, const array_node& arr, int level, const markdown_options& opts)
{
  out += fmt::format("\n{} {}\n\n", heading_prefix(level), n.name);
  if (!n.description.empty()) {
    out += n.description;
    out += "\n\n";
  }
  out += "_List of objects with the following items:_\n\n";
  schema_node placeholder;
  placeholder.name = n.name + "[]";
  emit_group(out, placeholder, *arr.items_shape, level + 1, opts);
}

void emit_options_table(std::string& out,
                        const std::vector<const schema_node*>& leaves,
                        const markdown_options& opts)
{
  if (leaves.empty()) {
    return;
  }
  std::string header = "| Option | Type ";
  std::string sep    = "|--------|------";
  if (opts.include_defaults) {
    header += "| Default ";
    sep    += "|---------";
  }
  if (opts.include_constraints) {
    header += "| Constraints ";
    sep    += "|-------------";
  }
  header += "| Description |\n";
  sep    += "|-------------|\n";

  out += header;
  out += sep;

  for (const auto* n : leaves) {
    const auto& leaf = std::get<leaf_node>(n->body);
    out += fmt::format("| `{}` | {} ", n->name, render_type(leaf));
    if (opts.include_defaults) {
      out += fmt::format("| {} ", render_default(leaf));
    }
    if (opts.include_constraints) {
      std::string c = render_constraints(leaf);
      if (n->required) {
        if (!c.empty()) {
          c += "; ";
        }
        c += "**required**";
      }
      out += fmt::format("| {} ", escape_pipes(c));
    }
    out += fmt::format("| {} |\n", escape_pipes(n->description));
  }
  out += "\n";
}

void emit_group(std::string& out, const schema_node& n, const group_node& group, int level, const markdown_options& opts)
{
  if (!n.name.empty()) {
    out += fmt::format("\n{} {}\n\n", heading_prefix(level), n.name);
  }
  if (!n.description.empty()) {
    out += n.description;
    out += "\n\n";
  }

  std::vector<const schema_node*> leaves;
  std::vector<const schema_node*> nested;
  for (const auto& child : group.children) {
    if (std::holds_alternative<leaf_node>(child.body)) {
      leaves.push_back(&child);
    } else {
      nested.push_back(&child);
    }
  }
  emit_options_table(out, leaves, opts);

  for (const auto* child : nested) {
    std::visit(
        [&](auto&& body) {
          using B = std::decay_t<decltype(body)>;
          if constexpr (std::is_same_v<B, group_node>) {
            emit_group(out, *child, body, level + 1, opts);
          } else if constexpr (std::is_same_v<B, array_node>) {
            emit_array(out, *child, body, level + 1, opts);
          }
        },
        child->body);
  }
}

void collect_named_leaf_types(const schema_node& n, std::map<std::string, leaf_node>& sink)
{
  std::visit(
      [&](auto&& body) {
        using B = std::decay_t<decltype(body)>;
        if constexpr (std::is_same_v<B, leaf_node>) {
          if (!body.type_name.empty() && sink.find(body.type_name) == sink.end()) {
            sink.emplace(body.type_name, body);
          }
        } else if constexpr (std::is_same_v<B, group_node>) {
          for (const auto& c : body.children) {
            collect_named_leaf_types(c, sink);
          }
        } else if constexpr (std::is_same_v<B, array_node>) {
          for (const auto& c : body.items_shape->children) {
            collect_named_leaf_types(c, sink);
          }
        }
      },
      n.body);
}

} // namespace

std::string emit_markdown(const schema_node& root, const markdown_options& opts)
{
  std::string out;
  if (!opts.title.empty()) {
    out += "# ";
    out += opts.title;
    out += "\n";
  }

  // Reusable-types section comes first so the rest of the document can link
  // back into it.
  std::map<std::string, leaf_node> named_types;
  collect_named_leaf_types(root, named_types);
  if (!named_types.empty()) {
    out += "\n## Reusable types\n\n";
    for (const auto& [name, leaf] : named_types) {
      out += fmt::format("### <a id=\"types-{}\"></a>`{}`\n\n", name, name);
      out += fmt::format("- Type: {}\n", type_label(leaf.type));
      if (!leaf.constraints.empty()) {
        out += fmt::format("- Constraints: {}\n", render_constraints(leaf));
      }
      out += "\n";
    }
  }

  std::visit(
      [&](auto&& body) {
        using B = std::decay_t<decltype(body)>;
        if constexpr (std::is_same_v<B, group_node>) {
          emit_group(out, root, body, opts.heading_level_start, opts);
        } else if constexpr (std::is_same_v<B, array_node>) {
          emit_array(out, root, body, opts.heading_level_start, opts);
        } else {
          // A bare leaf root — render as a tiny table.
          std::vector<const schema_node*> single = {&root};
          emit_options_table(out, single, opts);
        }
      },
      root.body);
  return out;
}

} // namespace config
} // namespace ocudu
