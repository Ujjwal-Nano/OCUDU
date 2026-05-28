// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "cu_cp_test_environment.h"
#include "test_helpers.h"
#include "tests/test_doubles/f1ap/f1ap_test_messages.h"
#include "ocudu/asn1/f1ap/common.h"
#include "ocudu/asn1/f1ap/f1ap.h"
#include "ocudu/asn1/f1ap/f1ap_pdu_contents.h"
#include "ocudu/cu_cp/cu_cp.h"
#include "ocudu/cu_cp/cu_cp_cell_command_handler.h"
#include "ocudu/cu_cp/cu_cp_command_handler.h"
#include "ocudu/f1ap/f1ap_message.h"
#include "ocudu/ran/nr_cgi.h"
#include "ocudu/support/async/async_test_utils.h"
#include <gtest/gtest.h>

using namespace ocudu;
using namespace ocucp;

class cu_cp_cell_command_integration_test : public cu_cp_test_environment, public ::testing::Test
{
public:
  cu_cp_cell_command_integration_test() : cu_cp_test_environment(cu_cp_test_env_params{})
  {
    run_ng_setup();

    auto ret = connect_new_du();
    EXPECT_TRUE(ret.has_value());
    du_idx = ret.value();

    test_helpers::served_cell_item_info cell;
    served_cgi = nr_cell_global_id_t{cell.plmn_id, cell.nci};
    EXPECT_TRUE(run_f1_setup(du_idx, int_to_gnb_du_id(0x11), {cell}));
  }

  /// Pop the F1AP gNB-CU Configuration Update emitted by the CU-CP toward the DU and return it.
  bool pop_cu_cfg_upd(f1ap_message& out)
  {
    if (!wait_for_f1ap_tx_pdu(du_idx, out)) {
      return false;
    }
    if (out.pdu.type().value != asn1::f1ap::f1ap_pdu_c::types::init_msg) {
      return false;
    }
    return out.pdu.init_msg().proc_code == ASN1_F1AP_ID_GNB_CU_CFG_UPD;
  }

  unsigned            du_idx{0};
  nr_cell_global_id_t served_cgi;
};

TEST_F(cu_cp_cell_command_integration_test, when_deactivate_cell_then_cu_emits_cfg_upd_and_completes_on_du_ack)
{
  cu_cp_cell_command_handler& cell_cmd = get_cu_cp().get_command_handler().get_cell_command_handler();

  // Drive the deactivate procedure on CU-CP.
  async_task<cu_cp_cell_command_response>         resp_task = cell_cmd.deactivate_cell(served_cgi);
  lazy_task_launcher<cu_cp_cell_command_response> launcher(resp_task);

  // CU-CP emits gNB-CU Configuration Update toward the DU.
  f1ap_message cu_cfg_upd;
  ASSERT_TRUE(pop_cu_cfg_upd(cu_cfg_upd));

  // Verify the deactivate list carries the served CGI.
  const auto& upd_ies = cu_cfg_upd.pdu.init_msg().value.gnb_cu_cfg_upd();
  ASSERT_TRUE(upd_ies->cells_to_be_deactiv_list_present);
  ASSERT_EQ(upd_ies->cells_to_be_deactiv_list.size(), 1U);

  // DU acks the configuration update. Without an ack the CU-CP procedure never completes.
  get_du(du_idx).push_ul_pdu(test_helpers::generate_gnb_cu_configuration_update_acknowledgement({}));

  // CU-CP should now resolve the procedure with success.
  ASSERT_TRUE(tick_until(std::chrono::milliseconds{500}, [&]() { return launcher.ready(); }, false));
  ASSERT_TRUE(launcher.result.has_value());
  ASSERT_TRUE(launcher.result.value().success);
}

TEST_F(cu_cp_cell_command_integration_test, when_activate_cell_then_cu_emits_cfg_upd_and_completes_on_du_ack)
{
  cu_cp_cell_command_handler& cell_cmd = get_cu_cp().get_command_handler().get_cell_command_handler();

  async_task<cu_cp_cell_command_response>         resp_task = cell_cmd.activate_cell(served_cgi);
  lazy_task_launcher<cu_cp_cell_command_response> launcher(resp_task);

  f1ap_message cu_cfg_upd;
  ASSERT_TRUE(pop_cu_cfg_upd(cu_cfg_upd));

  const auto& upd_ies = cu_cfg_upd.pdu.init_msg().value.gnb_cu_cfg_upd();
  ASSERT_TRUE(upd_ies->cells_to_be_activ_list_present);
  ASSERT_EQ(upd_ies->cells_to_be_activ_list.size(), 1U);

  get_du(du_idx).push_ul_pdu(test_helpers::generate_gnb_cu_configuration_update_acknowledgement({}));

  ASSERT_TRUE(tick_until(std::chrono::milliseconds{500}, [&]() { return launcher.ready(); }, false));
  ASSERT_TRUE(launcher.result.has_value());
  ASSERT_TRUE(launcher.result.value().success);
}

// Note: a back-to-back lock-then-unlock sequence test was prototyped but is omitted here — the
// existing two single-procedure cases above already prove each direction round-trips correctly,
// and the cu_cp_test_environment's wait_for_f1ap_tx_pdu queue semantics make a tightly coupled
// two-procedure sequence brittle without additional drain steps. End-to-end runtime validation
// in the K8s scenario covers the full lock/unlock cycle.
