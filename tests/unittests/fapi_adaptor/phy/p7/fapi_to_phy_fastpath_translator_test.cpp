// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "../../../phy/support/resource_grid_test_doubles.h"
#include "../../../phy/upper/downlink_processor_test_doubles.h"
#include "../../../phy/upper/uplink_request_processor_test_doubles.h"
#include "fapi_to_phy_fastpath_translator.h"
#include "message_builder_helpers.h"
#include "phy_to_fapi_time_event_fastpath_translator.h"
#include "ocudu/fapi/p7/builders/slot_indication_builder.h"
#include "ocudu/fapi/p7/p7_slot_indication_notifier.h"
#include "ocudu/fapi_adaptor/precoding_matrix_table_generator.h"
#include "ocudu/fapi_adaptor/uci_part2_correspondence_generator.h"
#include "ocudu/phy/support/resource_grid_pool.h"
#include "ocudu/phy/upper/downlink_processor.h"
#include "ocudu/phy/upper/uplink_pdu_slot_repository.h"
#include "ocudu/phy/upper/uplink_pdu_validator.h"
#include "ocudu/phy/upper/upper_phy_timing_context.h"
#include "ocudu/support/executors/manual_task_worker.h"
#include <gtest/gtest.h>

using namespace ocudu;
using namespace fapi_adaptor;
using namespace unittest;

namespace {

/// Error indication notifier spy implementation.
class error_indication_notifier_spy : public fapi::error_indication_notifier
{
  fapi::error_indication message;
  bool                   error_indication_detected = false;

public:
  void on_error_indication(const fapi::error_indication& msg) override
  {
    message                   = msg;
    error_indication_detected = true;
  }

  bool has_on_error_indication_been_called() const { return error_indication_detected; }

  const fapi::error_indication& get_message() const { return message; }
};

class downlink_pdu_validator_dummy : public downlink_pdu_validator
{
public:
  error_type<std::string> is_valid(const ssb_processor::pdu_t& pdu, unsigned bandwidth_nof_prbs) const override
  {
    return default_success_t();
  }
  error_type<std::string> is_valid(const pdcch_processor::pdu_t& pdu) const override { return default_success_t(); }
  error_type<std::string> is_valid(const pdsch_processor::pdu_t& pdu) const override { return default_success_t(); }
  error_type<std::string> is_valid(const nzp_csi_rs_generator::config_t& config) const override
  {
    return default_success_t();
  }
  error_type<std::string> is_valid(const prs_generator_configuration& config) const override
  {
    return default_success_t();
  }
};

class uplink_pdu_validator_dummy : public uplink_pdu_validator
{
public:
  error_type<std::string> is_valid(const prach_detector::configuration& config) const override
  {
    return default_success_t();
  }
  error_type<std::string> is_valid(const pucch_processor::format0_configuration& config) const override
  {
    return default_success_t();
  }
  error_type<std::string> is_valid(const pucch_processor::format1_configuration& config) const override
  {
    return default_success_t();
  }
  error_type<std::string> is_valid(const pucch_processor::format2_configuration& config) const override
  {
    return default_success_t();
  }
  error_type<std::string> is_valid(const pucch_processor::format3_configuration& config) const override
  {
    return default_success_t();
  }
  error_type<std::string> is_valid(const pucch_processor::format4_configuration& config) const override
  {
    return default_success_t();
  }
  error_type<std::string> is_valid(const pusch_processor::pdu_t& pdu) const override { return default_success_t(); }
  error_type<std::string> is_valid(const srs_estimator_configuration& config) const override
  {
    return default_success_t();
  }
};

class resource_grid_pool_dummy : public resource_grid_pool, private shared_resource_grid::pool_interface
{
public:
  explicit resource_grid_pool_dummy() {}

  shared_resource_grid allocate_resource_grid(slot_point slot) override
  {
    unsigned expected_available_ref_count = 0;
    bool     available                    = ref_count.compare_exchange_strong(expected_available_ref_count, 1);
    ocudu_assert(available, "The grid must NOT be reserved.");
    return {*this, ref_count};
  }

  unsigned get_getter_count() const { return getter_count; }

  bool is_available() const { return ref_count == 0; }

private:
  resource_grid& get() override
  {
    ocudu_assert(ref_count != 0, "Reference counter must NOT be zero.");
    ++getter_count;
    return grid;
  }

  void notify_release_scope() override { ocudu_assert(ref_count == 0, "Reference counter must be zero."); }

  std::atomic<unsigned> ref_count    = {};
  unsigned              getter_count = 0;

  resource_grid_dummy grid;
};

class downlink_processor_pool_dummy : public downlink_processor_pool
{
  static_vector<downlink_processor_spy, 2> dl_processor;

public:
  downlink_processor_pool_dummy()
  {
    dl_processor.emplace_back(0);
    dl_processor.emplace_back(1);
  }

  void stop() override
  {
    // Do nothing.
  }

  downlink_processor_controller& get_processor_controller(slot_point slot) override
  {
    return dl_processor[slot.slot_index()];
  }

  const downlink_processor_spy& processor(slot_point slot) const { return dl_processor[slot.slot_index()]; }
};

/// Spy implementation of an uplink request processor.
class uplink_request_processor_spy : public uplink_request_processor
{
  bool is_prach_requested  = false;
  bool is_uplink_requested = false;

public:
  void process_prach_request(const prach_buffer_context& context) override { is_prach_requested = true; }
  void process_uplink_slot_request(const resource_grid_context& context, const shared_resource_grid& grid) override
  {
    is_uplink_requested = true;
  }

  /// Returns true if uplink has been requested, otherwise false.
  bool has_uplink_been_requested() const { return is_uplink_requested; }
};

class uplink_pdu_slot_repository_spy : public uplink_pdu_slot_repository_pool,
                                       private unique_uplink_pdu_slot_repository::uplink_pdu_slot_repository_callback,
                                       private shared_resource_grid::pool_interface
{
public:
  uplink_pdu_slot_repository_spy() : grid_spy(rg_reader, rg_writer) {}

  unique_uplink_pdu_slot_repository get_pdu_slot_repository(slot_point slot) override
  {
    ocudu_assert(pusch_pdus.empty(), "PUSCH PDU list is not empty.");
    ocudu_assert(pucch_pdus.empty(), "PUCCH PDU list is not empty.");
    ocudu_assert(srs_pdus.empty(), "SRS PDU list is not empty.");
    current_slot = slot;
    return unique_uplink_pdu_slot_repository(*this);
  }

private:
  shared_resource_grid finish_adding_pdus() override { return {*this, grid_ref_count}; }

  void add_pusch_pdu(const pusch_pdu& pdu) override { pusch_pdus.emplace_back(pdu); }

  void add_pucch_pdu(const pucch_pdu& pdu) override { pucch_pdus.emplace_back(pdu); }

  void add_srs_pdu(const srs_pdu& pdu) override { srs_pdus.emplace_back(pdu); }

  resource_grid& get() override { return grid_spy; }

  void notify_release_scope() override {}

  slot_point               current_slot;
  std::vector<pusch_pdu>   pusch_pdus;
  std::vector<pucch_pdu>   pucch_pdus;
  std::vector<srs_pdu>     srs_pdus;
  resource_grid_reader_spy rg_reader;
  resource_grid_writer_spy rg_writer;
  resource_grid_spy        grid_spy;
  std::atomic<unsigned>    grid_ref_count = 0;
};

} // namespace

class fapi_to_phy_translator_fixture : public ::testing::Test
{
protected:
  downlink_processor_pool_dummy          dl_processor_pool;
  resource_grid_pool_dummy               rg_pool;
  uplink_request_processor_spy           ul_request_processor;
  uplink_pdu_slot_repository_spy         pdu_repo;
  const unsigned                         sector_id         = 0;
  const unsigned                         headroom_in_slots = 2;
  const subcarrier_spacing               scs               = subcarrier_spacing::kHz15;
  const slot_point                       slot              = {scs, 1, 0};
  rach_config_common                     prach_cfg;
  fapi::carrier_config                   carrier_cfg = {0, 0, 11, 0, 0, 0, 0, 0};
  downlink_pdu_validator_dummy           dl_pdu_validator;
  uplink_pdu_validator_dummy             ul_pdu_validator;
  error_indication_notifier_spy          error_notifier_spy;
  manual_task_worker                     worker;
  fapi_to_phy_fastpath_translator_config config =
      {sector_id, headroom_in_slots, false, scs, prach_cfg, carrier_cfg, {0}};
  fapi_to_phy_fastpath_translator_dependencies dependencies = {
      ocudulog::fetch_basic_logger("FAPI"),
      dl_processor_pool,
      rg_pool,
      dl_pdu_validator,
      ul_request_processor,
      pdu_repo,
      ul_pdu_validator,
      std::move(std::get<std::unique_ptr<precoding_matrix_repository>>(generate_precoding_matrix_tables(1, 0))),
      std::move(std::get<std::unique_ptr<uci_part2_correspondence_repository>>(generate_uci_part2_correspondence(1)))};
  fapi_to_phy_fastpath_translator translator;

public:
  fapi_to_phy_translator_fixture() : worker(1), translator(config, std::move(dependencies))
  {
    translator.set_error_indication_notifier(error_notifier_spy);
    translator.handle_new_slot(slot);
  }
};

TEST_F(fapi_to_phy_translator_fixture, downlink_processor_is_not_configured_on_new_slot)
{
  ASSERT_FALSE(dl_processor_pool.processor(slot).has_configure_resource_grid_method_been_called());
  ASSERT_TRUE(rg_pool.is_available());
  ASSERT_FALSE(error_notifier_spy.has_on_error_indication_been_called());
}

TEST_F(fapi_to_phy_translator_fixture, downlink_processor_is_configured_on_new_dl_tti_request)
{
  ASSERT_FALSE(dl_processor_pool.processor(slot).has_configure_resource_grid_method_been_called());
  ASSERT_EQ(rg_pool.get_getter_count(), 0);

  fapi::dl_tti_request msg;
  msg.slot = slot;
  // Add a PDU to the message.
  msg.pdus.emplace_back();

  translator.send_dl_tti_request(msg);

  // Assert that the downlink processor is configured.
  ASSERT_TRUE(dl_processor_pool.processor(slot).has_configure_resource_grid_method_been_called());

  ASSERT_FALSE(error_notifier_spy.has_on_error_indication_been_called());
}

TEST_F(fapi_to_phy_translator_fixture, dl_ssb_pdu_is_processed)
{
  fapi::dl_tti_request msg = build_valid_dl_tti_request();
  slot_point           msg_slot(msg.slot);

  ASSERT_FALSE(dl_processor_pool.processor(msg_slot).has_configure_resource_grid_method_been_called());
  ASSERT_EQ(rg_pool.get_getter_count(), 0);

  translator.handle_new_slot(msg_slot);
  // Process SSB PDU.
  translator.send_dl_tti_request(msg);

  // Assert that the downlink processor is configured.
  ASSERT_TRUE(dl_processor_pool.processor(msg_slot).has_configure_resource_grid_method_been_called());
  ASSERT_TRUE(dl_processor_pool.processor(msg_slot).has_process_ssb_method_been_called());
  // Assert that the resource grid has NOT been set to zero.
  ASSERT_EQ(rg_pool.get_getter_count(), 0);

  translator.handle_new_slot(msg_slot + 1);
  ++msg.slot;
  translator.send_dl_tti_request(msg);

  // Assert that the finish processing PDUs method of the previous slot downlink_processor has been called.
  ASSERT_TRUE(dl_processor_pool.processor(msg_slot).has_finish_processing_pdus_method_been_called());
  ASSERT_FALSE(error_notifier_spy.has_on_error_indication_been_called());
}

TEST_F(fapi_to_phy_translator_fixture, dl_ssb_pdu_within_allowed_delay_is_processed)
{
  const fapi::dl_tti_request& msg = build_valid_dl_tti_request();
  slot_point                  msg_slot(msg.slot);

  ASSERT_FALSE(dl_processor_pool.processor(msg_slot).has_configure_resource_grid_method_been_called());
  ASSERT_EQ(rg_pool.get_getter_count(), 0);

  translator.handle_new_slot(msg_slot);

  // Increase the slots.
  for (unsigned i = 1; i != headroom_in_slots; ++i) {
    translator.handle_new_slot(msg_slot + i);
  }

  // Send a DL_TTI.request.
  translator.send_dl_tti_request(msg);

  // Assert that the downlink processor is configured.
  ASSERT_TRUE(dl_processor_pool.processor(msg_slot).has_configure_resource_grid_method_been_called());
  ASSERT_TRUE(dl_processor_pool.processor(msg_slot).has_process_ssb_method_been_called());
  // Assert that the resource grid has NOT been set to zero.
  ASSERT_EQ(rg_pool.get_getter_count(), 0);
  ASSERT_FALSE(error_notifier_spy.has_on_error_indication_been_called());
}

TEST_F(fapi_to_phy_translator_fixture, receiving_a_dl_tti_request_sends_previous_slot)
{
  ASSERT_FALSE(dl_processor_pool.processor(slot).has_configure_resource_grid_method_been_called());
  ASSERT_EQ(rg_pool.get_getter_count(), 0);

  fapi::dl_tti_request msg;
  msg.slot = slot;
  // Add a pdu to the message.
  msg.pdus.emplace_back();

  // Increase the slots.
  for (unsigned i = 1; i != headroom_in_slots; ++i) {
    translator.handle_new_slot(slot + i);
  }

  // Send a DL_TTI.request.
  translator.send_dl_tti_request(msg);

  // Assert that the downlink processor is configured.
  ASSERT_TRUE(dl_processor_pool.processor(slot).has_configure_resource_grid_method_been_called());
  // Assert that the resource grid has NOT been set to zero.
  ASSERT_EQ(rg_pool.get_getter_count(), 0);

  // Send another DL_TTI.request and check that the previous one has been sent.
  ++msg.slot;
  translator.send_dl_tti_request(msg);

  // Assert that the finish processing PDUs method of the previous slot downlink_processor has been called.
  ASSERT_TRUE(dl_processor_pool.processor(slot).has_finish_processing_pdus_method_been_called());
  ASSERT_FALSE(error_notifier_spy.has_on_error_indication_been_called());
}

TEST_F(fapi_to_phy_translator_fixture, receiving_a_dl_tti_request_from_a_slot_delayed_more_that_allowed_triggers_error)
{
  slot_point current_slot = slot;

  translator.handle_new_slot(current_slot);

  fapi::dl_tti_request msg;
  msg.slot = current_slot;

  // Increase the slots.
  for (unsigned i = 0, e = headroom_in_slots + 1; i != e; ++i) {
    current_slot += 1;
    translator.handle_new_slot(current_slot);
  }

  // Send a DL_TTI.request.
  translator.send_dl_tti_request(msg);

  // Assert the error contents
  ASSERT_TRUE(error_notifier_spy.has_on_error_indication_been_called());
  const fapi::error_indication& error_msg = error_notifier_spy.get_message();
  ASSERT_EQ(error_msg.message_id, fapi::message_type_id::dl_tti_request);
  ASSERT_EQ(error_msg.error_code, fapi::error_code_id::out_of_sync);
  ASSERT_EQ(error_msg.slot, slot);
  ASSERT_EQ(error_msg.expected_slot, current_slot);
}

TEST_F(fapi_to_phy_translator_fixture, message_received_is_sended_when_a_message_for_the_next_slot_is_received)
{
  ASSERT_FALSE(dl_processor_pool.processor(slot).has_configure_resource_grid_method_been_called());
  ASSERT_EQ(rg_pool.get_getter_count(), 0);

  fapi::dl_tti_request msg;
  msg.slot = slot;
  // Add a PDU to the message.
  msg.pdus.emplace_back();

  // Send a DL_TTI.request.
  translator.send_dl_tti_request(msg);

  // Assert that the downlink processor is configured.
  ASSERT_TRUE(dl_processor_pool.processor(slot).has_configure_resource_grid_method_been_called());
  // Assert that the resource grid has NOT been set to zero.
  ASSERT_EQ(rg_pool.get_getter_count(), 0);

  // Increase the slots.
  ASSERT_FALSE(dl_processor_pool.processor(slot).has_finish_processing_pdus_method_been_called());

  translator.handle_new_slot(slot + headroom_in_slots);

  ASSERT_FALSE(dl_processor_pool.processor(slot).has_finish_processing_pdus_method_been_called());

  ++msg.slot;
  translator.send_dl_tti_request(msg);

  // Assert that the finish processing PDUs method of the previous slot downlink_processor has been called.
  ASSERT_TRUE(dl_processor_pool.processor(slot).has_finish_processing_pdus_method_been_called());
  ASSERT_FALSE(error_notifier_spy.has_on_error_indication_been_called());
}

TEST_F(fapi_to_phy_translator_fixture,
       empty_ul_tti_does_not_generate_request_when_allow_request_on_empty_slot_is_disabled)
{
  fapi::ul_tti_request msg;
  msg.slot = slot;

  translator.handle_new_slot(slot);
  translator.send_ul_tti_request(msg);

  ASSERT_FALSE(ul_request_processor.has_uplink_been_requested());
}

TEST_F(fapi_to_phy_translator_fixture, empty_ul_tti_generates_request_when_allow_request_on_empty_slot_is_enabled)
{
  fapi_to_phy_fastpath_translator translator_allow(
      {sector_id, headroom_in_slots, true, scs, prach_cfg, carrier_cfg, {0}},
      {ocudulog::fetch_basic_logger("FAPI"),
       dl_processor_pool,
       rg_pool,
       dl_pdu_validator,
       ul_request_processor,
       pdu_repo,
       ul_pdu_validator,
       std::move(std::get<std::unique_ptr<precoding_matrix_repository>>(generate_precoding_matrix_tables(1, 0))),
       std::move(
           std::get<std::unique_ptr<uci_part2_correspondence_repository>>(generate_uci_part2_correspondence(1)))});

  fapi::ul_tti_request msg;
  msg.slot = slot;

  translator_allow.handle_new_slot(slot);
  translator_allow.send_ul_tti_request(msg);

  ASSERT_TRUE(ul_request_processor.has_uplink_been_requested());
}

// =============================================================================
// phy_to_fapi_time_event_fastpath_translator gate tests.
//
// These exercise the FAPI cell lifecycle gate added to suppress slot indication
// delivery to MAC when a cell is in the inactive state. The gate is the
// receiving end of upper_phy_operation_controller::start()/stop() that gets
// driven by FAPI P5 START.request / STOP.request from MAC. See
// docs/cns/cns-ocudu-changes.md Change 6 for context.
// =============================================================================

namespace {

/// Spy implementation of fapi::p7_slot_indication_notifier that records
/// every on_slot_indication invocation.
class p7_slot_indication_notifier_spy : public fapi::p7_slot_indication_notifier
{
  unsigned call_count = 0;

public:
  void     on_slot_indication(const fapi::slot_indication& msg) override { ++call_count; }
  unsigned get_call_count() const { return call_count; }
};

} // namespace

class phy_to_fapi_time_event_translator_gate_fixture : public fapi_to_phy_translator_fixture
{
protected:
  p7_slot_indication_notifier_spy            slot_notifier_spy;
  phy_to_fapi_time_event_fastpath_translator time_translator;

public:
  phy_to_fapi_time_event_translator_gate_fixture() : time_translator(translator)
  {
    time_translator.set_p7_slot_indication_notifier(slot_notifier_spy);
  }

  upper_phy_timing_context make_context() const
  {
    upper_phy_timing_context context;
    context.slot       = slot_point_extended{slot};
    context.time_point = {};
    return context;
  }
};

TEST_F(phy_to_fapi_time_event_translator_gate_fixture, default_state_is_active_so_slot_indication_is_delivered)
{
  ASSERT_EQ(slot_notifier_spy.get_call_count(), 0u);
  time_translator.on_tti_boundary(make_context());
  ASSERT_EQ(slot_notifier_spy.get_call_count(), 1u);
}

TEST_F(phy_to_fapi_time_event_translator_gate_fixture, set_active_false_suppresses_slot_indication_delivery)
{
  time_translator.set_active(false);
  time_translator.on_tti_boundary(make_context());
  time_translator.on_tti_boundary(make_context());
  time_translator.on_tti_boundary(make_context());
  ASSERT_EQ(slot_notifier_spy.get_call_count(), 0u);
}

TEST_F(phy_to_fapi_time_event_translator_gate_fixture, set_active_true_after_false_resumes_slot_indication_delivery)
{
  time_translator.set_active(false);
  time_translator.on_tti_boundary(make_context());
  ASSERT_EQ(slot_notifier_spy.get_call_count(), 0u);

  time_translator.set_active(true);
  time_translator.on_tti_boundary(make_context());
  time_translator.on_tti_boundary(make_context());
  ASSERT_EQ(slot_notifier_spy.get_call_count(), 2u);
}

TEST_F(phy_to_fapi_time_event_translator_gate_fixture, gate_toggling_under_load_preserves_per_call_decision)
{
  // Each on_tti_boundary observes the gate state at that moment.
  time_translator.on_tti_boundary(make_context()); // active=true → 1
  time_translator.set_active(false);
  time_translator.on_tti_boundary(make_context()); // gated     → still 1
  time_translator.set_active(true);
  time_translator.on_tti_boundary(make_context()); // active     → 2
  time_translator.set_active(false);
  time_translator.on_tti_boundary(make_context()); // gated     → still 2
  ASSERT_EQ(slot_notifier_spy.get_call_count(), 2u);
}
