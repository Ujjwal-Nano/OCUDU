// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "e2_reconnection_routine.h"
#include "e2_setup_routine.h"
#include "ocudu/support/async/async_timer.h"

using namespace ocudu;

e2_reconnection_routine::e2_reconnection_routine(const e2ap_configuration&          cfg_,
                                                 e2_node_component_config_provider& node_cfg_provider_,
                                                 e2sm_manager&                      e2sm_mngr_,
                                                 e2_connection_manager&             e2_conn_mng_,
                                                 e2_subscription_manager&           subscription_mngr_,
                                                 timer_factory                      timers_,
                                                 ocudulog::basic_logger&            logger_) :
  cfg(cfg_),
  node_cfg_provider(node_cfg_provider_),
  e2sm_mngr(e2sm_mngr_),
  e2_conn_mng(e2_conn_mng_),
  subscription_mngr(subscription_mngr_),
  timers(timers_),
  logger(logger_),
  retry_timer(timers_.create_timer())
{
}

void e2_reconnection_routine::operator()(coro_context<async_task<void>>& ctx)
{
  CORO_BEGIN(ctx);

  logger.info("E2 connection lost. Starting reconnection...");

  subscription_mngr.stop();

  for (;;) {
    // Cleanly tear down any stale TNL before attempting a fresh connection.
    CORO_AWAIT(e2_conn_mng.handle_e2_disconnection_request());

    if (e2_conn_mng.handle_e2_tnl_connection_request()) {
      CORO_AWAIT_VALUE(reconnected,
                       launch_async<e2_setup_routine>(cfg, node_cfg_provider, e2sm_mngr, e2_conn_mng, timers, logger));
      if (reconnected) {
        break;
      }
    }

    logger.info("E2 reconnection attempt failed. Retrying in {} ms.", cfg.ric_reconnection_retry_time.count());
    CORO_AWAIT(async_wait_for(retry_timer, cfg.ric_reconnection_retry_time));
  }

  logger.info("E2 reconnection successful.");
  CORO_RETURN();
}
