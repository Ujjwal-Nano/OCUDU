// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "tracer_appconfig_cli11_schema.h"
#include "tracer_appconfig.h"
#include "ocudu/support/cli11_utils.h"

using namespace ocudu;

static void configure_cli11_trace_args(CLI::App& app, tracer_appconfig& config)
{
  add_option(
      app, "--filename", config.filename, "Set to a valid file path to enable tracing and write the trace to the file")
      ->capture_default_str();
  add_option(app,
             "--max_tracing_events_per_file",
             config.max_tracing_events_per_file,
             "Maximum number of events per file. Set to zero for no limit")
      ->capture_default_str();
  add_option(app,
             "--nof_tracing_events_after_severe",
             config.nof_tracing_events_after_severe,
             "Number of events to write prior to a severe event. Set to zero for writing all events")
      ->capture_default_str();
}

void ocudu::configure_cli11_with_tracer_appconfig_schema(CLI::App& app, tracer_appconfig& config)
{
  CLI::App* trace_subcmd = add_subcommand(app, "trace", "General tracer configuration")->configurable();
  configure_cli11_trace_args(*trace_subcmd, config);
}
