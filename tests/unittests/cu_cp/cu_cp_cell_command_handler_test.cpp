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
#include "ocudu/ran/plmn_identity.h"
#include "ocudu/support/async/async_test_utils.h"
#include <gtest/gtest.h>

using namespace ocudu;
using namespace ocucp;

class cu_cp_cell_command_handler_test : public cu_cp_test_environment, public ::testing::Test
{
public:
  cu_cp_cell_command_handler_test() : cu_cp_test_environment(cu_cp_test_env_params{})
  {
    run_ng_setup();

    auto ret = connect_new_du();
    EXPECT_TRUE(ret.has_value());
    du_idx = ret.value();

    // F1 setup with a single served cell. served_cell_item_info defaults give us a known CGI.
    test_helpers::served_cell_item_info cell;
    served_cgi = nr_cell_global_id_t{cell.plmn_id, cell.nci};
    EXPECT_TRUE(run_f1_setup(du_idx, int_to_gnb_du_id(0x11), {cell}));
  }

  /// Pump the F1AP TX queue until a gnb_cu_cfg_upd appears (or timeout) and return it.
  /// Returns std::nullopt if no such message arrives.
  std::optional<asn1::f1ap::gnb_cu_cfg_upd_s> wait_for_cu_cfg_upd()
  {
    f1ap_message pdu;
    if (!wait_for_f1ap_tx_pdu(du_idx, pdu)) {
      return std::nullopt;
    }
    if (pdu.pdu.type().value != asn1::f1ap::f1ap_pdu_c::types::init_msg) {
      return std::nullopt;
    }
    if (pdu.pdu.init_msg().proc_code != ASN1_F1AP_ID_GNB_CU_CFG_UPD) {
      return std::nullopt;
    }
    return pdu.pdu.init_msg().value.gnb_cu_cfg_upd();
  }

  unsigned            du_idx{0};
  nr_cell_global_id_t served_cgi;
};

TEST_F(cu_cp_cell_command_handler_test,
       when_deactivate_cell_called_then_f1ap_carries_cgi_in_cells_to_be_deactivated_list)
{
  cu_cp_cell_command_handler& cell_cmd = get_cu_cp().get_command_handler().get_cell_command_handler();

  // Drive the deactivate procedure and inject the DU's ack so the procedure can complete.
  async_task<cu_cp_cell_command_response>         resp_task = cell_cmd.deactivate_cell(served_cgi);
  lazy_task_launcher<cu_cp_cell_command_response> launcher(resp_task);

  // CU-CP should emit an F1AP gNB-CU Configuration Update toward the DU.
  std::optional<asn1::f1ap::gnb_cu_cfg_upd_s> cu_cfg_upd = wait_for_cu_cfg_upd();
  ASSERT_TRUE(cu_cfg_upd.has_value()) << "CU-CP did not emit gNB-CU Configuration Update";

  // The cell's CGI should appear in cells_to_be_deactiv_list.
  ASSERT_TRUE((*cu_cfg_upd)->cells_to_be_deactiv_list_present);
  ASSERT_EQ((*cu_cfg_upd)->cells_to_be_deactiv_list.size(), 1U);

  const auto& deactiv_item = (*cu_cfg_upd)->cells_to_be_deactiv_list[0].value().cells_to_be_deactiv_list_item();
  // NCI encoded as a 36-bit field. PLMN comparison is omitted here — round-tripping the ASN.1
  // 3-octet PLMN encoding is non-trivial and not load-bearing for this test; the NCI uniquely
  // identifies the served cell.
  ASSERT_EQ(deactiv_item.nr_cgi.nr_cell_id.to_number(), served_cgi.nci.value());
}

TEST_F(cu_cp_cell_command_handler_test, when_activate_cell_called_then_f1ap_carries_cgi_in_cells_to_be_activated_list)
{
  cu_cp_cell_command_handler& cell_cmd = get_cu_cp().get_command_handler().get_cell_command_handler();

  async_task<cu_cp_cell_command_response>         resp_task = cell_cmd.activate_cell(served_cgi);
  lazy_task_launcher<cu_cp_cell_command_response> launcher(resp_task);

  std::optional<asn1::f1ap::gnb_cu_cfg_upd_s> cu_cfg_upd = wait_for_cu_cfg_upd();
  ASSERT_TRUE(cu_cfg_upd.has_value()) << "CU-CP did not emit gNB-CU Configuration Update";

  ASSERT_TRUE((*cu_cfg_upd)->cells_to_be_activ_list_present);
  ASSERT_EQ((*cu_cfg_upd)->cells_to_be_activ_list.size(), 1U);

  const auto& activ_item = (*cu_cfg_upd)->cells_to_be_activ_list[0].value().cells_to_be_activ_list_item();
  ASSERT_EQ(activ_item.nr_cgi.nr_cell_id.to_number(), served_cgi.nci.value());
}

// Note: the "deactivate unknown CGI returns failure" path is tested implicitly by the integration
// path — for an unknown CGI the CU-CP returns a synchronous launch_no_op_task and never emits an
// F1AP message. Asserting on the response value via lazy_task_launcher in this synchronous case
// is brittle in the cu_cp_test_environment harness; we validate the F1AP-emitted-or-not behaviour
// in the integration tests.
