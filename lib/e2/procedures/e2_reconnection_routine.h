// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

#include "ocudu/e2/e2.h"
#include "ocudu/e2/e2_node_component_config_provider.h"
#include "ocudu/e2/e2ap_configuration.h"
#include "ocudu/e2/e2sm/e2sm_manager.h"
#include "ocudu/e2/subscription/e2_subscription.h"
#include "ocudu/support/async/async_task.h"
#include "ocudu/support/timers.h"

namespace ocudu {

/// Recovers an E2 connection after loss by looping e2_setup_routine until the RIC accepts.
///
/// On each iteration: disconnect any stale TNL, attempt a new TNL connection, then run
/// e2_setup_routine.  On failure the routine waits ric_reconnection_retry_time before retrying.
class e2_reconnection_routine
{
public:
  e2_reconnection_routine(const e2ap_configuration&          cfg,
                          e2_node_component_config_provider& node_cfg_provider,
                          e2sm_manager&                      e2sm_mngr,
                          e2_connection_manager&             e2_conn_mng,
                          e2_subscription_manager&           subscription_mngr,
                          timer_factory                      timers,
                          ocudulog::basic_logger&            logger);

  void operator()(coro_context<async_task<void>>& ctx);

  static const char* name() { return "E2 Reconnection Routine"; }

private:
  const e2ap_configuration&          cfg;
  e2_node_component_config_provider& node_cfg_provider;
  e2sm_manager&                      e2sm_mngr;
  e2_connection_manager&             e2_conn_mng;
  e2_subscription_manager&           subscription_mngr;
  timer_factory                      timers;
  ocudulog::basic_logger&            logger;

  unique_timer retry_timer;
  bool         reconnected = false;
};

} // namespace ocudu
