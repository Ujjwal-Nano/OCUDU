// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "dynamic_o_du_unit_cli11_schema.h"
#include "apps/helpers/config/config_builder.h"
#include "apps/helpers/metrics/metrics_config_cli11_schema.h"
#include "apps/services/worker_manager/cli11_cpu_affinities_parser_helper.h"
#include "apps/units/flexible_o_du/o_du_high/o_du_high_unit_config_cli11_schema.h"
#include "apps/units/flexible_o_du/o_du_low/du_low_config_cli11_schema.h"
#include "apps/units/flexible_o_du/split_7_2/helpers/ru_ofh_config_cli11_schema.h"
#include "apps/units/flexible_o_du/split_8/helpers/ru_sdr_config_cli11_schema.h"
#include "dynamic_o_du_unit_config.h"
#include "ocudu/adt/span.h"
#include "ocudu/support/error_handling.h"
#include <fmt/format.h>
#include <fmt/ranges.h>

using namespace ocudu;

static ru_ofh_unit_parsed_config ofh_cfg;
static ru_sdr_unit_config        sdr_cfg;
static ru_dummy_unit_config      dummy_cfg;

static void declare_ru_dummy_args(config::config_builder& b, ru_dummy_unit_config& config)
{
  b.option("--dl_processing_delay", config.dl_processing_delay, "DL processing processing delay in slots");
  b.option("--time_scaling",
           config.time_scaling,
           "Time scaling factor applied to the slot duration. Must be greater than zero. "
           "A value greater than one slows down the RU, while a value between zero and one speeds it up.")
      .min_value(0.0);
}

static void declare_cell_affinity_args(config::config_builder& b, ru_dummy_cpu_affinities_cell_unit_config& config)
{
  b.string_action(
      "--ru_cpus",
      [&config](const std::string& value) { parse_affinity_mask(config.ru_cpu_cfg.mask, value, "ru_cpus"); },
      [&config]() -> std::string {
        return fmt::format("{:,}", span<const size_t>(config.ru_cpu_cfg.mask.get_cpu_ids()));
      },
      "CPU cores used for the Radio Unit tasks",
      "comma-separated CPU ids or ranges, e.g. \"0-3,5\"");

  b.string_action(
      "--ru_pinning",
      [&config](const std::string& value) {
        config.ru_cpu_cfg.pinning_policy = to_affinity_mask_policy(value);
        if (config.ru_cpu_cfg.pinning_policy == sched_affinity_mask_policy::last) {
          report_error("Incorrect value={} used in {} property", value, "ru_pinning");
        }
      },
      [&config]() -> std::string { return to_string(config.ru_cpu_cfg.pinning_policy); },
      "Policy used for assigning CPU cores to the Radio Unit tasks",
      "one of: mask, round-robin");
}

static void declare_metrics_args(config::config_builder& b, ru_dummy_unit_metrics_config& config)
{
  b.group("layers", "Layer basis metrics configuration", [&](config::config_builder& l) {
    l.option("--enable_ru", config.enable_ru_metrics, "Enable Radio Unit metrics");
  });
}

void ocudu::configure_cli11_with_dynamic_o_du_unit_config_schema(config::config_builder&   b,
                                                                 dynamic_o_du_unit_config& parsed_cfg)
{
  configure_cli11_with_o_du_high_config_schema(b, parsed_cfg.odu_high_cfg);
  configure_cli11_with_du_low_config_schema(b, parsed_cfg.du_low_cfg);
  configure_cli11_with_ru_ofh_config_schema(b, ofh_cfg);
  configure_cli11_with_ru_sdr_config_schema(b, sdr_cfg);

  b.group("ru_dummy", "Dummy Radio Unit configuration",
          [&](config::config_builder& ru) { declare_ru_dummy_args(ru, dummy_cfg); });

  // Common metrics options (enable_json/log/verbose) under "metrics".
  app_helpers::configure_cli11_with_metrics_appconfig_schema(b, dummy_cfg.metrics_cfg.metrics_cfg);
  b.group("metrics", "Metrics configuration",
          [&](config::config_builder& m) { declare_metrics_args(m, dummy_cfg.metrics_cfg); });

  b.group("expert_execution", "Expert execution configuration", [&](config::config_builder& exec) {
    exec.array_of("--cell_affinities",
                  dummy_cfg.cell_affinities,
                  "Sets the cell CPU affinities configuration on a per cell basis",
                  [](config::config_builder& el, ru_dummy_cpu_affinities_cell_unit_config& c) {
                    declare_cell_affinity_args(el, c);
                  });
  });
}

void ocudu::configure_cli11_with_dynamic_o_du_unit_config_schema(CLI::App& app, dynamic_o_du_unit_config& parsed_cfg)
{
  config::schema_node discard;
  discard.body = config::group_node{};
  config::config_builder b(app, discard);
  configure_cli11_with_dynamic_o_du_unit_config_schema(b, parsed_cfg);
}

static void manage_ru(CLI::App& app, dynamic_o_du_unit_config& parsed_cfg)
{
  // Manage the RU optionals
  auto     ofh_subcmd        = app.get_subcommand("ru_ofh");
  auto     sdr_subcmd        = app.get_subcommand("ru_sdr");
  auto     dummy_subcmd      = app.get_subcommand("ru_dummy");
  unsigned nof_ofh_entries   = ofh_subcmd->count_all();
  unsigned nof_sdr_entries   = sdr_subcmd->count_all();
  unsigned nof_dummy_entries = dummy_subcmd->count_all();

  // Count the number of RU types.
  unsigned nof_ru_types = (nof_ofh_entries != 0) ? 1 : 0;
  nof_ru_types += (nof_sdr_entries != 0) ? 1 : 0;
  nof_ru_types += (nof_dummy_entries != 0) ? 1 : 0;

  if (nof_ru_types > 1) {
    ocudu_terminate("Radio Unit configuration allows either a SDR, Open Fronthaul, or Dummy configuration, but not "
                    "different types of them at the same time");
  }

  if (nof_ofh_entries != 0) {
    parsed_cfg.ru_cfg = ofh_cfg;
    sdr_subcmd->disabled();
    dummy_subcmd->disabled();

    return;
  }

  if (nof_sdr_entries != 0) {
    parsed_cfg.ru_cfg = sdr_cfg;
    ofh_subcmd->disabled();
    dummy_subcmd->disabled();

    return;
  }

  parsed_cfg.ru_cfg = dummy_cfg;
  sdr_subcmd->disabled();
  ofh_subcmd->disabled();
}

void ocudu::autoderive_dynamic_o_du_parameters_after_parsing(CLI::App& app, dynamic_o_du_unit_config& parsed_cfg)
{
  const unsigned nof_cells = parsed_cfg.odu_high_cfg.du_high_cfg.config.cells_cfg.size();
  autoderive_o_du_high_parameters_after_parsing(app, parsed_cfg.odu_high_cfg);
  // Auto derive SDR parameters.
  autoderive_ru_sdr_parameters_after_parsing(app, sdr_cfg, nof_cells);
  // Auto derive OFH parameters.
  autoderive_ru_ofh_parameters_after_parsing(app, ofh_cfg);

  // Set the parsed RU.
  manage_ru(app, parsed_cfg);

  if (std::holds_alternative<ru_dummy_unit_config>(parsed_cfg.ru_cfg)) {
    auto& dummy = std::get<ru_dummy_unit_config>(parsed_cfg.ru_cfg);
    if (dummy.cell_affinities.size() < nof_cells) {
      dummy.cell_affinities.resize(nof_cells);
    }
  }

  // Auto derive DU low parameters.
  const auto&   cell = parsed_cfg.odu_high_cfg.du_high_cfg.config.cells_cfg.front().cell;
  const nr_band band = cell.band ? cell.band.value() : band_helper::get_band_from_dl_arfcn(cell.dl_f_ref_arfcn);
  autoderive_du_low_parameters_after_parsing(app, parsed_cfg.du_low_cfg, band_helper::get_duplex_mode(band));
}
