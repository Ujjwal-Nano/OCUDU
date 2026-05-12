// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "config_builder.h"
#include <limits>
#include <regex>
#include <unordered_map>

namespace ocudu {
namespace config {

// ---------------------------------------------------------------------------
// schema merge helper
// ---------------------------------------------------------------------------

void merge_into(schema_node& dst, schema_node&& src)
{
  auto* dst_group = std::get_if<group_node>(&dst.body);
  auto* src_group = std::get_if<group_node>(&src.body);
  if (dst_group == nullptr || src_group == nullptr) {
    return;
  }
  for (auto& src_child : src_group->children) {
    schema_node* existing = nullptr;
    for (auto& dst_child : dst_group->children) {
      if (dst_child.name != src_child.name) {
        continue;
      }
      if (std::holds_alternative<group_node>(dst_child.body) && std::holds_alternative<group_node>(src_child.body)) {
        existing = &dst_child;
        break;
      }
      if (std::holds_alternative<leaf_node>(dst_child.body) && std::holds_alternative<leaf_node>(src_child.body)) {
        // Same-name leaf already present — keep dst's binding (first wins).
        existing = &dst_child;
        break;
      }
    }
    if (existing != nullptr) {
      if (std::holds_alternative<group_node>(existing->body) && std::holds_alternative<group_node>(src_child.body)) {
        merge_into(*existing, std::move(src_child));
      }
      // Same-name leaves: do nothing — dst already has the option recorded.
      continue;
    }
    dst_group->children.push_back(std::move(src_child));
  }
}

// ---------------------------------------------------------------------------
// option_handle
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Fallback cascade registry. One entry per root CLI::App, holding the list
// of (source_option_name, dst_option) edges declared via
// option_handle::fallback_from(). A single final_callback per root app
// processes the entire list once parse completes.
// ---------------------------------------------------------------------------

namespace {

struct fallback_edge {
  std::string  source_name;
  CLI::Option* dst_opt;
};

struct fallback_registry {
  std::vector<fallback_edge> edges;
  bool                       callback_installed = false;
};

std::unordered_map<CLI::App*, fallback_registry>& fallback_registries()
{
  static std::unordered_map<CLI::App*, fallback_registry> map;
  return map;
}

CLI::Option* find_option_recursive(CLI::App* app, const std::string& name)
{
  if (auto* o = app->get_option_no_throw(name)) {
    return o;
  }
  for (auto* sub : app->get_subcommands({})) {
    if (auto* o = find_option_recursive(sub, name)) {
      return o;
    }
  }
  return nullptr;
}

void install_fallback_cascade(CLI::App* root_app)
{
  auto& reg = fallback_registries()[root_app];
  if (reg.callback_installed) {
    return;
  }
  reg.callback_installed = true;

  // Use final_callback rather than callback() so we don't collide with any
  // user-registered parse_complete_callback (e.g. cu_up.cpp's autoderivation).
  root_app->final_callback([root_app]() {
    auto it = fallback_registries().find(root_app);
    if (it == fallback_registries().end()) {
      return;
    }
    for (const auto& edge : it->second.edges) {
      auto* src = find_option_recursive(root_app, edge.source_name);
      if (src == nullptr || src->count() == 0) {
        continue;
      }
      if (edge.dst_opt->count() > 0) {
        continue; // user set the destination explicitly — don't override
      }
      const auto& results = src->results();
      if (results.empty()) {
        continue;
      }
      edge.dst_opt->default_val<std::string>(results.front());
    }
  });
}

} // namespace

option_handle::option_handle(CLI::Option* opt, schema_node* node, CLI::App* root_app)
  : cli11_opt_(opt), node_(node), root_app_(root_app)
{}

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

option_handle& option_handle::fallback_from(std::string source_name)
{
  leaf().fallback_source = source_name;
  if (root_app_ != nullptr) {
    fallback_registries()[root_app_].edges.push_back({std::move(source_name), cli11_opt_});
    install_fallback_cascade(root_app_);
  }
  return *this;
}

// ---------------------------------------------------------------------------
// array_handle
// ---------------------------------------------------------------------------

array_handle::array_handle(CLI::Option* opt, schema_node* node) : cli11_opt_(opt), node_(node) {}

array_node& array_handle::arr()
{
  return std::get<array_node>(node_->body);
}

array_handle& array_handle::key(std::string leaf_name)
{
  arr().key_name = std::move(leaf_name);
  return *this;
}

array_handle& array_handle::min_items(std::size_t n)
{
  if (cli11_opt_ != nullptr) {
    cli11_opt_->expected(static_cast<int>(n), -1);
  }
  arr().min_items = n;
  return *this;
}

array_handle& array_handle::max_items(std::size_t n)
{
  if (cli11_opt_ != nullptr) {
    cli11_opt_->expected(1, static_cast<int>(n));
  }
  arr().max_items = n;
  return *this;
}

// ---------------------------------------------------------------------------
// config_builder
// ---------------------------------------------------------------------------

config_builder::config_builder(CLI::App& app, schema_node& root)
  : app_(&app), root_(&root), root_app_(&app)
{}

config_builder::config_builder(CLI::App& app, schema_node& root, CLI::App* root_app)
  : app_(&app), root_(&root), root_app_(root_app)
{}

option_handle config_builder::flag(const std::string& flag_name, bool& target, const std::string& description)
{
  CLI::Option* opt = app_->add_flag(flag_name, target, description);
  opt->capture_default_str();

  schema_node leaf;
  leaf.name        = detail::canonical_name(flag_name);
  leaf.description = description;

  leaf_node payload;
  constexpr auto d       = detail::describe_scalar<bool>();
  payload.type           = d.type;
  payload.integer_bits   = d.integer_bits;
  payload.integer_signed = d.integer_signed;
  payload.default_str    = detail::format_default(target);
  payload.emit_value     = [&target](YAML::Node& node) { node = target; };
  leaf.body              = std::move(payload);

  schema_node& inserted = push_child(std::move(leaf));
  return option_handle{opt, &inserted, root_app_};
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

enum class sanity_probe_enum { a, b, c };

struct sanity_probe_struct {
  int                       int_field   = 42;
  double                    float_field = 3.14;
  std::string               str_field   = "hello";
  bool                      bool_field  = true;
  std::vector<std::string>  str_list;
  std::vector<int>          int_list;
  std::vector<sanity_probe_inner> entries;
  sanity_probe_enum         enum_field  = sanity_probe_enum::a;
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
             })
      .key("x")
      .min_items(1)
      .max_items(16);

  b.enum_option("--enum_field",
                cfg.enum_field,
                "An enum-valued option",
                {{"a", sanity_probe_enum::a}, {"b", sanity_probe_enum::b}, {"c", sanity_probe_enum::c}});

  std::optional<sanity_probe_enum> opt_enum;
  b.auto_enum_option("--auto_enum", opt_enum, "Integer or \"auto\"");
}

} // namespace

