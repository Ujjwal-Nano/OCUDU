// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "du_appconfig_cli11_schema_extra.h"
#include "apps/helpers/f1u/f1u_appconfig.h"
#include "apps/helpers/hal/hal_appconfig_extra.h"
#include "du_appconfig.h"

using namespace ocudu;

#ifdef DPDK_FOUND
static void manage_hal_optional(CLI::App& app, du_appconfig& du_cfg)
{
  if (!is_hal_section_present(app)) {
    du_cfg.hal_config.reset();
  }
}
#endif

static void configure_default_f1u(du_appconfig& du_cfg)
{
  if (du_cfg.f1u_cfg.f1u_sockets.f1u_socket_cfg.empty()) {
    f1u_socket_appconfig default_f1u_cfg;
    default_f1u_cfg.bind_addr = "127.0.10.2";
    du_cfg.f1u_cfg.f1u_sockets.f1u_socket_cfg.push_back(default_f1u_cfg);
  }
}

void ocudu::autoderive_du_parameters_after_parsing(CLI::App& app, du_appconfig& du_cfg)
{
#ifdef DPDK_FOUND
  manage_hal_optional(app, du_cfg);
#endif

  configure_default_f1u(du_cfg);
}
