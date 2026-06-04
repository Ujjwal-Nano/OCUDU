// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI

#include "scheduler_swap.h"
#include "../slicing/slice_ue_repository.h" // for slice_ue accessors  // VERIFY path
#include "ocudu/support/csi_grid_registry.h"

using namespace ocudu;

namespace {

swap_sched::swap_params to_params(const swap_scheduler_config& c)
{
  swap_sched::swap_params p;
  p.num_users         = c.num_users;
  p.rus_per_user      = c.rus_per_user;
  p.num_redun         = c.num_redun;
  p.epsilon           = c.epsilon;
  p.allocation_period = c.allocation_period;
  p.allocation_delay  = 0;
  return p;
}

} // namespace

scheduler_swap::scheduler_swap(const swap_scheduler_config& cfg_) : cfg(cfg_), alloc(to_params(cfg_))
{
  ue_to_user.fill(-1);
  user_to_ue.assign(cfg.num_users, INVALID_DU_UE_INDEX);          // VERIFY sentinel name
  user_to_rnti.assign(cfg.num_users, rnti_t::INVALID_RNTI);        // VERIFY sentinel name
  alloc.init_assignment(/*seed*/ 1);
}

void scheduler_swap::add_ue(du_ue_index_t ue_index)
{
  if (ue_to_user[ue_index] >= 0) {
    return; // already mapped
  }
  for (unsigned u = 0; u != user_to_ue.size(); ++u) {
    if (user_to_ue[u] == INVALID_DU_UE_INDEX) {
      user_to_ue[u]        = ue_index;
      ue_to_user[ue_index] = static_cast<int>(u);
      return;
    }
  }
  // No free user slot: more UEs attached than configured num_users.
  // For a fixed testbed set cfg.num_users to your UE count. (Consider logging here.)
}

void scheduler_swap::rem_ue(du_ue_index_t ue_index)
{
  int u = ue_to_user[ue_index];
  if (u < 0) {
    return;
  }
  user_to_ue[u]        = INVALID_DU_UE_INDEX;
  user_to_rnti[u]      = rnti_t::INVALID_RNTI;
  ue_to_user[ue_index] = -1;
}

swap_sched::csi_grid scheduler_swap::build_csi_grid() const
{
  const unsigned U = cfg.num_users;
  const unsigned R = num_rus();

  // Strictly-positive floor: a UE that has not sounded yet must not be a hard zero,
  // since the swap's gain math divides by CSI values.
  swap_sched::csi_grid csi(U, std::vector<double>(R, 1e-9));

  for (unsigned u = 0; u != U; ++u) {
    if (user_to_rnti[u] == rnti_t::INVALID_RNTI) {
      continue; // this user slot is empty / no RNTI learned yet
    }
    auto g = csi_grid_registry::instance().get(user_to_rnti[u]); // [rx_port][ru]
    for (const auto& per_port : g) {                             // collapse rx ports -> scalar power
      for (unsigned r = 0; r != R && r < per_port.size(); ++r) {
        csi[u][r] += per_port[r];
      }
    }
  }
  return csi;
}

void scheduler_swap::maybe_run_swap(slot_point sl, span<ue_newtx_candidate> candidates)
{
  // Learn / refresh each candidate UE's RNTI so build_csi_grid() can look it up.
  for (const ue_newtx_candidate& c : candidates) {
    du_ue_index_t idx = c.ue->ue_index();        // VERIFY accessor
    int           u   = ue_to_user[idx];
    if (u >= 0) {
      user_to_rnti[u] = c.ue->crnti();           // VERIFY accessor (slice_ue rnti)
    }
  }

  // Gate to the SRS reporting period (run once per cfg.swap_period_slots).
  if (last_swap_slot.valid() && (sl - last_swap_slot) < static_cast<int>(cfg.swap_period_slots)) {
    return;
  }
  last_swap_slot = sl;

  alloc.step(build_csi_grid());
}

void scheduler_swap::compute_ue_dl_priorities(slot_point               pdcch_slot,
                                              slot_point               pdsch_slot,
                                              span<ue_newtx_candidate> ue_candidates)
{
  // Re-run the swap on the SRS-period boundary (uses SRS-derived CSI; DL via TDD reciprocity).
  maybe_run_swap(pdsch_slot, ue_candidates);

  // Priority here is secondary: the swap's real effect comes from RB restriction in the
  // grant allocator (via owned_rus()). Order by pending data so a served UE drains first.
  for (ue_newtx_candidate& c : ue_candidates) {
    c.priority = static_cast<ue_sched_priority>(c.pending_bytes.value());
  }
}

void scheduler_swap::compute_ue_ul_priorities(slot_point /*pdcch_slot*/,
                                              slot_point /*pusch_slot*/,
                                              span<ue_newtx_candidate> ue_candidates)
{
  // The swap is applied to DL here (reciprocity). UL kept neutral.
  for (ue_newtx_candidate& c : ue_candidates) {
    c.priority = static_cast<ue_sched_priority>(c.pending_bytes.value());
  }
}

std::vector<unsigned> scheduler_swap::owned_rus(du_ue_index_t ue_index) const
{
  int u = ue_to_user[ue_index];
  if (u < 0) {
    return {};
  }
  return alloc.assignment()[u];
}
