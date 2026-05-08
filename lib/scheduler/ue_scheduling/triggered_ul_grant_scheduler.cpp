// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#include "triggered_ul_grant_scheduler.h"
#include "../cell/resource_grid.h"
#include "../config/ue_configuration.h"
#include "../ue_context/ue.h"
#include "ocudu/ran/logical_channel/lcid_dl_sch.h"
#include "ocudu/scheduler/result/pdsch_info.h"

using namespace ocudu;

triggered_ul_grant_scheduler::triggered_ul_grant_scheduler(ue_repository& ues_, du_cell_index_t cell_index_) :
  ues(ues_), cell_index(cell_index_)
{
}

void triggered_ul_grant_scheduler::process_dl_results(slot_point                     pdcch_slot,
                                                      slot_point                     pdsch_slot,
                                                      const cell_resource_allocator& cell_alloc)
{
  if (not pdsch_slot.valid()) {
    return;
  }

  for (const dl_msg_alloc& grant : cell_alloc[pdsch_slot].result.dl.ue_grants) {
    ue* u = ues.find(grant.context.ue_index);
    if (u == nullptr) {
      continue;
    }
    const ue_configuration* ue_cfg = u->ue_cfg_dedicated();
    if (ue_cfg == nullptr) {
      continue;
    }
    const auto lc_list = ue_cfg->logical_channels();
    if (not lc_list.has_value()) {
      continue;
    }

    for (const dl_msg_tb_info& tb : grant.tb_list) {
      for (const dl_msg_lc_info& lc : tb.lc_chs_to_sched) {
        if (not lc.lcid.is_sdu()) {
          continue;
        }
        const auto& lc_cfg = lc_list.value()[lc.lcid.to_lcid()];
        if (not lc_cfg->triggered_ul_grant.has_value()) {
          continue;
        }
        const auto& trig = *lc_cfg->triggered_ul_grant;
        queue.push_back(
            {pdcch_slot + trig.delay_slots, grant.context.ue_index, lc_cfg->lc_group, units::bytes{trig.grant_size}});
      }
    }
  }
}

void triggered_ul_grant_scheduler::run_slot(slot_point pdcch_slot)
{
  for (const pending_grant& g : queue) {
    if (g.target_pdcch_slot > pdcch_slot) {
      continue;
    }
    ue* u = ues.find(g.ue_index);
    if (u == nullptr) {
      continue;
    }
    ul_bsr_indication_message bsr{};
    bsr.cell_index = cell_index;
    bsr.ue_index   = g.ue_index;
    bsr.crnti      = u->crnti;
    bsr.type       = bsr_format::SHORT_BSR;
    bsr.reported_lcgs.push_back({g.lcg_id, g.bytes.value()});
    u->handle_bsr_indication(bsr);
  }

  clean_queue(pdcch_slot);
}

void triggered_ul_grant_scheduler::clean_queue(slot_point pdcch_slot)
{
  queue.erase(std::remove_if(queue.begin(),
                             queue.end(),
                             [&](const pending_grant& g) { return g.target_pdcch_slot <= pdcch_slot; }),
              queue.end());
}
