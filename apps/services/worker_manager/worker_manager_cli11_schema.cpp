// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "worker_manager_cli11_schema.h"
#include "apps/helpers/config/config_builder.h"
#include "cli11_cpu_affinities_parser_helper.h"
#include "worker_manager_appconfig.h"
#include "ocudu/adt/expected.h"
#include "ocudu/support/cli11_utils.h"
#include "ocudu/support/error_handling.h"

using namespace ocudu;

static void declare_main_pool_threads_args(config::config_builder& b, main_thread_pool_appconfig& config)
{
  b.option("--nof_threads", config.nof_threads, "Number of threads for processing upper PHY and upper layers.");
  b.option("--task_queue_size", config.task_queue_size, "Main thread pool task queue size.");
  b.option("--backoff_period", config.backoff_period, "Main thread pool back-off period, in microseconds.");
}

static void declare_cpu_affinities_args(config::config_builder& b, cpu_affinities_appconfig& config)
{
  // Affinities are string-parsed into typed structs at parse time; the
  // builder can't see the live values as typed leaves. Wire them through
  // cli11_app() so they're parsed correctly, even though they remain
  // invisible to the schema emitters.
  CLI::App& app = b.cli11_app();
  add_option_function<std::string>(
      app,
      "--main_pool_cpus",
      [&config](const std::string& value) {
        parse_affinity_mask(config.main_pool_cpu_cfg.mask, value, "main_pool_cpus");
      },
      "CPU cores assigned to main thread pool");
  add_option_function<std::string>(
      app,
      "--main_pool_pinning",
      [&config](const std::string& value) {
        config.main_pool_cpu_cfg.pinning_policy = to_affinity_mask_policy(value);
        if (config.main_pool_cpu_cfg.pinning_policy == sched_affinity_mask_policy::last) {
          report_error("Incorrect value={} used in {} property", value, "main_pool_pinning");
        }
      },
      "Policy used for assigning CPU cores to the main thread pool");
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
