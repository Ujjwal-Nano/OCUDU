// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI

#pragma once

#include "scheduler_policy.h"
#include "swap/swap_allocator.h" // your framework-agnostic engine (drop it in policy/swap/)
#include "ocudu/ran/rnti.h"
#include "ocudu/scheduler/config/scheduler_expert_config.h"
#include <array>
#include <vector>
#include <fstream>
#include <limits>

namespace ocudu {

/// \brief Periodic worst-first ("min_csi") swap scheduler policy.
///
/// This class HOSTS the framework-agnostic swap_allocator:
///   - maintains a stable du_ue_index_t <-> swap user-index map (add_ue / rem_ue);
///   - once per SRS period, builds the per-(user, RU) CSI grid from the SRS-derived
///     csi_grid_registry and runs swap_allocator::step();
///   - exposes the resulting per-UE RU ownership via owned_rus().
///
/// IMPORTANT: the scheduler_policy interface only sets UE *priorities*; it does NOT
/// assign RBs. The swap's per-RU ownership is therefore ENFORCED in the grant
/// allocator (intra_slice_scheduler), which must call owned_rus() to restrict each
/// UE to the RBs of its owned RUs. This class decides the assignment; on its own it
/// does not change which RBs a UE receives.
class scheduler_swap : public scheduler_policy
{
public:
  explicit scheduler_swap(const swap_scheduler_config& cfg_);

  void add_ue(du_ue_index_t ue_index) override;
  void rem_ue(du_ue_index_t ue_index) override;
  scheduler_swap* as_swap() override { return this; }
  void compute_ue_dl_priorities(slot_point               pdcch_slot,
                                slot_point               pdsch_slot,
                                span<ue_newtx_candidate> ue_candidates) override;

  void compute_ue_ul_priorities(slot_point               pdcch_slot,
                                slot_point               pusch_slot,
                                span<ue_newtx_candidate> ue_candidates) override;

  void save_dl_newtx_grants(span<const dl_msg_alloc> /*dl_grants*/) override {}
  void save_ul_newtx_grants(span<const ul_sched_info> /*ul_grants*/) override {}

  /// RUs owned by this UE for the current period (empty if unmapped / not yet assigned).
  /// The grant allocator calls this to restrict the UE's RB allocation.
  std::vector<unsigned> owned_rus(du_ue_index_t ue_index) const;

  /// RU r spans RBs [ru_first_rb(r), ru_first_rb(r) + rbs_per_ru()).
  unsigned ru_first_rb(unsigned ru) const { return ru * cfg.rbs_per_ru; }
  unsigned rbs_per_ru() const { return cfg.rbs_per_ru; }

private:
  /// Run swap_allocator::step() if a full SRS period has elapsed; also refresh RNTIs.
  void maybe_run_swap(slot_point sl, span<ue_newtx_candidate> candidates);

  /// Build the [user][RU] CSI grid from the registry for the currently-mapped users.
  swap_sched::csi_grid build_csi_grid() const;

  unsigned num_rus() const { return cfg.num_users * cfg.rus_per_user + cfg.num_redun; }

  swap_scheduler_config      cfg;
  swap_sched::swap_allocator alloc;

  // Stable mapping between DU UE index and swap user index.
  std::array<int, MAX_NOF_DU_UES> ue_to_user;   // -1 == unmapped
  std::vector<du_ue_index_t>      user_to_ue;    // user idx -> du ue idx
  std::vector<rnti_t>             user_to_rnti;  // user idx -> rnti (for registry lookup)
  std::vector<slot_point> user_last_seen; // last slot this user's UE appeared as a candidate

  slot_point last_swap_slot;

  std::ofstream metrics_log;

};

} // namespace ocudu
