// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "worker_manager_cli11_schema.h"
#include "apps/services/worker_manager/cli11_cpu_affinities_parser_helper.h"
#include "worker_manager_appconfig.h"
#include "ocudu/support/cli11_utils.h"
#include "ocudu/support/error_handling.h"

using namespace ocudu;

static void configure_cli11_affinities_args(CLI::App& app, expert_execution_appconfig& config)
{
  add_option_function<std::string>(
      app,
      "--main_pool_cpus",
      [&config](const std::string& value) {
        parse_affinity_mask(config.affinities.main_pool_cpu_cfg.mask, value, "main_pool_cpus");
      },
      "CPU cores assigned to main thread pool");
  add_option_function<std::string>(
      app,
      "--main_pool_pinning",
      [&config](const std::string& value) {
        config.affinities.main_pool_cpu_cfg.pinning_policy = to_affinity_mask_policy(value);
        if (config.affinities.main_pool_cpu_cfg.pinning_policy == sched_affinity_mask_policy::last) {
          report_error("Incorrect value={} used in {} property", value, "main_pool_pinning");
        }
      },
      "Policy used for assigning CPU cores to the main thread pool");
}

static void configure_cli11_threads_args(CLI::App& app, expert_execution_appconfig& config)
{
  CLI::App* main_pool_subcmd = add_subcommand(app, "main_pool", "Main thread pool configuration")->configurable();
  add_option(*main_pool_subcmd,
             "--nof_threads",
             config.threads.main_pool.nof_threads,
             "Number of threads for processing upper PHY and upper layers.")
      ->capture_default_str();
  add_option(*main_pool_subcmd,
             "--task_queue_size",
             config.threads.main_pool.task_queue_size,
             "Main thread pool task queue size.")
      ->capture_default_str();
  add_option(*main_pool_subcmd,
             "--backoff_period",
             config.threads.main_pool.backoff_period,
             "Main thread pool back-off period, in microseconds.")
      ->capture_default_str();
}

void ocudu::configure_cli11_with_worker_manager_appconfig_schema(CLI::App& app, expert_execution_appconfig& config)
{
  CLI::App* top_subcmd = add_subcommand(app, "expert_execution", "Expert execution configuration")->configurable();
  CLI::App* affinities_subcmd =
      add_subcommand(*top_subcmd, "affinities", "Application CPU affinities configuration")->configurable();
  configure_cli11_affinities_args(*affinities_subcmd, config);
  CLI::App* threads_subcmd = add_subcommand(*top_subcmd, "threads", "Threads configuration")->configurable();
  configure_cli11_threads_args(*threads_subcmd, config);
}
