// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "logger_appconfig_cli11_schema.h"
#include "apps/helpers/config/config_builder.h"
#include "logger_appconfig.h"
#include "logger_appconfig_cli11_utils.h"
#include "ocudu/support/cli11_utils.h"

using namespace ocudu;

static void declare_log_args(config::config_builder& b, logger_appconfig& cfg)
{
  b.option("--filename", cfg.filename, "Log file output path");

  // Log-level options are enum-via-string-function and still go through the
  // legacy CLI11 path: they translate strings to ocudulog::basic_levels at
  // parse time, so the builder can't see the live value as a typed leaf.
  // The schema-visible portion is what this function leaves through the
  // builder API; the level options are wired below via cli11_app().
  CLI::App& app = b.cli11_app();

  app_helpers::add_log_option(
      app, cfg.all_level, "--all_level", "Default log level for PHY, MAC, RLC, PDCP, RRC, SDAP, NGAP and GTPU");
  app_helpers::add_log_option(app, cfg.lib_level, "--lib_level", "Generic log level ");
  app_helpers::add_log_option(app, cfg.e2ap_level, " --e2ap_level", "E2AP log level");

  auto config_level_check = [](const std::string& value) -> std::string {
    if (auto level = ocudulog::str_to_basic_level(value);
        !level.has_value() || level.value() == ocudulog::basic_levels::error ||
        level.value() == ocudulog::basic_levels::warning) {
      return "Log level value not supported. Accepted values [none,info,debug]";
    }
    return {};
  };

  add_option_function<std::string>(
      app, " --config_level", app_helpers::capture_log_level_function(cfg.config_level), "Config log level")
      ->default_str(ocudulog::basic_level_to_string(cfg.config_level))
      ->check(config_level_check);

  b.option("--hex_max_size",
           cfg.hex_max_size,
           "Maximum number of bytes to print in hex (zero for no hex dumps, -1 for unlimited bytes)")
      .range(-1, 1024);

  // Post-parsing callback. This allows us to set the log level to "all" level
  // if no individual level is provided.
  app.callback([&app, &cfg]() {
    if (app.count("--all_level") == 0 || cfg.all_level == ocudulog::basic_levels::warning) {
      return;
    }
    const auto options = app.get_options();
    for (auto* option : options) {
      if (option->check_name("--all_level") || option->get_single_name().find("level") == std::string::npos) {
        continue;
      }
      if (option->count()) {
        continue;
      }
      if (option->check_name("--config_level")) {
        if (cfg.all_level == ocudulog::basic_levels::error) {
          option->default_val<std::string>("none");
          continue;
        }
      }
      if (option->check_name("--lib_level")) {
        continue;
      }
      option->default_val<std::string>(ocudulog::basic_level_to_string(cfg.all_level));
    }
  });
}

void ocudu::configure_cli11_with_logger_appconfig_schema(config::config_builder& b, logger_appconfig& config)
{
  b.group("log", "Logging configuration", [&](config::config_builder& log_b) { declare_log_args(log_b, config); });
}

void ocudu::configure_cli11_with_logger_appconfig_schema(CLI::App& app, logger_appconfig& config)
{
  config::schema_node discard;
  discard.body = config::group_node{};
  config::config_builder b(app, discard);
  configure_cli11_with_logger_appconfig_schema(b, config);
}
