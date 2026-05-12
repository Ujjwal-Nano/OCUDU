// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

#include "CLI/CLI11.hpp"
#include "ocudu/support/cli11_utils.h"
#include "ocudu/support/config_parsers.h"
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <yaml-cpp/yaml.h>
#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace ocudu {
namespace config {

// ===========================================================================
// Constraint taxonomy. Closed set; each constraint maps 1:1 to a JSON Schema
// 2020-12 keyword. Constraints that don't fit must be expressed in CLI11 via
// option_handle::note() and accepted as free-text in the description.
// ===========================================================================

struct range_constraint {
  double min;
  double max;
};

struct min_value_constraint {
  double min;
};

struct max_value_constraint {
  double max;
};

struct length_constraint {
  std::size_t min;
  std::size_t max;
};

struct min_length_constraint {
  std::size_t min;
};

struct max_length_constraint {
  std::size_t max;
};

struct items_constraint {
  std::size_t min;
  std::size_t max;
};

struct min_items_constraint {
  std::size_t min;
};

struct max_items_constraint {
  std::size_t max;
};

struct enum_constraint {
  std::vector<std::string> values;
};

struct pattern_constraint {
  std::string regex;
};

using constraint = std::variant<range_constraint,
                                min_value_constraint,
                                max_value_constraint,
                                length_constraint,
                                min_length_constraint,
                                max_length_constraint,
                                items_constraint,
                                min_items_constraint,
                                max_items_constraint,
                                enum_constraint,
                                pattern_constraint>;

// ===========================================================================
// Scalar type tags. JSON Schema "type" field.
// ===========================================================================

enum class scalar_type { integer, number, string, boolean };

// ===========================================================================
// Metadata tree. Populated as a side effect of declaration on config_builder.
// ===========================================================================

struct schema_node;

struct leaf_node {
  scalar_type                       type;
  /// True if the target is std::vector<scalar>. JSON Schema renders as
  /// {"type":"array","items":{"type":<type>}}.
  bool                              is_scalar_array = false;
  /// C++-level integer width hint (8/16/32/64). Used by emitters that need
  /// bit-precise types (e.g. YANG uint16 vs uint32). 0 if not applicable.
  int                               integer_bits = 0;
  /// C++-level integer signedness. Only meaningful when integer_bits > 0.
  bool                              integer_signed = true;
  std::optional<std::string>        default_str;
  std::vector<constraint>           constraints;
  /// Free-text descriptive notes appended to the description.
  std::vector<std::string>          notes;
  /// Type-erased value renderer. Assigns the leaf's target to the yaml node.
  std::function<void(YAML::Node&)>  emit_value;
};

struct group_node {
  std::vector<schema_node> children;
};

struct array_node {
  /// Shape of one element, extracted once from an exemplar. The leaf renderers
  /// in this tree bind the discarded exemplar — do NOT call them. Used only
  /// for schema and docs emission. shared_ptr (rather than unique_ptr) so the
  /// containing schema_node remains copyable, which lets host applications
  /// compose schemas from independent units via config::merge_into.
  std::shared_ptr<group_node>                            items_shape;
  /// Iterator that invokes its argument once per actual element with a freshly
  /// built group_node bound to that element's struct. Used by yaml_writer.
  std::function<void(std::function<void(group_node&)>)>  for_each_element;
  /// Name of the leaf within items_shape that uniquely identifies an element.
  /// Required by YANG (`list ... { key "..."; }`); empty for emitters that
  /// don't care.
  std::string                                            key_name;
  /// Optional cardinality bounds. JSON Schema renders as minItems/maxItems;
  /// YANG as min-elements/max-elements.
  std::optional<std::size_t>                             min_items;
  std::optional<std::size_t>                             max_items;
};

struct schema_node {
  std::string                                       name;        ///< Canonical name, no leading "--".
  std::string                                       description;
  bool                                              required = false;
  std::variant<leaf_node, group_node, array_node>   body;
};

// ===========================================================================
// Internal: name + type helpers.
// ===========================================================================

namespace detail {

inline std::string canonical_name(const std::string& cli_flag)
{
  std::string first = cli_flag.substr(0, cli_flag.find(','));
  if (first.size() >= 2 && first[0] == '-' && first[1] == '-') {
    first = first.substr(2);
  } else if (!first.empty() && first[0] == '-') {
    first = first.substr(1);
  }
  return first;
}

template <typename T>
struct is_vector : std::false_type {};

template <typename T, typename A>
struct is_vector<std::vector<T, A>> : std::true_type {};

template <typename T>
struct vector_value {
  using type = T;
};

template <typename T, typename A>
struct vector_value<std::vector<T, A>> {
  using type = T;
};

template <typename T>
struct is_chrono_duration : std::false_type {};

template <typename Rep, typename Period>
struct is_chrono_duration<std::chrono::duration<Rep, Period>> : std::true_type {};

template <typename T>
struct chrono_rep {
  using type = T;
};

template <typename Rep, typename Period>
struct chrono_rep<std::chrono::duration<Rep, Period>> {
  using type = Rep;
};

template <typename T>
struct is_optional : std::false_type {};

template <typename T>
struct is_optional<std::optional<T>> : std::true_type {};

template <typename T>
struct optional_value {
  using type = T;
};

template <typename T>
struct optional_value<std::optional<T>> {
  using type = T;
};

struct scalar_descriptor {
  scalar_type type;
  int         integer_bits   = 0;
  bool        integer_signed = true;
};

template <typename T>
constexpr scalar_descriptor describe_scalar()
{
  if constexpr (is_optional<T>::value) {
    return describe_scalar<typename optional_value<T>::type>();
  } else if constexpr (is_chrono_duration<T>::value) {
    return describe_scalar<typename chrono_rep<T>::type>();
  } else if constexpr (std::is_enum_v<T>) {
    return describe_scalar<std::underlying_type_t<T>>();
  } else if constexpr (std::is_same_v<T, bool>) {
    return {scalar_type::boolean, 0, false};
  } else if constexpr (std::is_integral_v<T>) {
    return {scalar_type::integer, static_cast<int>(sizeof(T) * 8), std::is_signed_v<T>};
  } else if constexpr (std::is_floating_point_v<T>) {
    return {scalar_type::number, 0, false};
  } else if constexpr (std::is_same_v<T, std::string>) {
    return {scalar_type::string, 0, false};
  } else {
    static_assert(sizeof(T) == 0, "Unsupported scalar type for config_builder option");
    return {scalar_type::string, 0, false};
  }
}

template <typename T>
std::optional<std::string> format_default(const T& v)
{
  if constexpr (is_optional<T>::value) {
    if (!v.has_value()) {
      return std::nullopt;
    }
    return format_default(*v);
  } else if constexpr (is_chrono_duration<T>::value) {
    return fmt::format("{}", v.count());
  } else if constexpr (std::is_enum_v<T>) {
    return fmt::format("{}", static_cast<std::underlying_type_t<T>>(v));
  } else if constexpr (std::is_same_v<T, bool>) {
    return v ? std::string("true") : std::string("false");
  } else if constexpr (std::is_same_v<T, std::string>) {
    return v;
  } else if constexpr (is_vector<T>::value) {
    return fmt::format("[{}]", fmt::join(v, ", "));
  } else {
    return fmt::format("{}", v);
  }
}

} // namespace detail

// ===========================================================================
// option_handle. Returned by leaf-declaration methods. Mutates BOTH the
// underlying CLI11 option AND the corresponding schema_node leaf.
// NOTE: deliberately exposes no raw_cli11() escape hatch — anything not
// expressible through this interface should be added to the taxonomy.
// ===========================================================================

class config_builder;

/// Chainable handle for an array_of declaration. Adds array-level metadata
/// that doesn't apply to scalar leaves (notably the YANG `key` requirement).
class array_handle
{
public:
  array_handle(CLI::Option* opt, schema_node* node);

  /// Designates which leaf inside the items shape acts as the unique key for
  /// each element. Required for YANG `list` emission; ignored by JSON Schema
  /// / yaml-writer / docs emitters.
  array_handle& key(std::string leaf_name);

  array_handle& min_items(std::size_t n);
  array_handle& max_items(std::size_t n);

private:
  array_node& arr();

  CLI::Option* cli11_opt_;
  schema_node* node_;
};

class option_handle
{
public:
  option_handle(CLI::Option* opt, schema_node* node);

  option_handle& required();
  option_handle& range(double min, double max);
  option_handle& min_value(double v);
  option_handle& max_value(double v);
  option_handle& enum_values(std::vector<std::string> values);
  option_handle& pattern(std::string regex);
  option_handle& min_length(std::size_t n);
  option_handle& max_length(std::size_t n);
  option_handle& min_items(std::size_t n);
  option_handle& max_items(std::size_t n);

  /// Free-text constraint note appended to the description. Use for things
  /// outside the JSON Schema-aligned taxonomy (e.g. cross-field rules).
  option_handle& note(std::string extra);

private:
  leaf_node& leaf();

  CLI::Option* cli11_opt_;
  schema_node* node_;
};

// ===========================================================================
// config_builder. The single entry point for declaring options.
// ===========================================================================

/// Merge \p src into \p dst at the group level. Both must hold group_nodes.
/// Children with the same name are merged recursively when both sides are
/// groups; otherwise src's children are appended. Used when an application
/// composes its schema from multiple independently-built trees (e.g. main
/// appconfig + an application_unit's contribution).
void merge_into(schema_node& dst, schema_node&& src);

class config_builder
{
public:
  /// Construct against an existing CLI11 app + a schema_node holding a
  /// group_node (the root or a group child).
  config_builder(CLI::App& app, schema_node& root);

  /// Transitional accessor for the underlying CLI11 subcommand. Lets a partly
  /// migrated app delegate sub-sections to legacy `configure_cli11_*` helpers
  /// while the surrounding code is already builder-driven. Options registered
  /// through this handle are NOT visible to the schema/docs/YANG emitters —
  /// callers should migrate them to the builder API before deleting it.
  CLI::App& cli11_app() { return *app_; }

  // -- Leaves --

  template <typename T>
  option_handle option(const std::string& flag, T& target, const std::string& description);

  option_handle flag(const std::string& flag_name, bool& target, const std::string& description);

  template <typename T>
  option_handle enumeration(const std::string&        flag,
                            T&                        target,
                            const std::string&        description,
                            std::vector<std::string>  values);

  /// String-backed enum option. The mapping defines the legal names ↔ enum
  /// values; CLI11 accepts the names, the typed enum target is updated through
  /// a capture lambda, and the schema layer sees the option as a string leaf
  /// with an enum_constraint populated from the mapping. emit_value renders
  /// the current enum value through the inverse mapping. Replaces every site
  /// that used add_option_function<string> + a manual string→enum converter.
  template <typename E>
  option_handle enum_option(const std::string&                     flag,
                            E&                                     target,
                            const std::string&                     description,
                            std::vector<std::pair<std::string, E>> mapping);

  /// Integer-or-"auto" option backed by std::optional<E>, where E is an enum
  /// class whose underlying type is integral. Parses "auto" (or empty) as
  /// std::nullopt and any other input as an integer cast to E. Mirrors the
  /// existing add_auto_enum_option helper. The schema describes the leaf as
  /// type=integer with a free-text note about the "auto" sentinel — JSON
  /// Schema's anyOf would express this more precisely but is not yet in the
  /// taxonomy.
  template <typename E>
  option_handle auto_enum_option(const std::string& flag, std::optional<E>& target, const std::string& description);

  // -- Groups (CLI11 subcommands) --

  template <typename Configurator>
  void group(const std::string& name, const std::string& description, Configurator&& configurator);

  // -- Arrays of structures --

  /// Internally re-uses the existing vector<string>+nested-CLI11 pattern
  /// (see add_option_cell in ocudu/support/cli11_utils.h). The configurator
  /// must be a pure binding function and is invoked:
  ///   * once on an exemplar element to extract the items shape;
  ///   * once per parsed YAML element at CLI11 parse time, on target[i];
  ///   * once per element at value-emission time, on target[i].
  /// ElementType must be default-constructible.
  template <typename Container, typename ElementConfigurator>
  array_handle array_of(const std::string&        flag,
                        Container&                target,
                        const std::string&        description,
                        ElementConfigurator&&     element_configurator);

private:
  CLI::App*    app_;
  schema_node* root_;

  schema_node& push_child(schema_node child);
};

// ===========================================================================
// Inline template implementations.
// ===========================================================================

template <typename T>
option_handle config_builder::option(const std::string& flag, T& target, const std::string& description)
{
  CLI::Option* opt = add_option(*app_, flag, target, description);
  opt->capture_default_str();

  // Deduplicate against existing leaves with the same canonical name. CLI11's
  // add_option (via ocudu/support/cli11_utils.h) chains callbacks so multiple
  // registrations of the same name update both targets — for the schema we
  // only want one entry. Keep the first registration's emit_value as
  // authoritative.
  const std::string canonical = detail::canonical_name(flag);
  for (auto& existing : std::get<group_node>(root_->body).children) {
    if (existing.name == canonical && std::holds_alternative<leaf_node>(existing.body)) {
      return option_handle{opt, &existing};
    }
  }

  schema_node leaf;
  leaf.name        = canonical;
  leaf.description = description;

  leaf_node payload;
  if constexpr (detail::is_vector<T>::value) {
    using element_t                  = typename detail::vector_value<T>::type;
    constexpr auto d                 = detail::describe_scalar<element_t>();
    payload.type                     = d.type;
    payload.integer_bits             = d.integer_bits;
    payload.integer_signed           = d.integer_signed;
    payload.is_scalar_array          = true;
  } else {
    constexpr auto d                 = detail::describe_scalar<T>();
    payload.type                     = d.type;
    payload.integer_bits             = d.integer_bits;
    payload.integer_signed           = d.integer_signed;
    payload.is_scalar_array          = false;
  }
  payload.default_str = detail::format_default(target);
  if constexpr (detail::is_optional<T>::value) {
    using inner = typename detail::optional_value<T>::type;
    payload.emit_value = [&target](YAML::Node& node) {
      if (!target.has_value()) {
        node = YAML::Node(YAML::NodeType::Null);
        return;
      }
      if constexpr (std::is_enum_v<inner>) {
        node = static_cast<std::underlying_type_t<inner>>(*target);
      } else if constexpr (detail::is_chrono_duration<inner>::value) {
        node = target->count();
      } else if constexpr (std::is_integral_v<inner> && sizeof(inner) == 1 && !std::is_same_v<inner, bool>) {
        node = static_cast<int>(*target);
      } else {
        node = *target;
      }
    };
  } else if constexpr (std::is_enum_v<T>) {
    payload.emit_value = [&target](YAML::Node& node) {
      node = static_cast<std::underlying_type_t<T>>(target);
    };
  } else if constexpr (detail::is_chrono_duration<T>::value) {
    payload.emit_value = [&target](YAML::Node& node) { node = target.count(); };
  } else if constexpr (std::is_integral_v<T> && sizeof(T) == 1 && !std::is_same_v<T, bool>) {
    // yaml-cpp renders uint8_t / int8_t as characters by default; cast to int.
    payload.emit_value = [&target](YAML::Node& node) { node = static_cast<int>(target); };
  } else {
    payload.emit_value = [&target](YAML::Node& node) { node = target; };
  }
  leaf.body = std::move(payload);

  schema_node& inserted = push_child(std::move(leaf));
  return option_handle{opt, &inserted};
}

template <typename T>
option_handle config_builder::enumeration(const std::string&       flag,
                                          T&                       target,
                                          const std::string&       description,
                                          std::vector<std::string> values)
{
  static_assert(std::is_same_v<T, std::string>,
                "enumeration() currently supports std::string targets only.");

  option_handle h = option(flag, target, description);
  h.enum_values(std::move(values));
  return h;
}

template <typename E>
option_handle config_builder::enum_option(const std::string&                     flag,
                                          E&                                     target,
                                          const std::string&                     description,
                                          std::vector<std::pair<std::string, E>> mapping)
{
  // Extract legal names + provide an inverse mapping closure.
  std::vector<std::string> names;
  names.reserve(mapping.size());
  for (const auto& p : mapping) {
    names.push_back(p.first);
  }
  auto to_name = [mapping](const E& v) -> std::string {
    for (const auto& p : mapping) {
      if (p.second == v) {
        return p.first;
      }
    }
    return {};
  };
  auto setter = [&target, mapping](const std::string& s) {
    for (const auto& p : mapping) {
      if (p.first == s) {
        target = p.second;
        return;
      }
    }
  };

  CLI::Option* opt =
      ocudu::add_option_function<std::string>(*app_, flag, std::function<void(const std::string&)>{setter}, description);
  opt->default_str(to_name(target));
  opt->check(CLI::IsMember(names));

  const std::string canonical = detail::canonical_name(flag);
  // Same dedup behaviour as option(): CLI11 chains callbacks, schema keeps
  // the first registration as authoritative.
  for (auto& existing : std::get<group_node>(root_->body).children) {
    if (existing.name == canonical && std::holds_alternative<leaf_node>(existing.body)) {
      return option_handle{opt, &existing};
    }
  }

  schema_node leaf;
  leaf.name        = canonical;
  leaf.description = description;

  leaf_node payload;
  payload.type        = scalar_type::string;
  payload.default_str = to_name(target);
  payload.constraints.emplace_back(enum_constraint{names});
  payload.emit_value = [&target, to_name](YAML::Node& node) { node = to_name(target); };
  leaf.body          = std::move(payload);

  schema_node& inserted = push_child(std::move(leaf));
  return option_handle{opt, &inserted};
}

template <typename E>
option_handle config_builder::auto_enum_option(const std::string& flag,
                                               std::optional<E>&  target,
                                               const std::string& description)
{
  static_assert(std::is_enum_v<E>, "auto_enum_option target must be std::optional<enum>");
  using underlying = std::underlying_type_t<E>;

  auto setter = [&target](const std::string& in) {
    if (in.empty() || in == "auto") {
      return;
    }
    std::stringstream ss(in);
    underlying        v;
    ss >> v;
    target = static_cast<E>(v);
  };
  CLI::Option* opt =
      ocudu::add_option_function<std::string>(*app_, flag, std::function<void(const std::string&)>{setter}, description);
  opt->check([](const std::string& in_str) -> std::string {
    if (in_str == "auto" || in_str.empty()) {
      return {};
    }
    CLI::TypeValidator<int> int_validator("INTEGER");
    return int_validator(in_str);
  });
  opt->default_str("auto");

  const std::string canonical = detail::canonical_name(flag);
  for (auto& existing : std::get<group_node>(root_->body).children) {
    if (existing.name == canonical && std::holds_alternative<leaf_node>(existing.body)) {
      return option_handle{opt, &existing};
    }
  }

  schema_node leaf;
  leaf.name        = canonical;
  leaf.description = description;

  leaf_node payload;
  payload.type           = scalar_type::integer;
  payload.integer_bits   = static_cast<int>(sizeof(underlying) * 8);
  payload.integer_signed = std::is_signed_v<underlying>;
  payload.default_str    = std::string("auto");
  payload.notes.emplace_back("set to \"auto\" to auto-derive");
  payload.emit_value = [&target](YAML::Node& node) {
    if (!target.has_value()) {
      node = std::string("auto");
      return;
    }
    node = static_cast<long long>(static_cast<underlying>(*target));
  };
  leaf.body = std::move(payload);

  schema_node& inserted = push_child(std::move(leaf));
  return option_handle{opt, &inserted};
}

template <typename Configurator>
void config_builder::group(const std::string& name, const std::string& description, Configurator&& configurator)
{
  CLI::App* subcmd = add_subcommand(*app_, name, description);

  // Multiple callers may contribute to the same group (e.g. several helpers
  // all extending the "metrics" subcommand). Reuse an existing child when
  // present so the schema mirrors the CLI11 subcommand structure.
  auto&        children = std::get<group_node>(root_->body).children;
  schema_node* target   = nullptr;
  for (auto& c : children) {
    if (c.name == name && std::holds_alternative<group_node>(c.body)) {
      target = &c;
      break;
    }
  }
  if (target == nullptr) {
    schema_node child;
    child.name        = name;
    child.description = description;
    child.body        = group_node{};
    target            = &push_child(std::move(child));
  }

  config_builder sub(*subcmd, *target);
  configurator(sub);
}

template <typename Container, typename ElementConfigurator>
array_handle config_builder::array_of(const std::string&    flag,
                                      Container&            target,
                                      const std::string&    description,
                                      ElementConfigurator&& element_configurator)
{
  using element_t = typename Container::value_type;
  static_assert(std::is_default_constructible_v<element_t>,
                "array_of element type must be default-constructible (used to extract items shape).");

  // 1) Build the items shape once from an exemplar.
  auto items_shape = std::make_shared<group_node>();
  {
    element_t   exemplar{};
    CLI::App    throwaway_app("array_of shape probe");
    schema_node probe_root;
    probe_root.body = group_node{};
    config_builder probe(throwaway_app, probe_root);
    element_configurator(probe, exemplar);
    *items_shape = std::move(std::get<group_node>(probe_root.body));
  }

  // 2) Wire the CLI11 parse callback: standard vector<string>+nested-CLI11 hack.
  auto elem_cfg_copy = element_configurator;
  auto parse_lambda  = [&target, elem_cfg_copy](const std::vector<std::string>& values) {
    target.resize(values.size());
    for (std::size_t i = 0; i < values.size(); ++i) {
      CLI::App subapp("");
      subapp.config_formatter(create_yaml_config_parser());
      subapp.allow_config_extras(CLI::config_extras_mode::capture);
      schema_node discard;
      discard.body = group_node{};
      config_builder elem_builder(subapp, discard);
      elem_cfg_copy(elem_builder, target[i]);
      std::istringstream ss(values[i]);
      subapp.parse_from_stream(ss);
    }
  };
  CLI::Option* opt = add_option_cell(*app_, flag, parse_lambda, description);

  // 3) Register the array node. for_each_element re-builds metadata per actual
  //    element at emission time, so renderers bind the live struct.
  schema_node child;
  child.name        = detail::canonical_name(flag);
  child.description = description;

  array_node arr;
  arr.items_shape       = std::move(items_shape);
  arr.for_each_element  = [&target, elem_cfg_copy](std::function<void(group_node&)> visit) {
    for (auto& elem : target) {
      CLI::App    throwaway_app("array_of emit");
      schema_node tmp_root;
      tmp_root.body = group_node{};
      config_builder elem_builder(throwaway_app, tmp_root);
      elem_cfg_copy(elem_builder, elem);
      visit(std::get<group_node>(tmp_root.body));
    }
  };
  child.body = std::move(arr);

  schema_node& inserted = push_child(std::move(child));
  return array_handle{opt, &inserted};
}

} // namespace config
} // namespace ocudu
