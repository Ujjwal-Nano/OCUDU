// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI

#include "scheduler_swap.h"
#include "../slicing/slice_ue_repository.h" // for slice_ue accessors  // VERIFY path
#include "ocudu/support/csi_grid_registry.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <vector>

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
  // NEW: pick the weakest-first policy from the yaml. Requires a
  //   std::string policy = "wrf";
  // field on swap_scheduler_config (see scheduler_expert_config.h). Absent /
  // unknown -> WRF, so existing configs are unchanged.
  p.policy            = swap_sched::policy_from_string(c.policy);
  return p;
}

// ---------------------------------------------------------------------------
// Exhaustive max-min optimum, for LOGGING ONLY (decision-cost comparison).
// Brute-forces every way to give each user rpu distinct RUs (disjoint) and
// returns the best achievable weakest-user floor. This does NOT drive the
// allocation — the swap still owns the air interface; we only compute the
// oracle value + its compute time to log alongside the swap's alloc_us, giving
// an apples-to-apples (C++, same CPU) swap-vs-optimal timing.
//
// WARNING: the number of assignments explodes with U and R. A guard caps it
// so this can never stall the real-time scheduler; above the cap it is skipped
// and logged as -1. Safe at the testbed scale (U=3,R=6 -> 90 assignments).
// ---------------------------------------------------------------------------
using csi_grid = swap_sched::csi_grid;
using assignment = std::vector<std::vector<unsigned>>; // [user] -> owned RUs

void mm_user(const csi_grid&, unsigned, unsigned, unsigned, unsigned,
             std::vector<char>&, std::vector<double>&, assignment&, double&,
             assignment&);

void mm_comb(const csi_grid& csi, unsigned U, unsigned rpu, unsigned R,
             unsigned user, unsigned start, unsigned k,
             std::vector<unsigned>& pick, std::vector<char>& used,
             std::vector<double>& served, assignment& cur, double& best,
             assignment& bestA)
{
  if (k == 0) {
    double s = 0.0;
    for (unsigned r : pick) s += csi[user][r];
    served[user] = s;
    cur[user]    = pick;                 // record this user's chosen RU set
    mm_user(csi, U, rpu, R, user + 1, used, served, cur, best, bestA);
    return;
  }
  for (unsigned r = start; r < R; ++r) {
    if (used[r]) continue;
    used[r] = 1; pick.push_back(r);
    mm_comb(csi, U, rpu, R, user, r + 1, k - 1, pick, used, served, cur, best, bestA);
    pick.pop_back(); used[r] = 0;
  }
}

void mm_user(const csi_grid& csi, unsigned U, unsigned rpu, unsigned R,
             unsigned user, std::vector<char>& used, std::vector<double>& served,
             assignment& cur, double& best, assignment& bestA)
{
  if (user == U) {
    double w = served[0];
    for (unsigned u = 1; u < U; ++u) w = std::min(w, served[u]);
    if (w > best) { best = w; bestA = cur; }   // keep the WINNING assignment
    return;
  }
  std::vector<unsigned> pick;
  pick.reserve(rpu);
  mm_comb(csi, U, rpu, R, user, 0, rpu, pick, used, served, cur, best, bestA);
}

// Returns the optimal weakest-user floor AND fills `bestA` with the assignment
// that achieves it — so the timed work is "find + form the optimal allocation",
// not merely compute a scalar.
double exhaustive_maxmin(const csi_grid& csi, unsigned U, unsigned rpu, unsigned R,
                         assignment& bestA)
{
  std::vector<char>   used(R, 0);
  std::vector<double> served(U, 0.0);
  assignment          cur(U);
  double best = -1.0;
  mm_user(csi, U, rpu, R, 0, used, served, cur, best, bestA);
  return best;
}

// Number of valid assignments = prod_u C(R - u*rpu, rpu). Early-outs at the cap.
unsigned long long mm_count(unsigned U, unsigned rpu, unsigned R)
{
  unsigned long long total = 1;
  unsigned avail = R;
  for (unsigned u = 0; u < U; ++u) {
    if (avail < rpu) return 0;
    unsigned long long c = 1;
    for (unsigned i = 0; i < rpu; ++i) c = c * (avail - i) / (i + 1);
    total *= c;
    avail -= rpu;
    if (total > 1000000ULL) return total; // huge -> caller will skip
  }
  return total;
}

constexpr unsigned long long MM_ASSIGN_CAP = 200000ULL; // real-time safety guard

} // namespace

scheduler_swap::scheduler_swap(const swap_scheduler_config& cfg_) : cfg(cfg_), alloc(to_params(cfg_))
{
  ue_to_user.fill(-1);
  user_to_ue.assign(cfg.num_users, INVALID_DU_UE_INDEX);          // VERIFY sentinel name
  user_to_rnti.assign(cfg.num_users, rnti_t::INVALID_RNTI);        // VERIFY sentinel name
  user_last_seen.assign(cfg.num_users, slot_point{});
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
  const unsigned R = num_rus(); // = num_users*rus_per_user + num_redun (e.g. 6 at rus_per_user=2)

  // Initialize the grid mapping matrix for 4 RUs
  swap_sched::csi_grid csi(U, std::vector<double>(R, 1e-9));

  for (unsigned u = 0; u != U; ++u) {
    if (user_to_rnti[u] == rnti_t::INVALID_RNTI) {
      continue; 
    }
    
    auto g = csi_grid_registry::instance().get(user_to_rnti[u]); // [rx_port][rb]
    
    for (const auto& per_port : g) {                             
      for (unsigned r = 0; r != R; ++r) {
        // Truly dynamic mapping based on your YAML configuration parameter
        const unsigned rb_begin = r * cfg.rbs_per_ru; 
        const unsigned rb_end   = std::min<unsigned>(rb_begin + cfg.rbs_per_ru, per_port.size());

        
        for (unsigned rb = rb_begin; rb < rb_end; ++rb) {
          csi[u][r] += per_port[rb];
        }
      }
    }
  }
  return csi;
}


void scheduler_swap::maybe_run_swap(slot_point sl, span<ue_newtx_candidate> candidates)
{
// --- temp instrumentation: log candidate presence/pending_bytes per slot ---
  {
    static std::ofstream candlog("/tmp/cand_presence.jsonl", std::ios::app);
    const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::system_clock::now().time_since_epoch()).count();
    for (const ue_newtx_candidate& c : candidates) {
      candlog << "{\"t\":" << now_ms
              << ",\"rnti\":" << static_cast<unsigned>(c.ue->crnti())
              << ",\"pending_bytes\":" << c.pending_bytes.value() << "}\n";
    }
    candlog.flush();
  }
// --- end instrumentation ---

// Learn / refresh each candidate UE's mapping and RNTI (lazy, works for any UE count).
  for (const ue_newtx_candidate& c : candidates) {
    du_ue_index_t idx = c.ue->ue_index();
    if (ue_to_user[idx] < 0) {
      add_ue(idx); // first sight of this UE: claim a free user slot

      // --- temp instrumentation: log every add_ue (re-)registration event ---
      {
        static std::ofstream addlog("/tmp/add_ue_events.jsonl", std::ios::app);
        const auto add_now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::system_clock::now().time_since_epoch()).count();
        addlog << "{\"t\":" << add_now_ms
               << ",\"rnti\":" << static_cast<unsigned>(c.ue->crnti()) << "}\n";
        addlog.flush();
      }
      // --- end instrumentation ---
    }
    int u = ue_to_user[idx];
    if (u >= 0) {
      user_to_rnti[u]   = c.ue->crnti();
      user_last_seen[u] = sl;
    }
  }
  
  // =========================================================================
  // FIX: Protect inactive/buffering UEs (like YouTube) via SRS confirmation
  // =========================================================================
  for (unsigned u = 0; u != user_to_ue.size(); ++u) {
    if (user_to_ue[u] != INVALID_DU_UE_INDEX) {
      
      // Query the global SRS registry. If it returns an empty grid, the physical link is gone.
      bool srs_is_alive = !csi_grid_registry::instance().get(user_to_rnti[u]).empty(); 
      
      // Only clean up the user slot if their physical uplink SRS disappears, 
      // OR if they have been completely silent across a massive fallback timeout window (500x).
      if (!srs_is_alive || (user_last_seen[u].valid() && 
          (sl - user_last_seen[u]) > static_cast<int>(500 * cfg.swap_period_slots))) {
          
        csi_grid_registry::instance().erase(user_to_rnti[u]);
        rem_ue(user_to_ue[u]);
        user_last_seen[u] = slot_point{};
      }
    }
  }
  // =========================================================================

  // =========================================================================

  // Gate to the SRS reporting period (run once per cfg.swap_period_slots).
  if (last_swap_slot.valid() && (sl - last_swap_slot) < static_cast<int>(cfg.swap_period_slots)) {
    return;
  }
  last_swap_slot = sl;

  swap_sched::csi_grid csi = build_csi_grid();
  auto _t0 = std::chrono::high_resolution_clock::now();
  alloc.step(csi);
  auto _t1 = std::chrono::high_resolution_clock::now();
  double alloc_us = std::chrono::duration<double,std::micro>(_t1-_t0).count();

  // LOGGING ONLY: exhaustive max-min optimum + its compute time, on the SAME
  // CSI and CPU, so opt_alloc_us is directly comparable to the swap's alloc_us.
  // Does NOT touch the allocation. Guarded so it can never stall the RT loop.
  double opt_weakest = -1.0, opt_alloc_us = -1.0;
  {
    const unsigned Uu = cfg.num_users;
    const unsigned Rr = num_rus();
    if (mm_count(Uu, cfg.rus_per_user, Rr) <= MM_ASSIGN_CAP) {
      assignment opt_assign(Uu);
      auto _o0 = std::chrono::high_resolution_clock::now();
      opt_weakest = exhaustive_maxmin(csi, Uu, cfg.rus_per_user, Rr, opt_assign);
      auto _o1 = std::chrono::high_resolution_clock::now();
      opt_alloc_us = std::chrono::duration<double,std::micro>(_o1-_o0).count();
      // opt_assign now holds the optimal allocation (not applied — oracle only).
    }
  }

  // --- metrics: one JSON line per period ---
  if (!metrics_log.is_open()) {
    metrics_log.open("/tmp/swap_metrics.jsonl", std::ios::app);
  }
  if (metrics_log.is_open()) {
    const auto& assign  = alloc.assignment();
    double      weakest = std::numeric_limits<double>::max();
    metrics_log << "{\"t\":" << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch()).count()
                << ",\"slot\":" << sl.count() << ",\"users\":[";
    for (unsigned u = 0; u < csi.size(); ++u) {
      double served = 0.0;
      for (unsigned ru : assign[u]) {
        if (ru < csi[u].size()) {
          served += csi[u][ru];
        }
      }
      weakest = std::min(weakest, served);
      if (u) {
        metrics_log << ",";
      }
      metrics_log << "{\"u\":" << u << ",\"rnti\":" << static_cast<unsigned>(user_to_rnti[u]) << ",\"served\":" << served << ",\"rus\":[";
      for (unsigned k = 0; k < assign[u].size(); ++k) {
        if (k) {
          metrics_log << ",";
        }
        metrics_log << assign[u][k];
      }
      metrics_log << "],\"csi\":[";
      for (unsigned r = 0; r < csi[u].size(); ++r) {
        if (r) {
          metrics_log << ",";
        }
        metrics_log << csi[u][r];
      }
      metrics_log << "]}";
    }
    // NEW: stamp the active policy and R so each capture is self-describing.
    // The offline tools (swap_policies.py / plot_outage.py) can then auto-set
    // --rus-per-user and label WRF vs WUF without a side file.
    metrics_log << "],\"weakest_user_csi\":" << weakest
                << ",\"alloc_us\":" << alloc_us
                << ",\"opt_weakest\":" << opt_weakest
                << ",\"opt_alloc_us\":" << opt_alloc_us
                << ",\"policy\":\""
                << (swap_sched::policy_from_string(cfg.policy) == swap_sched::swap_policy::WUF
                        ? "wuf" : "wrf")
                << "\",\"rus_per_user\":" << cfg.rus_per_user
                << "}\n";
    metrics_log.flush();
  }
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
