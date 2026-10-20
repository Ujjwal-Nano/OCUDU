// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI

#include "scheduler_swap.h"
#include "../slicing/slice_ue_repository.h" // for slice_ue accessors  // VERIFY path
#include "ocudu/support/csi_grid_registry.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <limits>
#include <utility>
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

// Repetitions for the sub-10us decision-cost microbenchmarks (baselines + the
// epsilon-swept swap). A SINGLE timed call cannot resolve the ~0.1us per-epsilon
// difference on a CPU that is also running the gNB: clock_gettime overhead and OS
// jitter are the same size, so the epsilon ordering came out essentially at random
// (the inversion landed on GGM in one capture and on WRF in the next, with
// identical code). We instead run DECISION_TIMING_REPS reps and take the MINIMUM
// time: the fastest rep is the one with the least interference, i.e. closest to
// the pure compute cost, and it is robust to preemption spikes (those only make a
// rep slower). Since the per-epsilon work is genuinely monotone, a clean
// measurement is monotone. Tunable: raise for a cleaner min, lower if the extra
// per-period work (~REPS x a few us, logging-only, once per swap period) risks RT
// slot deadlines. The real applied-step tail stays in alloc_us.
constexpr int DECISION_TIMING_REPS = 32;

// ---------------------------------------------------------------------------
// TIMING-ONLY implementations of the Zhang paper's baseline algorithms. They run
// each swap period on the SAME CSI and CPU as the real swap, purely to measure
// their decision cost against the swap's alloc_us — their output is DISCARDED
// (the swap, WRF/WUF, owns the air interface). Faithful ports of the validated
// offline comparator (compare_algorithms.py); the returned floor is only there
// so the compiler cannot optimise the work away (callers store it `volatile`).
// GGM is NOT here: it is the same pairwise machinery as WRF/WUF, so alloc_us
// already represents its cost. Exhaustive is timed separately (opt_alloc_us).
// ---------------------------------------------------------------------------

// Kuhn augmenting path for the Max-Min bottleneck feasibility test.
bool kuhn_try(const csi_grid& csi, unsigned rpu, unsigned R, double theta,
              unsigned slot, std::vector<int>& match_col, std::vector<char>& seen)
{
  const unsigned u = slot / rpu;
  for (unsigned r = 0; r < R; ++r) {
    if (csi[u][r] >= theta && !seen[r]) {
      seen[r] = 1;
      if (match_col[r] == -1 ||
          kuhn_try(csi, rpu, R, theta, static_cast<unsigned>(match_col[r]), match_col, seen)) {
        match_col[r] = static_cast<int>(slot);
        return true;
      }
    }
  }
  return false;
}

bool mm_feasible(const csi_grid& csi, unsigned U, unsigned R, unsigned rpu, double theta)
{
  const unsigned nrows = U * rpu;
  std::vector<int> match_col(R, -1);   // resource -> matched user-slot
  unsigned matched = 0;
  for (unsigned s = 0; s < nrows; ++s) {
    std::vector<char> seen(R, 0);
    if (kuhn_try(csi, rpu, R, theta, s, match_col, seen)) matched++;
  }
  return matched == nrows;
}

// Max-Min (Trassl): maximise the minimum assigned per-resource gain s.t. each
// user gets rpu distinct RUs. Bottleneck via gain threshold + bipartite matching
// (same optimum as the paper's sort + Dulmage-Mendelsohn). O(R^3 U^5)-class.
double timing_maxmin(const csi_grid& csi, unsigned U, unsigned R, unsigned rpu)
{
  std::vector<double> gains;
  gains.reserve(static_cast<size_t>(U) * R);
  for (unsigned u = 0; u < U; ++u)
    for (unsigned r = 0; r < R; ++r) gains.push_back(csi[u][r]);
  std::sort(gains.begin(), gains.end(), std::greater<double>());
  gains.erase(std::unique(gains.begin(), gains.end()), gains.end());
  int lo = 0, hi = static_cast<int>(gains.size()) - 1, bestk = static_cast<int>(gains.size());
  while (lo <= hi) {                               // smallest index (largest theta) feasible
    int mid = (lo + hi) / 2;
    if (mm_feasible(csi, U, R, rpu, gains[mid])) { bestk = mid; hi = mid - 1; }
    else lo = mid + 1;
  }
  double theta = (bestk < static_cast<int>(gains.size())) ? gains[bestk]
                 : (gains.empty() ? 0.0 : gains.back());
  // FORM the allocation at the optimal threshold (part of the decision cost, so
  // the timed work is "find AND form" the allocation, like the exhaustive timer).
  std::vector<int> match_col(R, -1);               // resource -> matched user-slot
  for (unsigned s = 0; s < U * rpu; ++s) {
    std::vector<char> seen(R, 0);
    kuhn_try(csi, rpu, R, theta, s, match_col, seen);
  }
  std::vector<double> served(U, 0.0);
  for (unsigned r = 0; r < R; ++r)
    if (match_col[r] != -1)
      served[static_cast<unsigned>(match_col[r]) / rpu] += csi[static_cast<unsigned>(match_col[r]) / rpu][r];
  double fl = served[0];
  for (unsigned u = 1; u < U; ++u) fl = std::min(fl, served[u]);
  return fl;                                         // aggregate Gamma_min of the Max-Min allocation
}

// Preference-Based (Schwarzenberg, gradient variant): each round serve the
// most-constrained user (lowest served + best-available gain) its best free RU.
double timing_preference(const csi_grid& csi, unsigned U, unsigned R, unsigned rpu)
{
  std::vector<double>   served(U, 0.0);
  std::vector<char>     freeR(R, 1);
  std::vector<unsigned> need(U, rpu);
  for (unsigned step = 0; step < U * rpu; ++step) {
    int bu = -1; double bestpot = 0.0;
    for (unsigned u = 0; u < U; ++u) {
      if (need[u] == 0) continue;
      double ba = -1.0;
      for (unsigned r = 0; r < R; ++r) if (freeR[r] && csi[u][r] > ba) ba = csi[u][r];
      double pot = served[u] + ba;
      if (bu < 0 || pot < bestpot) { bu = static_cast<int>(u); bestpot = pot; }
    }
    if (bu < 0) break;
    int br = -1; double bg = -1.0;
    for (unsigned r = 0; r < R; ++r)
      if (freeR[r] && csi[bu][r] > bg) { bg = csi[bu][r]; br = static_cast<int>(r); }
    if (br < 0) { need[bu] = 0; continue; }
    served[bu] += csi[bu][br]; freeR[br] = 0; need[bu]--;
  }
  double fl = served[0];
  for (unsigned u = 1; u < U; ++u) fl = std::min(fl, served[u]);
  return fl;
}

// Bulk (Walstab): the most-constrained user claims all rpu of its top free RUs
// at once (U rounds instead of RU).
double timing_bulk(const csi_grid& csi, unsigned U, unsigned R, unsigned rpu)
{
  std::vector<double> served(U, 0.0);
  std::vector<char>   freeR(R, 1);
  std::vector<char>   done(U, 0);
  for (unsigned step = 0; step < U; ++step) {
    int bu = -1; double bestpot = 0.0;
    for (unsigned u = 0; u < U; ++u) {
      if (done[u]) continue;
      std::vector<double> vals;
      for (unsigned r = 0; r < R; ++r) if (freeR[r]) vals.push_back(csi[u][r]);
      std::sort(vals.begin(), vals.end(), std::greater<double>());
      double ts = 0.0;
      for (unsigned i = 0; i < rpu && i < vals.size(); ++i) ts += vals[i];
      double pot = served[u] + ts;
      if (bu < 0 || pot < bestpot) { bu = static_cast<int>(u); bestpot = pot; }
    }
    if (bu < 0) break;
    std::vector<std::pair<double, unsigned>> order;
    for (unsigned r = 0; r < R; ++r) if (freeR[r]) order.push_back({csi[bu][r], r});
    std::sort(order.begin(), order.end(), std::greater<std::pair<double, unsigned>>());
    for (unsigned i = 0; i < rpu && i < order.size(); ++i) {
      unsigned r = order[i].second; served[bu] += csi[bu][r]; freeR[r] = 0;
    }
    done[bu] = 1;
  }
  double fl = served[0];
  for (unsigned u = 1; u < U; ++u) fl = std::min(fl, served[u]);
  return fl;
}

// Epsilon-swept swap decision (WRF or GGM), TIMING-ONLY. Warm-started from the
// current assignment `start`; runs one pairwise-swap decision at the given
// epsilon and returns the resulting weakest floor (output discarded). policy:
// 0 = WRF (worst-resource-first), 1 = GGM (global highest-gain-first). Mirrors
// compare_algorithms.py policy_wrf / policy_ggm so the timing is representative
// across epsilon. At R=1 epsilon does not bind (min(eps, #RUs)=#RUs), so all
// epsilons coincide; they separate at R>=2.
double timing_swap(const csi_grid& csi, const assignment& start,
                   unsigned U, unsigned R, unsigned epsilon, int policy)
{
  assignment ur = start;                   // mutated copy, discarded
  std::vector<int>    owner(R, -1);
  std::vector<double> ru_perf(R, 0.0);
  for (unsigned u = 0; u < U; ++u)
    for (unsigned r : ur[u]) { owner[r] = static_cast<int>(u); ru_perf[r] = csi[u][r]; }
  std::vector<unsigned> allocated;
  for (unsigned r = 0; r < R; ++r) if (owner[r] != -1) allocated.push_back(r);
  std::sort(allocated.begin(), allocated.end(),
            [&](unsigned a, unsigned b) { return ru_perf[a] < ru_perf[b]; });
  unsigned nw = std::min<unsigned>(epsilon, static_cast<unsigned>(allocated.size()));
  std::vector<unsigned> worst(allocated.begin(), allocated.begin() + nw);
  std::vector<unsigned> spares;
  for (unsigned r = 0; r < R; ++r) if (owner[r] == -1) spares.push_back(r);

  struct Op { int type; unsigned wr, tr; double gain; int B; };
  std::vector<Op> ops;
  for (unsigned wr : worst) {
    unsigned A = static_cast<unsigned>(owner[wr]);
    double c_A_worst = csi[A][wr];
    for (unsigned B = 0; B < U; ++B) {
      if (B == A || ur[B].empty()) continue;
      for (unsigned tr : ur[B]) {
        double d_swap = std::min(csi[A][tr], csi[B][wr]);
        double d_curr = std::min(c_A_worst, csi[B][tr]);
        if (d_swap > d_curr && c_A_worst < csi[A][tr])
          ops.push_back({1, wr, tr, (d_swap - d_curr) / csi[B][wr], static_cast<int>(B)});
      }
    }
    for (unsigned sr : spares)
      if (csi[A][sr] > c_A_worst)
        ops.push_back({2, wr, sr, (csi[A][sr] - c_A_worst) / c_A_worst, -1});
  }

  std::vector<char> locked(R, 0);
  auto apply_op = [&](const Op& o) {
    unsigned A = static_cast<unsigned>(owner[o.wr]);
    for (auto& x : ur[A]) if (x == o.wr) { x = o.tr; break; }
    if (o.type == 1) {
      for (auto& x : ur[o.B]) if (x == o.tr) { x = o.wr; break; }
      owner[o.wr] = o.B;
    } else {
      owner[o.wr] = -1;
    }
    owner[o.tr] = static_cast<int>(A);
    locked[o.wr] = 1; locked[o.tr] = 1;
  };

  if (policy == 1) {                        // GGM: global highest-gain-first
    std::sort(ops.begin(), ops.end(), [](const Op& a, const Op& b) { return a.gain > b.gain; });
    for (auto& o : ops)
      if (!locked[o.wr] && !locked[o.tr] && o.gain > 0.0) apply_op(o);
  } else {                                  // WRF: worst-RU-first, one swap per worst RU
    for (unsigned wr : worst) {             // worst is already ascending by current quality
      if (locked[wr]) continue;
      std::vector<Op> ru_ops;
      for (auto& o : ops) if (o.wr == wr) ru_ops.push_back(o);
      std::sort(ru_ops.begin(), ru_ops.end(), [](const Op& a, const Op& b) { return a.gain > b.gain; });
      for (auto& o : ru_ops)
        if (!locked[o.wr] && !locked[o.tr] && o.gain > 0.0) { apply_op(o); break; }
    }
  }

  double fl = std::numeric_limits<double>::max();
  for (unsigned u = 0; u < U; ++u) {
    double s = 0.0;
    for (unsigned r : ur[u]) s += csi[u][r];
    fl = std::min(fl, s);
  }
  return fl;
}

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
  // Capture the PRE-step assignment: the real warm-start for this period's swap
  // (the previous period's result, not yet adapted to the new CSI). The epsilon-
  // swept timers below start from HERE, so they do genuine per-period adaptation
  // work and epsilon actually binds — timing from the post-step (already-adapted)
  // assignment on the same CSI is near-idempotent and hides the epsilon effect.
  assignment prev_assign(cfg.num_users);
  {
    const auto& aa0 = alloc.assignment();
    for (unsigned u = 0; u < cfg.num_users; ++u)
      for (unsigned ru : aa0[u]) prev_assign[u].push_back(ru);
  }
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

  // LOGGING ONLY: time the paper's baselines (Max-Min / Preference / Bulk) on the
  // SAME CSI and CPU, so their decision cost is directly comparable to the swap's
  // alloc_us (and GGM's, which shares the swap machinery). Output DISCARDED — the
  // swap owns the air. `volatile` stops the compiler eliding the work.
  double maxmin_us = -1.0, pref_us = -1.0, bulk_us = -1.0;
  {
    const unsigned Uu = cfg.num_users;
    const unsigned Rr = num_rus();
    if (Uu * cfg.rus_per_user <= Rr) {
      // Microbenchmark each baseline the SAME way as the swap timers (warm once,
      // then take the MINIMUM over DECISION_TIMING_REPS reps), so the cross-method
      // comparison is apples-to-apples in measurement method, not just in CSI/CPU.
      // The min is the least-interference run ~ pure compute cost; it is stable
      // run-to-run, unlike the old single-shot call whose jitter (~0.1-0.5 us) was
      // a real fraction of pref/bulk and shuffled the ordering between captures.
      auto bench = [&](auto fn) {
        volatile double warm = fn(csi, Uu, Rr, cfg.rus_per_user); (void)warm; // warm this path
        double best = std::numeric_limits<double>::max();
        for (int i = 0; i < DECISION_TIMING_REPS; ++i) {
          auto a = std::chrono::high_resolution_clock::now();
          volatile double v = fn(csi, Uu, Rr, cfg.rus_per_user); (void)v;
          auto b = std::chrono::high_resolution_clock::now();
          best = std::min(best, std::chrono::duration<double,std::micro>(b-a).count());
        }
        return best;
      };
      maxmin_us = bench(timing_maxmin);
      pref_us   = bench(timing_preference);
      bulk_us   = bench(timing_bulk);
    }
  }

  // LOGGING ONLY: epsilon-swept swap decision times — WRF and GGM at eps {1,2,4},
  // timed on the gNB CPU, warm-started from the current assignment, output
  // discarded. At R=1 epsilon does not bind so they coincide; they separate at
  // R>=2. Logged per period so timing-vs-epsilon can be plotted offline.
  double wrf_us_e1=-1.0, wrf_us_e2=-1.0, wrf_us_e4=-1.0;
  double ggm_us_e1=-1.0, ggm_us_e2=-1.0, ggm_us_e4=-1.0;
  {
    const unsigned Uu = cfg.num_users;
    const unsigned Rr = num_rus();
    if (Uu * cfg.rus_per_user <= Rr) {
      // warm-start from the PRE-step assignment (real per-period adaptation)
      const assignment& cur = prev_assign;
      // Microbenchmark each (policy, epsilon): warm it, then take the MINIMUM over
      // DECISION_TIMING_REPS reps. A single timed call could NOT resolve the
      // ~0.1us per-epsilon step on this CPU, so the epsilon ordering came out at
      // random per capture (GGM inverted in one run, WRF in the next, same code).
      // Per-path warming alone did not fix that — it only moved which policy
      // showed the inversion — because the problem is measurement resolution, not
      // cache ordering. The min over many reps is the least-interference run, i.e.
      // the true compute cost, and since the per-epsilon work is monotone (ops grow
      // with epsilon; WRF's apply loop and GGM's global sort both scale with it),
      // the measured trend is now monotone for BOTH policies. timing_swap copies
      // `start` each call, so the reps are independent and all warm-start from the
      // same pre-step assignment.
      auto timeit = [&](int pol, unsigned e) {
        volatile double warm = timing_swap(csi, cur, Uu, Rr, e, pol); (void)warm; // warm THIS (pol,e)
        double best = std::numeric_limits<double>::max();
        for (int i = 0; i < DECISION_TIMING_REPS; ++i) {
          auto a = std::chrono::high_resolution_clock::now();
          volatile double v = timing_swap(csi, cur, Uu, Rr, e, pol); (void)v;
          auto b = std::chrono::high_resolution_clock::now();
          best = std::min(best, std::chrono::duration<double,std::micro>(b-a).count());
        }
        return best;
      };
      wrf_us_e1 = timeit(0, 1); wrf_us_e2 = timeit(0, 2); wrf_us_e4 = timeit(0, 4);
      ggm_us_e1 = timeit(1, 1); ggm_us_e2 = timeit(1, 2); ggm_us_e4 = timeit(1, 4);
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
    // NEW: stamp the active policy, R and epsilon so each capture is
    // self-describing. The offline tools (swap_policies.py / plot_outage.py /
    // swap_decision_quality.py) can then auto-set --rus-per-user, label WRF vs
    // WUF, and show epsilon without a side file.
    metrics_log << "],\"weakest_user_csi\":" << weakest
                << ",\"alloc_us\":" << alloc_us
                << ",\"opt_weakest\":" << opt_weakest
                << ",\"opt_alloc_us\":" << opt_alloc_us
                << ",\"policy\":\""
                << (swap_sched::policy_from_string(cfg.policy) == swap_sched::swap_policy::WUF
                        ? "wuf" : "wrf")
                << "\",\"rus_per_user\":" << cfg.rus_per_user
                << ",\"epsilon\":" << cfg.epsilon
                << ",\"maxmin_us\":" << maxmin_us
                << ",\"pref_us\":" << pref_us
                << ",\"bulk_us\":" << bulk_us
                << ",\"wrf_us_e1\":" << wrf_us_e1
                << ",\"wrf_us_e2\":" << wrf_us_e2
                << ",\"wrf_us_e4\":" << wrf_us_e4
                << ",\"ggm_us_e1\":" << ggm_us_e1
                << ",\"ggm_us_e2\":" << ggm_us_e2
                << ",\"ggm_us_e4\":" << ggm_us_e4
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
