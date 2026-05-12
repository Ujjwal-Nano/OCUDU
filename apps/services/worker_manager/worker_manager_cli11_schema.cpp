// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "worker_manager_cli11_schema.h"
#include "apps/helpers/config/config_builder.h"
#include "cli11_cpu_affinities_parser_helper.h"
#include "worker_manager_appconfig.h"
#include "ocudu/adt/expected.h"
#include "ocudu/adt/span.h"
#include "ocudu/support/error_handling.h"
#include <fmt/format.h>
#include <fmt/ranges.h>

using namespace ocudu;

static void declare_main_pool_threads_args(config::config_builder& b, main_thread_pool_appconfig& config)
{
  b.option("--nof_threads", config.nof_threads, "Number of threads for processing upper PHY and upper layers.");
  b.option("--task_queue_size", config.task_queue_size, "Main thread pool task queue size.");
  b.option("--backoff_period", config.backoff_period, "Main thread pool back-off period, in microseconds.");
}

static void declare_cpu_affinities_args(config::config_builder& b, cpu_affinities_appconfig& config)
{
  // String-bound CPU-mask and pinning-policy options go through string_action:
  // the setter calls the existing parser, the getter renders the current
  // typed value back into the canonical string form. Schema sees a string
  // leaf with a free-text note about accepted shapes.
  b.string_action(
      "--main_pool_cpus",
      [&config](const std::string& value) {
        parse_affinity_mask(config.main_pool_cpu_cfg.mask, value, "main_pool_cpus");
      },
      [&config]() -> std::string {
        const auto& ids = config.main_pool_cpu_cfg.mask.get_cpu_ids();
        return fmt::format("{:,}", span<const size_t>(ids));
      },
      "CPU cores assigned to main thread pool",
      "comma-separated CPU ids or ranges, e.g. \"0-3,5,7\"");

  b.string_action(
      "--main_pool_pinning",
      [&config](const std::string& value) {
        config.main_pool_cpu_cfg.pinning_policy = to_affinity_mask_policy(value);
        if (config.main_pool_cpu_cfg.pinning_policy == sched_affinity_mask_policy::last) {
          report_error("Incorrect value={} used in {} property", value, "main_pool_pinning");
        }
      },
      [&config]() -> std::string { return to_string(config.main_pool_cpu_cfg.pinning_policy); },
      "Policy used for assigning CPU cores to the main thread pool",
      "one of: mask, round-robin");
}

void ocudu::configure_cli11_with_worker_manager_appconfig_schema(config::config_builder&     b,
                                                                 expert_execution_appconfig& config)
{
  b.group("expert_execution", "Expert execution configuration", [&](config::config_builder& expert) {
    expert.group("affinities", "Application CPU affinities configuration", [&](config::config_builder& aff) {
      declare_cpu_affinities_args(aff, config.affinities);
    });
    expert.group("threads", "Threads configuration", [&](config::config_builder& threads) {
      threads.group("main_pool", "Main thread pool configuration", [&](config::config_builder& mp) {
        declare_main_pool_threads_args(mp, config.threads.main_pool);
      });
    });
  });
}

void ocudu::configure_cli11_with_worker_manager_appconfig_schema(CLI::App& app, expert_execution_appconfig& config)
{
  config::schema_node discard;
  discard.body = config::group_node{};
  config::config_builder b(app, discard);
  configure_cli11_with_worker_manager_appconfig_schema(b, config);
}
