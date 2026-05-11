// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "tracer_appconfig_cli11_schema.h"
#include "apps/helpers/config/config_builder.h"
#include "tracer_appconfig.h"

using namespace ocudu;

void ocudu::configure_cli11_with_tracer_appconfig_schema(config::config_builder& b, tracer_appconfig& config)
{
  b.group("trace", "General tracer configuration", [&](config::config_builder& tr) {
    tr.option("--filename", config.filename, "Set to a valid file path to enable tracing and write the trace to the file");
    tr.option("--max_tracing_events_per_file",
              config.max_tracing_events_per_file,
              "Maximum number of events per file. Set to zero for no limit");
    tr.option("--nof_tracing_events_after_severe",
              config.nof_tracing_events_after_severe,
              "Number of events to write prior to a severe event. Set to zero for writing all events");
  });
}

void ocudu::configure_cli11_with_tracer_appconfig_schema(CLI::App& app, tracer_appconfig& config)
{
  config::schema_node discard;
  discard.body = config::group_node{};
  config::config_builder b(app, discard);
  configure_cli11_with_tracer_appconfig_schema(b, config);
}
