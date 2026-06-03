// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "lib/e2/procedures/e2_reconnection_routine.h"
#include "tests/unittests/e2/common/e2_test_helpers.h"
#include "ocudu/support/async/async_test_utils.h"
#include <gtest/gtest.h>

using namespace ocudu;

/// Connection manager stub configurable to fail TNL a fixed number of times before succeeding.
class dummy_e2_conn_mngr_reconnect : public e2_connection_manager
{
public:
  int tnl_fail_count   = 0; // fail the first N TNL requests
  int tnl_call_count   = 0;
  int disconnect_count = 0;

  bool handle_e2_tnl_connection_request() override
  {
    ++tnl_call_count;
    return tnl_call_count > tnl_fail_count;
  }

  async_task<void> handle_e2_disconnection_request() override
  {
    ++disconnect_count;
    return launch_async([](coro_context<async_task<void>>& ctx) {
      CORO_BEGIN(ctx);
      CORO_RETURN();
    });
  }

  async_task<e2_setup_response_message> handle_e2_setup_request(const e2_setup_request_message& /*req*/) override
  {
    auto resp = response;
    return launch_async([resp](coro_context<async_task<e2_setup_response_message>>& ctx) mutable {
      CORO_BEGIN(ctx);
      CORO_RETURN(resp);
    });
  }

  e2_setup_response_message response = {};
};

/// Subscription manager stub that records stop() calls.
class dummy_e2_sub_mngr_reconnect : public e2_subscription_manager
{
public:
  int stop_count = 0;

  e2_subscribe_reponse_message handle_subscription_setup(const asn1::e2ap::ric_sub_request_s&) override { return {}; }
  e2_subscribe_delete_response_message handle_subscription_delete(const asn1::e2ap::ric_sub_delete_request_s&) override
  {
    return {};
  }
  void
  start_subscription(const asn1::e2ap::ric_request_id_s&, uint16_t, e2_event_manager&, e2_message_notifier&) override
  {
  }
  void stop_subscription(const asn1::e2ap::ric_request_id_s&,
                         e2_event_manager&,
                         const asn1::e2ap::ric_sub_delete_request_s&) override
  {
  }
  void            add_e2sm_service(std::string, std::unique_ptr<e2sm_interface>) override {}
  e2sm_interface* get_e2sm_interface(std::string) override { return nullptr; }
  void            add_ran_function_oid(uint16_t, std::string) override {}
  void            stop() override { ++stop_count; }
};

class e2_reconnection_routine_test : public ::testing::Test
{
protected:
  void SetUp() override
  {
    ocudulog::fetch_basic_logger("TEST").set_level(ocudulog::basic_levels::debug);
    ocudulog::init();

    cfg                             = config_helpers::make_default_e2ap_config();
    cfg.gnb_du_id                   = int_to_gnb_du_id(1);
    cfg.e2sm_kpm_enabled            = true;
    cfg.ric_reconnection_retry_time = std::chrono::milliseconds{100};
    factory                         = timer_factory{timers, task_worker};

    du_meas_provider = std::make_unique<dummy_e2sm_kpm_du_meas_provider>();
    e2sm_kpm_packer  = std::make_unique<e2sm_kpm_asn1_packer>(*du_meas_provider);
    kpm_iface        = std::make_unique<e2sm_kpm_impl>(test_logger, *e2sm_kpm_packer, *du_meas_provider);
    e2sm_mngr        = std::make_unique<e2sm_manager>(test_logger);
    e2sm_mngr->add_e2sm_service(e2sm_kpm_asn1_packer::oid, std::move(kpm_iface));

    conn_mngr                   = std::make_unique<dummy_e2_conn_mngr_reconnect>();
    conn_mngr->response.success = true;
    sub_mngr                    = std::make_unique<dummy_e2_sub_mngr_reconnect>();
    node_cfg_event              = std::make_unique<manual_event<std::vector<e2_node_component_config>>>();
    node_cfg_provider           = std::make_unique<dummy_e2_node_component_config_provider>(*node_cfg_event);

    // Pre-deliver a node config so the setup routine doesn't block on the event.
    e2_node_component_config ncfg;
    ncfg.interface_type = e2_node_component_interface_type::f1;
    node_cfg_event->set(std::vector<e2_node_component_config>{std::move(ncfg)});
  }

  void TearDown() override { ocudulog::flush(); }

  void tick()
  {
    timers.tick();
    task_worker.run_pending_tasks();
  }

  e2ap_configuration cfg;
  timer_manager      timers;
  manual_task_worker task_worker{64};
  timer_factory      factory;

  std::unique_ptr<dummy_e2sm_kpm_du_meas_provider>                     du_meas_provider;
  std::unique_ptr<e2sm_kpm_asn1_packer>                                e2sm_kpm_packer;
  std::unique_ptr<e2sm_interface>                                      kpm_iface;
  std::unique_ptr<e2sm_manager>                                        e2sm_mngr;
  std::unique_ptr<dummy_e2_conn_mngr_reconnect>                        conn_mngr;
  std::unique_ptr<dummy_e2_sub_mngr_reconnect>                         sub_mngr;
  std::unique_ptr<manual_event<std::vector<e2_node_component_config>>> node_cfg_event;
  std::unique_ptr<dummy_e2_node_component_config_provider>             node_cfg_provider;

  ocudulog::basic_logger& test_logger = ocudulog::fetch_basic_logger("TEST");
};

/// When the TNL succeeds on the first attempt the routine completes after one setup.
TEST_F(e2_reconnection_routine_test, immediate_reconnect_when_tnl_succeeds)
{
  async_task<void> t = launch_async<e2_reconnection_routine>(
      cfg, *node_cfg_provider, *e2sm_mngr, *conn_mngr, *sub_mngr, factory, test_logger);
  lazy_task_launcher<void> launcher(t);
  task_worker.run_pending_tasks();

  ASSERT_TRUE(t.ready());
  ASSERT_EQ(sub_mngr->stop_count, 1);
  ASSERT_EQ(conn_mngr->tnl_call_count, 1);
}

/// Subscriptions are stopped before reconnection begins, even if the first attempt fails.
TEST_F(e2_reconnection_routine_test, subscriptions_stopped_on_reconnection)
{
  conn_mngr->tnl_fail_count = 2; // fail first two TNL attempts

  async_task<void> t = launch_async<e2_reconnection_routine>(
      cfg, *node_cfg_provider, *e2sm_mngr, *conn_mngr, *sub_mngr, factory, test_logger);
  lazy_task_launcher<void> launcher(t);
  task_worker.run_pending_tasks();

  // Subscriptions must be stopped even though reconnection is still in progress.
  ASSERT_EQ(sub_mngr->stop_count, 1);
  ASSERT_FALSE(t.ready());

  // Advance timers past two retry periods to allow the third (successful) attempt.
  for (unsigned i = 0; i < 2; ++i) {
    for (unsigned ms = 0; ms <= 100; ++ms) {
      tick();
    }
  }

  ASSERT_TRUE(t.ready());
  ASSERT_EQ(conn_mngr->tnl_call_count, 3);
}

/// When the RIC is absent the routine retries until TNL succeeds.
TEST_F(e2_reconnection_routine_test, retries_until_tnl_succeeds)
{
  conn_mngr->tnl_fail_count = 1;

  async_task<void> t = launch_async<e2_reconnection_routine>(
      cfg, *node_cfg_provider, *e2sm_mngr, *conn_mngr, *sub_mngr, factory, test_logger);
  lazy_task_launcher<void> launcher(t);
  task_worker.run_pending_tasks();

  ASSERT_FALSE(t.ready());

  // One retry period.
  for (unsigned ms = 0; ms <= 100; ++ms) {
    tick();
  }

  ASSERT_TRUE(t.ready());
  ASSERT_EQ(conn_mngr->tnl_call_count, 2);
}
