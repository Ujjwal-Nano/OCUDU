// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

/// \file
/// \brief Unit tests for the configured_grant_scheduler. Verifies that the scheduler correctly places periodic
/// CG PUSCH grants in the right slots, and that the scheduler output (symbols, RBs, RNTI) matches the CG
/// configuration.

#include "tests/test_doubles/scheduler/scheduler_config_helper.h"
#include "tests/test_doubles/scheduler/scheduler_result_finder.h"
#include "tests/test_doubles/utils/test_rng_seed.h"
#include "tests/unittests/scheduler/test_utils/result_test_helpers.h"
#include "tests/unittests/scheduler/test_utils/scheduler_test_simulator.h"
#include "ocudu/ran/du_types.h"
#include "ocudu/ran/logical_channel/lcid.h"
#include "ocudu/ran/sps/cg_configuration.h"
#include "ocudu/scheduler/config/cg_builder_params.h"
#include "ocudu/scheduler/resource_grid_util.h"
#include <gtest/gtest.h>

using namespace ocudu;

/// Helper struct that holds parameters for a single CG test scenario.
struct cg_test_params {
  cg_configuration::periodicity_t periodicity    = cg_configuration::periodicity_t::sl40;
  unsigned                        period_slots   = 40;
  unsigned                        slot_offset    = 0;
  unsigned                        nof_rbs        = 10;
  unsigned                        mcs            = 5;
  unsigned                        nof_harq_procs = 4;
};

/// Base class for all configured_grant_scheduler tests. Provides cell + UE setup with CG enabled and
/// helper methods to find CG grants in the scheduler output.
class configured_grant_scheduler_test : public scheduler_test_simulator, public ::testing::Test
{
protected:
  static constexpr rnti_t ue_crnti = to_rnti(0x4601);
  /// CS-RNTI assigned to the test UE. Matches the temporary value used in du_cg_res_mng.cpp.
  static constexpr rnti_t cs_rnti = to_rnti(0xe0ef);

  cg_test_params                           cg_params;
  sched_cell_configuration_request_message cell_req;

  explicit configured_grant_scheduler_test(const cg_test_params& params_ = {}) :
    scheduler_test_simulator(/*tx_rx_delay=*/4), cg_params(params_)
  {
    cell_req                     = sched_config_helper::make_default_sched_cell_configuration_request();
    cell_req.ran.init_bwp.cg_cfg = cg_builder_params{
        .periodicity        = cg_params.periodicity,
        .slot_offset        = cg_params.slot_offset,
        .nof_rbs            = cg_params.nof_rbs,
        .mcs                = cg_params.mcs,
        .nof_harq_processes = cg_params.nof_harq_procs,
    };
    add_cell(cell_req);
  }

  /// Adds the test UE to the scheduler with CG configuration.
  void add_cg_ue(du_ue_index_t ue_idx = to_du_ue_index(0))
  {
    auto ue_req     = sched_config_helper::create_default_sched_ue_creation_request(cell_req.ran);
    ue_req.ue_index = ue_idx;
    ue_req.crnti    = ue_crnti;
    // The UE config was generated from the cell RAN config, so cg_cfg is already set.
    // We only need to patch the CS-RNTI.
    ASSERT_TRUE(ue_req.cfg.cells.has_value() and not ue_req.cfg.cells->empty());
    auto& ue_cell = ue_req.cfg.cells->front();
    ASSERT_TRUE(ue_cell.serv_cell_cfg.ul_config.has_value());
    ASSERT_TRUE(ue_cell.serv_cell_cfg.ul_config->init_ul_bwp.cg_cfg.has_value());
    ue_cell.serv_cell_cfg.ul_config->init_ul_bwp.cg_cfg->cs_rnti = cs_rnti;
    add_ue(ue_req);
  }

  /// Advances the scheduler until a CG PUSCH for cs_rnti is seen in the output, or \c max_slots is exhausted.
  const ul_sched_info* run_until_next_cg_pusch(unsigned max_slots = 0)
  {
    if (max_slots == 0) {
      // The CG PUSCH is booked max_ul_slot_alloc_delay slots ahead, so the first grant appears in last_sched_result()
      // max_ul_slot_alloc_delay + (period - 1) steps after UE creation. Add extra margin for safety.
      const unsigned ul_delay = get_max_slot_ul_alloc_delay(/*ntn_cs_koffset=*/0);
      max_slots               = ul_delay + cg_params.period_slots + 20;
    }
    for (unsigned i = 0; i < max_slots; ++i) {
      run_slot();
      const ul_sched_info* grant = find_ue_pusch(cs_rnti, *last_sched_result());
      if (grant != nullptr) {
        return grant;
      }
    }
    return nullptr;
  }
};

/// Test: after adding a CG UE, CG PUSCH grants appear and repeat with the configured period.
TEST_F(configured_grant_scheduler_test, cg_grants_are_periodic)
{
  add_cg_ue();

  // Find the first CG grant.
  const ul_sched_info* first_grant = run_until_next_cg_pusch();
  ASSERT_NE(first_grant, nullptr) << "No CG PUSCH found within " << cg_params.period_slots + 20 << " slots";
  const slot_point first_slot = last_result_slot();

  // Advance exactly cg_period_slots more and expect the next CG grant.
  for (unsigned i = 0; i < cg_params.period_slots; ++i) {
    run_slot();
  }
  EXPECT_NE(find_ue_pusch(cs_rnti, *last_sched_result()), nullptr)
      << "Expected second CG PUSCH " << cg_params.period_slots << " slots after first";
  EXPECT_EQ(last_result_slot() - first_slot, cg_params.period_slots);

  // Advance one more period and verify a third grant.
  for (unsigned i = 0; i < cg_params.period_slots; ++i) {
    run_slot();
  }
  EXPECT_NE(find_ue_pusch(cs_rnti, *last_sched_result()), nullptr)
      << "Expected third CG PUSCH " << 2 * cg_params.period_slots << " slots after first";
}

/// Test: the CG PUSCH output uses the CS-RNTI (not the C-RNTI).
TEST_F(configured_grant_scheduler_test, cg_pusch_rnti_is_cs_rnti)
{
  add_cg_ue();
  const ul_sched_info* grant = run_until_next_cg_pusch();
  ASSERT_NE(grant, nullptr);
  EXPECT_EQ(grant->pusch_cfg.rnti, cs_rnti);
  // Verify the grant is NOT indexed by the C-RNTI.
  EXPECT_NE(grant->pusch_cfg.rnti, ue_crnti);
}

/// Test: the OFDM symbols in the CG PUSCH match the PUSCH time-domain allocation entry used by the CG config.
TEST_F(configured_grant_scheduler_test, cg_pusch_symbols_match_td_alloc)
{
  add_cg_ue();
  const ul_sched_info* grant = run_until_next_cg_pusch();
  ASSERT_NE(grant, nullptr);

  // CG uses time_domain_allocation = 0 (entry 0 of the cell's common PUSCH TD alloc list).
  const auto& pusch_td_list = cell_cfg().params.ul_cfg_common.init_ul_bwp.pusch_cfg_common->pusch_td_alloc_list;
  ASSERT_FALSE(pusch_td_list.empty()) << "Common PUSCH TD alloc list is empty";
  EXPECT_EQ(grant->pusch_cfg.symbols, pusch_td_list[0].symbols);
}

/// Test: the VRBs in the CG PUSCH match the configured VRB allocation (start=10, length=nof_rbs).
TEST_F(configured_grant_scheduler_test, cg_pusch_rbs_match_cg_config)
{
  add_cg_ue();
  const ul_sched_info* grant = run_until_next_cg_pusch();
  ASSERT_NE(grant, nullptr);

  // The CG config builder sets vrbs = {10, 10 + nof_rbs}.
  const vrb_interval expected_vrbs{10, 10U + cg_params.nof_rbs};
  ASSERT_TRUE(grant->pusch_cfg.rbs.is_type1()) << "Expected type-1 (contiguous) VRB allocation";
  EXPECT_EQ(grant->pusch_cfg.rbs.type1(), expected_vrbs);
}

/// Test: the decision context fields are filled correctly for CG PUSCH.
TEST_F(configured_grant_scheduler_test, cg_pusch_context_fields_are_correct)
{
  add_cg_ue();
  const ul_sched_info* grant = run_until_next_cg_pusch();
  ASSERT_NE(grant, nullptr);

  // CG always schedules new transmissions (no retransmissions).
  EXPECT_EQ(grant->context.nof_retxs, 0U);
  // The UE index should be valid.
  EXPECT_NE(grant->context.ue_index, INVALID_DU_UE_INDEX);
}

/// Test: after removing a CG UE, no further CG PUSCH grants are produced.
TEST_F(configured_grant_scheduler_test, after_ue_removal_no_more_cg_grants)
{
  add_cg_ue();

  // Confirm at least one grant appears before removal.
  ASSERT_NE(run_until_next_cg_pusch(), nullptr) << "Pre-condition: expected at least one CG grant";

  // Remove the UE.
  rem_ue(to_du_ue_index(0));

  // Run 2 periods and verify no more CG grants.
  for (unsigned i = 0; i < 2 * cg_params.period_slots + 10; ++i) {
    run_slot();
    EXPECT_EQ(find_ue_pusch(cs_rnti, *last_sched_result()), nullptr) << "Unexpected CG PUSCH after UE removal";
  }
}

/// Test: a UE added to a cell that has CG configured, but whose UE config does NOT include CG (cs_rnti = 0),
/// does not produce CG grants (the scheduler handles an absent/invalid cs_rnti gracefully).
TEST_F(configured_grant_scheduler_test, ue_with_no_cg_config_produces_no_cg_grants)
{
  // Add a UE but intentionally leave cs_rnti unset (zero / invalid).
  auto ue_req     = sched_config_helper::create_default_sched_ue_creation_request(cell_req.ran);
  ue_req.ue_index = to_du_ue_index(0);
  ue_req.crnti    = ue_crnti;
  // Do NOT set cs_rnti — leave cg_cfg->cs_rnti at its default (zero / INVALID_RNTI).
  // The grant wheel uses crnti for tracking, and any resulting PUSCH will have rnti = 0.
  add_ue(ue_req);

  // Run for 2 periods and verify no PUSCH with cs_rnti = 0xe0ef appears.
  for (unsigned i = 0; i < 2 * cg_params.period_slots + 10; ++i) {
    run_slot();
    EXPECT_EQ(find_ue_pusch(cs_rnti, *last_sched_result()), nullptr);
  }
}

/// Parameterised fixture for checking that the periodicity is honoured for different CG period values.
struct cg_period_test_params {
  cg_configuration::periodicity_t periodicity;
  unsigned                        period_slots;
};

class cg_period_test : public configured_grant_scheduler_test,
                       public ::testing::WithParamInterface<cg_period_test_params>
{
protected:
  cg_period_test() :
    configured_grant_scheduler_test(cg_test_params{
        .periodicity    = GetParam().periodicity,
        .period_slots   = GetParam().period_slots,
        .slot_offset    = 0,
        .nof_rbs        = 10,
        .mcs            = 5,
        .nof_harq_procs = 8,
    })
  {
  }
};

TEST_P(cg_period_test, period_is_honoured)
{
  add_cg_ue();

  const ul_sched_info* first_grant = run_until_next_cg_pusch();
  ASSERT_NE(first_grant, nullptr) << "No CG grant found for period=" << cg_params.period_slots;
  const slot_point first_slot = last_result_slot();

  for (unsigned i = 0; i < cg_params.period_slots; ++i) {
    run_slot();
  }
  EXPECT_NE(find_ue_pusch(cs_rnti, *last_sched_result()), nullptr);
  EXPECT_EQ(last_result_slot() - first_slot, cg_params.period_slots);
}

INSTANTIATE_TEST_SUITE_P(cg_periods,
                         cg_period_test,
                         ::testing::Values(cg_period_test_params{cg_configuration::periodicity_t::sl10, 10},
                                           cg_period_test_params{cg_configuration::periodicity_t::sl20, 20},
                                           cg_period_test_params{cg_configuration::periodicity_t::sl40, 40},
                                           cg_period_test_params{cg_configuration::periodicity_t::sl80, 80}));

/// Test: when a PUCCH HARQ-ACK falls in the same slot as a CG PUSCH, the UCI is moved to the PUSCH and the PUCCH
/// is removed.
TEST_F(configured_grant_scheduler_test, cg_pusch_absorbs_pucch_harq_ack)
{
  auto_uci = true;
  auto_crc = true;
  add_cg_ue();

  bool found = false;
  for (unsigned i = 0; i != static_cast<unsigned>(cg_params.periodicity) * 5; ++i) {
    // Keep DL buffer non-empty so the scheduler generates PDSCHs → PUCCH HARQ-ACK reports.
    push_dl_buffer_state(dl_buffer_state_indication_message{to_du_ue_index(0), LCID_SRB1, 10000});
    run_slot();

    const sched_result&  res      = *last_sched_result();
    const ul_sched_info* cg_pusch = find_ue_pusch_with_harq_ack(cs_rnti, res);
    if (cg_pusch == nullptr) {
      continue;
    }
    found = true;
    // The PUSCH UCI should carry at least one HARQ-ACK bit.
    EXPECT_GT(cg_pusch->uci->harq->harq_ack_nof_bits, 0U);
    // The PUCCH with HARQ-ACK for this UE must have been removed after the mux.
    EXPECT_EQ(find_ue_pucch_with_harq_ack(ue_crnti, res.ul.pucchs), nullptr)
        << "PUCCH HARQ-ACK should be absent after UCI mux onto CG PUSCH";
    break;
  }
  EXPECT_TRUE(found) << "No CG PUSCH with muxed HARQ-ACK UCI was observed within 200 slots";
}

/// Fixture for the CSI mux test. The default CSI report slot offset is 9 (from du_csi_params), so the CG
/// slot_offset is set to 9 as well. This guarantees that every other CG PUSCH slot (period=40) coincides
/// with a CSI PUCCH slot (period=80, offset=9): overlaps at slots 9, 89, 169, ...
class cg_csi_mux_test : public configured_grant_scheduler_test
{
protected:
  static constexpr unsigned csi_report_slot_offset = 9; ///< Matches du_csi_params default.

  cg_csi_mux_test() : configured_grant_scheduler_test(cg_test_params{.slot_offset = csi_report_slot_offset}) {}
};

/// Test: when a PUCCH CSI report falls in the same slot as a CG PUSCH, the CSI is moved to the PUSCH and the
/// PUCCH is removed.
TEST_F(cg_csi_mux_test, cg_pusch_absorbs_pucch_csi)
{
  auto_uci = true;
  auto_crc = true;
  add_cg_ue();

  // With CG offset=9 and CSI period=80/offset=9, overlaps occur every 80 slots: at slots 9, 89, 169, ...
  // Run for 3 CSI periods to ensure at least two overlap opportunities.
  bool found = false;
  for (unsigned i = 0; i != 300; ++i) {
    push_dl_buffer_state(dl_buffer_state_indication_message{to_du_ue_index(0), LCID_SRB1, 10000});
    run_slot();

    const sched_result&  res      = *last_sched_result();
    const ul_sched_info* cg_pusch = find_ue_pusch(cs_rnti, res);
    if (cg_pusch == nullptr or not cg_pusch->uci.has_value()) {
      continue;
    }
    if (not cg_pusch->uci->csi.has_value() or cg_pusch->uci->csi->csi_part1_nof_bits == 0) {
      continue;
    }
    found = true;
    // The PUSCH UCI should carry at least one CSI Part 1 bit.
    EXPECT_GT(cg_pusch->uci->csi->csi_part1_nof_bits, 0U);
    // The PUCCH carrying CSI for this UE must have been removed after the mux.
    EXPECT_EQ(find_ue_pucch_with_csi(ue_crnti, res.ul.pucchs), nullptr)
        << "PUCCH CSI should be absent after UCI mux onto CG PUSCH";
    break;
  }
  EXPECT_TRUE(found) << "No CG PUSCH with muxed CSI was observed within 300 slots";
}
