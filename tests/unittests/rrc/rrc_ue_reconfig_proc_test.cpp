// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "rrc_ue_test_helpers.h"
#include "rrc_ue_test_messages.h"
#include "ocudu/asn1/rrc_nr/dl_dcch_msg_ies.h"
#include "ocudu/asn1/rrc_nr/rrc_nr.h"
#include "ocudu/rrc/meas_types.h"
#include "ocudu/support/async/async_test_utils.h"
#include <gtest/gtest.h>

using namespace ocudu;
using namespace ocucp;

/// Fixture class RRC Reconfiguration tests preparation (to bring UE in RRC connected state)
class rrc_ue_reconfig : public rrc_ue_test_helper, public ::testing::Test
{
protected:
  static void SetUpTestSuite() { ocudulog::init(); }

  void SetUp() override
  {
    init();

    ocudulog::basic_logger& rrc_logger = ocudulog::fetch_basic_logger("RRC", false);
    rrc_logger.set_level(ocudulog::basic_levels::debug);
    rrc_logger.set_hex_dump_max_size(32);

    receive_setup_request();

    // check if the RRC setup message was generated
    ASSERT_EQ(get_srb0_pdu_type(), asn1::rrc_nr::dl_ccch_msg_type_c::c1_c_::types::rrc_setup);

    // check if SRB1 was created
    check_srb1_exists();

    receive_setup_complete();
  }

  void TearDown() override
  {
    // flush logger after each test
    ocudulog::flush();
  }
};

/// Test the RRC reconfig with connected AMF
TEST_F(rrc_ue_reconfig, when_reconfig_complete_received_proc_successful)
{
  // Prepare args
  rrc_reconfiguration_procedure_request args = generate_rrc_reconfiguration_procedure_request();

  // Trigger Reconfig
  async_task<bool>         t = get_rrc_ue_control_message_handler()->handle_rrc_reconfiguration_request(args);
  lazy_task_launcher<bool> t_launcher(t);

  ASSERT_FALSE(t.ready());

  check_rrc_reconfig_pdu();

  // Receive Reconfig complete
  receive_reconfig_complete();

  ASSERT_TRUE(t.ready());
}

// Fixture for handover-related RRC UE tests (no need to bring UE to RRC_CONNECTED).
class rrc_ue_handover : public rrc_ue_test_helper, public ::testing::Test
{
protected:
  static void SetUpTestSuite() { ocudulog::init(); }
  void        SetUp() override { init(); }
};

// Verify that source measConfig encoded in AS-Config inside HandoverPreparationInformation
// is stored and forwarded as current_meas_config to on_measurement_config_request, so that
// the target can generate explicit remove lists instead of relying on fullConfig.
TEST_F(rrc_ue_handover, as_config_meas_ids_stored_and_used_as_remove_list)
{
  // Build HandoverPreparationInfo with AS-Config carrying one measObject and one measId.
  asn1::rrc_nr::ho_prep_info_s ho_prep;
  auto&                        ies = ho_prep.crit_exts.set_c1().set_ho_prep_info();

  asn1::rrc_nr::rrc_recfg_s rrc_recfg;
  auto&                     recfg_ies = rrc_recfg.crit_exts.set_rrc_recfg();
  recfg_ies.meas_cfg_present          = true;
  // Add a minimal measObject (meas_obj choice must be initialized to a valid type).
  recfg_ies.meas_cfg.meas_obj_to_add_mod_list.resize(1);
  auto& meas_obj       = recfg_ies.meas_cfg.meas_obj_to_add_mod_list[0];
  meas_obj.meas_obj_id = 3;
  meas_obj.meas_obj.set_meas_obj_nr();
  // Add a minimal measId (no nested choices — safe to pack as-is).
  recfg_ies.meas_cfg.meas_id_to_add_mod_list.resize(1);
  recfg_ies.meas_cfg.meas_id_to_add_mod_list[0].meas_id = 5;

  byte_buffer   recfg_buf;
  asn1::bit_ref bref_recfg{recfg_buf};
  ASSERT_EQ(rrc_recfg.pack(bref_recfg), asn1::OCUDUASN_SUCCESS);
  ies.source_cfg_present   = true;
  ies.source_cfg.rrc_recfg = std::move(recfg_buf);

  byte_buffer   ho_prep_buf;
  asn1::bit_ref bref_ho{ho_prep_buf};
  ASSERT_EQ(ho_prep.pack(bref_ho), asn1::OCUDUASN_SUCCESS);

  // Feed HandoverPreparationInfo to the RRC UE — it should store the source meas IDs.
  ASSERT_TRUE(rrc_ue->handle_rrc_handover_preparation_info(std::move(ho_prep_buf)));

  // Trigger generate_meas_config with no explicit arg — should fall back to stored source meas cfg.
  rrc_ue->generate_meas_config();

  // The stored source meas cfg must have been forwarded to on_measurement_config_request
  // so the cell_meas_manager can build remove lists from it.
  ASSERT_TRUE(rrc_ue_cu_cp_notifier.last_current_meas_config.has_value());
  const auto& fwd = rrc_ue_cu_cp_notifier.last_current_meas_config.value();
  ASSERT_EQ(fwd.meas_obj_to_add_mod_list.size(), 1u);
  EXPECT_EQ(meas_obj_id_to_uint(fwd.meas_obj_to_add_mod_list[0].meas_obj_id), 3u);
  ASSERT_EQ(fwd.meas_id_to_add_mod_list.size(), 1u);
  EXPECT_EQ(meas_id_to_uint(fwd.meas_id_to_add_mod_list[0].meas_id), 5u);
}
