// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "e2_cli11_schema.h"
#include "apps/helpers/config/config_builder.h"
#include "apps/helpers/e2/e2_appconfig.h"
#include "apps/helpers/network/sctp_cli11_schema.h"

using namespace ocudu;

void ocudu::configure_cli11_with_e2_config_schema(config::config_builder& b,
                                                  e2_config&              config,
                                                  const std::string&      option_name,
                                                  const std::string&      option_description)
{
  b.group("e2", "E2 parameters", [&](config::config_builder& e2) {
    e2.option(option_name, config.enable_unit_e2, option_description);
    e2.option(
        "--addrs,--addr",
        config.ip_addrs,
        "RIC addresses to be used for E2 interface. Multiple addresses can be specified for SCTP multi-homing");
    e2.option("--port", config.port, "RIC port").range(20000, 40000);
    e2.option(
        "--bind_addrs,--bind_addr",
        config.bind_addrs,
        "Local bind addresses to be used for E2 interface. Multiple addresses can be specified for SCTP multi-homing. "
        "If left empty, implicit bind is performed");
    configure_cli11_sctp_socket_args(e2, config.sctp);
    e2.option("--e2sm_kpm_enabled", config.e2sm_kpm_enabled, "Enable KPM service module");
    e2.option("--e2sm_rc_enabled", config.e2sm_rc_enabled, "Enable RC service module");
    e2.option("--e2sm_ccc_enabled", config.e2sm_ccc_enabled, "Enable CCC service module");
  });
}

void ocudu::configure_cli11_with_e2_config_schema(CLI::App&          app,
                                                  e2_config&         config,
                                                  const std::string& option_name,
                                                  const std::string& option_description)
{
  config::schema_node discard;
  discard.body = config::group_node{};
  config::config_builder b(app, discard);
  configure_cli11_with_e2_config_schema(b, config, option_name, option_description);
}
