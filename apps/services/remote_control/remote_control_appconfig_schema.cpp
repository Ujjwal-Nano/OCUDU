// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "apps/services/remote_control/remote_control_appconfig_schema.h"
#include "apps/helpers/config/config_builder.h"
#include "apps/services/remote_control/remote_control_appconfig.h"

using namespace ocudu;

void ocudu::declare_remote_control_appconfig_schema(config::config_builder&   b,
                                                                 remote_control_appconfig& config)
{
  b.group("remote_control", "Remote control configuration", [&](config::config_builder& rc) {
    rc.option("--enabled", config.enabled, "Enables the Remote Control Server");
    rc.option("--bind_addr", config.bind_addr, "Remote Control Server bind address");
    rc.option("--port", config.port, "Port where the remote control server listens for incoming connections")
        .range(0, 65535);
  });

  b.group("metrics", "Metrics configuration", [&](config::config_builder& m) {
    m.option("--enable_json", config.enable_metrics_subscription, "Enable JSON metrics reporting");
  });
}

