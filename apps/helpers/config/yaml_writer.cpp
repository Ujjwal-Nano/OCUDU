// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "yaml_writer.h"

namespace ocudu {
namespace config {

namespace {

void emit_group(YAML::Node& node, const group_node& g);

void emit_node(YAML::Node& node, const schema_node& n)
{
  YAML::Node child = node[n.name];
  std::visit(
      [&](auto&& body) {
        using B = std::decay_t<decltype(body)>;
        if constexpr (std::is_same_v<B, leaf_node>) {
          body.emit_value(child);
        } else if constexpr (std::is_same_v<B, group_node>) {
          emit_group(child, body);
        } else if constexpr (std::is_same_v<B, array_node>) {
          YAML::Node seq(YAML::NodeType::Sequence);
          body.for_each_element([&](group_node& elem_meta) {
            YAML::Node item(YAML::NodeType::Map);
            emit_group(item, elem_meta);
            seq.push_back(item);
          });
          child = seq;
        }
      },
      n.body);
}

void emit_group(YAML::Node& node, const group_node& g)
{
  for (const auto& child : g.children) {
    emit_node(node, child);
  }
}

} // namespace

void emit_yaml(YAML::Node& node, const schema_node& root)
{
  std::visit(
      [&](auto&& body) {
        using B = std::decay_t<decltype(body)>;
        if constexpr (std::is_same_v<B, group_node>) {
          emit_group(node, body);
        } else {
          // For a leaf or array root, write it directly under \p node.
          emit_node(node, root);
        }
      },
      root.body);
}

} // namespace config
} // namespace ocudu
