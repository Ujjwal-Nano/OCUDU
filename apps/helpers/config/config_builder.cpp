// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "config_builder.h"
#include <limits>
#include <regex>

namespace ocudu {
namespace config {

// ---------------------------------------------------------------------------
// option_handle
// ---------------------------------------------------------------------------

option_handle::option_handle(CLI::Option* opt, schema_node* node) : cli11_opt_(opt), node_(node) {}

leaf_node& option_handle::leaf()
{
  return std::get<leaf_node>(node_->body);
}

option_handle& option_handle::required()
{
  cli11_opt_->required();
  node_->required = true;
  return *this;
}

option_handle& option_handle::range(double min, double max)
{
  cli11_opt_->check(CLI::Range(min, max));
  leaf().constraints.emplace_back(range_constraint{min, max});
  return *this;
}

option_handle& option_handle::min_value(double v)
{
  cli11_opt_->check(CLI::Range(v, std::numeric_limits<double>::infinity()));
  leaf().constraints.emplace_back(min_value_constraint{v});
  return *this;
}

option_handle& option_handle::max_value(double v)
{
  cli11_opt_->check(CLI::Range(-std::numeric_limits<double>::infinity(), v));
  leaf().constraints.emplace_back(max_value_constraint{v});
  return *this;
}

option_handle& option_handle::enum_values(std::vector<std::string> values)
{
  cli11_opt_->check(CLI::IsMember(values));
  leaf().constraints.emplace_back(enum_constraint{std::move(values)});
  return *this;
}

option_handle& option_handle::pattern(std::string regex)
{
  std::string copy = regex;
  cli11_opt_->check(CLI::Validator(
      [copy](const std::string& input) -> std::string {
        if (std::regex_match(input, std::regex(copy))) {
          return "";
        }
        return "Value does not match pattern " + copy;
      },
      "pattern(" + regex + ")"));
  leaf().constraints.emplace_back(pattern_constraint{std::move(regex)});
  return *this;
}

option_handle& option_handle::min_length(std::size_t n)
{
  // CLI11 has no built-in length check on strings; enforce via a custom validator.
  cli11_opt_->check(CLI::Validator(
      [n](const std::string& input) -> std::string {
        if (input.size() < n) {
          return "String must be at least " + std::to_string(n) + " characters";
        }
        return "";
      },
      "min_length(" + std::to_string(n) + ")"));
  leaf().constraints.emplace_back(min_length_constraint{n});
  return *this;
}

option_handle& option_handle::max_length(std::size_t n)
{
  cli11_opt_->check(CLI::Validator(
      [n](const std::string& input) -> std::string {
        if (input.size() > n) {
          return "String must be at most " + std::to_string(n) + " characters";
        }
        return "";
      },
      "max_length(" + std::to_string(n) + ")"));
  leaf().constraints.emplace_back(max_length_constraint{n});
  return *this;
}

option_handle& option_handle::min_items(std::size_t n)
{
  // CLI11 enforces minimum element count via expected_min on multi-value options.
  cli11_opt_->expected(static_cast<int>(n), -1);
  leaf().constraints.emplace_back(min_items_constraint{n});
  return *this;
}

option_handle& option_handle::max_items(std::size_t n)
{
  // CLI11 enforces an upper bound via expected_max on multi-value options.
  cli11_opt_->expected(1, static_cast<int>(n));
  leaf().constraints.emplace_back(max_items_constraint{n});
  return *this;
}

option_handle& option_handle::note(std::string extra)
{
  leaf().notes.push_back(std::move(extra));
  return *this;
}

// ---------------------------------------------------------------------------
// config_builder
// ---------------------------------------------------------------------------

config_builder::config_builder(CLI::App& app, schema_node& root) : app_(&app), root_(&root) {}

option_handle config_builder::flag(const std::string& flag_name, bool& target, const std::string& description)
{
  CLI::Option* opt = app_->add_flag(flag_name, target, description);
  opt->capture_default_str();

  schema_node leaf;
  leaf.name        = detail::canonical_name(flag_name);
  leaf.description = description;

  leaf_node payload;
  payload.type        = scalar_type::boolean;
  payload.default_str = detail::format_default(target);
  payload.emit_value  = [&target](YAML::Node& node) { node = target; };
  leaf.body           = std::move(payload);

  schema_node& inserted = push_child(std::move(leaf));
  return option_handle{opt, &inserted};
}

schema_node& config_builder::push_child(schema_node child)
{
  auto& children = std::get<group_node>(root_->body).children;
  children.push_back(std::move(child));
  return children.back();
}

} // namespace config
} // namespace ocudu

// ===========================================================================
// Compile-time instantiation check. The header is template-heavy and has no
// consumers in this commit; this anonymous-namespace probe forces the compiler
// to type-check every overload end-to-end. It is never called.
// ===========================================================================

namespace {

struct sanity_probe_inner {
  int         x = 0;
  std::string s;
};

struct sanity_probe_struct {
  int                       int_field   = 42;
  double                    float_field = 3.14;
  std::string               str_field   = "hello";
  bool                      bool_field  = true;
  std::vector<std::string>  str_list;
  std::vector<int>          int_list;
  std::vector<sanity_probe_inner> entries;
};

[[maybe_unused]] void config_builder_instantiation_probe()
{
  using namespace ocudu::config;

  CLI::App     app("probe");
  schema_node  root;
  root.body = group_node{};

  config_builder b(app, root);
  sanity_probe_struct cfg;

  b.option("--int", cfg.int_field, "int").range(0, 100).required();
  b.option("--float", cfg.float_field, "float").min_value(0.0).max_value(10.0);
  b.option("--str", cfg.str_field, "string").pattern("^[a-z]+$").min_length(1).max_length(64);
  b.flag("--bool", cfg.bool_field, "bool");
  b.option("--strs", cfg.str_list, "strings").min_items(1).max_items(8);
  b.option("--ints", cfg.int_list, "ints");
  b.enumeration("--mode", cfg.str_field, "mode", {"a", "b", "c"}).note("only meaningful with --bool=true");

  b.group("nested", "nested params", [&](config_builder& nb) {
    nb.option("--inner", cfg.int_field, "inner int");
  });

  b.array_of("--entries", cfg.entries, "list of entries",
             [](config_builder& el, sanity_probe_inner& entry) {
               el.option("--x", entry.x, "x value").range(0, 10);
               el.option("--s", entry.s, "s value");
             });
}

} // namespace

