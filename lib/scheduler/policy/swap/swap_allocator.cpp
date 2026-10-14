/*
 * swap_allocator.cpp  -  periodic worst-first resource swap, with a
 * yaml-selectable policy:
 *
 *   policy: wrf   (Weakest Resource First)  -- the paper / original behaviour:
 *                 key on the worst RU, pairwise swaps, normalised-gain ranking.
 *   policy: wuf   (Weakest User First)      -- key on the weakest USER's served
 *                 sum, pairwise swaps ranked by the resulting max-min floor.
 *
 * At rus_per_user == 1 the two target the same resource; they can still pick a
 * different swap partner (WRF ranks by normalised gain, WUF by min(served_A,
 * served_B)). At rus_per_user >= 2 they genuinely diverge, and WUF is the one
 * aligned with the max-min objective. See tools/swap_policies.py for the
 * offline twin of these two functions (identical logic, same decisions).
 *
 * ---- ADD TO swap_allocator.h ----------------------------------------------
 *   enum class swap_policy { WRF, WUF };
 *
 *   // in struct swap_params:
 *   swap_policy policy = swap_policy::WRF;   // set from yaml "policy:"
 *
 *   // free helper (declare in header, defined at bottom of this file):
 *   swap_policy policy_from_string(const std::string& s);
 *
 *   // in class swap_allocator, private:
 *   realloc_stats decide_wrf_(const csi_grid& csi);
 *   realloc_stats decide_wuf_(const csi_grid& csi);
 * ---------------------------------------------------------------------------
 *
 * ---- WHERE YOU BUILD swap_params FROM YAML --------------------------------
 *   p.policy = swap_sched::policy_from_string(
 *                  cfg["policy"] ? cfg["policy"].as<std::string>() : "wrf");
 *   (defaults to WRF if the key is absent, so old configs are unchanged.)
 *
 * ---- LOGGER (swap.jsonl writer, wherever it lives) ------------------------
 *   stamp the active policy on each decision line so captures are self-describing:
 *       "policy":"wuf"
 *   e.g. append  (params_.policy==swap_policy::WUF ? "wuf" : "wrf").
 * ---------------------------------------------------------------------------
 */
#include "swap_allocator.h"

#include <algorithm>
#include <numeric>
#include <random>
#include <string>
#include <cctype>

namespace swap_sched {

namespace {
/// Replace the first occurrence of `from` with `to` in `v`.
void replace_in(std::vector<unsigned>& v, unsigned from, unsigned to) {
  for (unsigned& x : v) {
    if (x == from) { x = to; return; }
  }
}
} // namespace

swap_allocator::swap_allocator(swap_params p) : params_(p) {
  user_rus_.assign(params_.num_users, {});
}

void swap_allocator::init_assignment(uint64_t seed) {
  const unsigned R = params_.num_rus();

  // Free pool of all RUs, shuffled deterministically.
  std::vector<unsigned> pool(R);
  std::iota(pool.begin(), pool.end(), 0u);
  std::mt19937_64 rng(seed);
  std::shuffle(pool.begin(), pool.end(), rng);

  // Hand each user rus_per_user RUs from the shuffled pool.
  std::size_t next = 0;
  for (unsigned u = 0; u < params_.num_users; ++u) {
    user_rus_[u].clear();
    for (unsigned k = 0; k < params_.rus_per_user && next < pool.size(); ++k) {
      user_rus_[u].push_back(pool[next++]);
    }
  }
}

std::vector<unsigned> swap_allocator::ru_to_user() const {
  std::vector<unsigned> owner(params_.num_rus(), UNASSIGNED);
  for (unsigned u = 0; u < params_.num_users; ++u) {
    for (unsigned r : user_rus_[u]) {
      owner[r] = u;
    }
  }
  return owner;
}

// ---------------------------------------------------------------------------
// step(): shared gating + CSI-snapshot selection, then dispatch on policy.
// ---------------------------------------------------------------------------
realloc_stats swap_allocator::step(const csi_grid& csi_now) {
  realloc_stats stats;

  // t is 1-based, like the MATLAB loop index.
  ++period_idx_;
  csi_history_.push_back(csi_now);

  // Is reallocation due this period?
  const long t_alloc = static_cast<long>(period_idx_) -
                       static_cast<long>(params_.allocation_delay);
  const bool need = (t_alloc > 0) && (params_.allocation_period > 0) &&
                    (t_alloc % static_cast<long>(params_.allocation_period) == 0);
  if (!need) {
    return stats; // ran == false
  }

  // CSI snapshot used for the decision (possibly stale by allocation_delay).
  const std::size_t ref = static_cast<std::size_t>(std::max<long>(1, t_alloc));
  const csi_grid&   csi = csi_history_[ref - 1];

  return (params_.policy == swap_policy::WUF) ? decide_wuf_(csi)
                                              : decide_wrf_(csi);
}

// ---------------------------------------------------------------------------
// WRF — faithful original: worst RU first, Type-1 swap + Type-2 spare,
// normalised-gain ranking, lock touched RUs. (Unchanged logic.)
// ---------------------------------------------------------------------------
realloc_stats swap_allocator::decide_wrf_(const csi_grid& csi) {
  realloc_stats stats;
  stats.ran = true;

  const unsigned U = params_.num_users;
  const unsigned R = params_.num_rus();

  // RU -> owner and per-RU performance (owner's CSI on that RU).
  std::vector<unsigned> owner(R, UNASSIGNED);
  std::vector<double>   ru_perf(R, 0.0);
  for (unsigned u = 0; u < U; ++u) {
    for (unsigned r : user_rus_[u]) {
      owner[r]   = u;
      ru_perf[r] = csi[u][r];
    }
  }

  // Allocated RUs sorted ascending by performance; take the epsilon worst.
  std::vector<unsigned> allocated;
  allocated.reserve(R);
  for (unsigned r = 0; r < R; ++r) {
    if (owner[r] != UNASSIGNED) allocated.push_back(r);
  }
  std::sort(allocated.begin(), allocated.end(),
            [&](unsigned a, unsigned b) { return ru_perf[a] < ru_perf[b]; });
  const unsigned num_worst =
      std::min<unsigned>(params_.epsilon, static_cast<unsigned>(allocated.size()));
  std::vector<unsigned> worst(allocated.begin(), allocated.begin() + num_worst);

  // Spare RUs (unowned; only present when num_redun > 0).
  std::vector<unsigned> spares;
  for (unsigned r = 0; r < R; ++r) {
    if (owner[r] == UNASSIGNED) spares.push_back(r);
  }

  // ----- Generate candidate operations -----
  struct op {
    int      type;        // 1 = swap with another user's RU, 2 = take a spare
    unsigned worst_ru;
    unsigned target_ru;
    double   gain;
    unsigned target_user; // valid for type 1 only
  };
  std::vector<op> ops;

  for (unsigned wr : worst) {
    const unsigned A         = owner[wr];
    const double   c_A_worst = csi[A][wr];

    // Type 1: swap A's worst RU with some other user B's RU.
    for (unsigned B = 0; B < U; ++B) {
      if (B == A || user_rus_[B].empty()) continue;
      for (unsigned tr : user_rus_[B]) {
        const double c_A_target = csi[A][tr];
        const double c_B_worst  = csi[B][wr];
        const double c_B_target = csi[B][tr];
        const double d_swap     = std::min(c_A_target, c_B_worst);
        const double d_current  = std::min(c_A_worst, c_B_target);
        if (d_swap > d_current && c_A_worst < c_A_target) {
          const double gain = (d_swap - d_current) / c_B_worst;
          ops.push_back(op{1, wr, tr, gain, B});
        }
      }
    }

    // Type 2: replace A's worst RU with a better spare RU.
    for (unsigned sr : spares) {
      const double c_A_spare = csi[A][sr];
      if (c_A_spare > c_A_worst) {
        const double gain = (c_A_spare - c_A_worst) / c_A_worst;
        ops.push_back(op{2, wr, sr, gain, UNASSIGNED});
      }
    }
  }

  if (ops.empty()) {
    return stats;
  }

  // ----- Execute: worst-RU-first, then highest-gain-first per RU -----
  std::vector<char> locked(R, 0);

  // Priority order over the worst RUs: ascending current CSI (neediest first).
  std::vector<unsigned> order(worst.size());
  std::iota(order.begin(), order.end(), 0u);
  std::sort(order.begin(), order.end(), [&](unsigned i, unsigned j) {
    return csi[owner[worst[i]]][worst[i]] < csi[owner[worst[j]]][worst[j]];
  });

  for (unsigned idx : order) {
    const unsigned wr = worst[idx];
    if (locked[wr]) continue;

    // Gather this RU's candidate ops, best gain first.
    std::vector<op> ru_ops;
    for (const op& o : ops) {
      if (o.worst_ru == wr) ru_ops.push_back(o);
    }
    if (ru_ops.empty()) continue;
    std::sort(ru_ops.begin(), ru_ops.end(),
              [](const op& a, const op& b) { return a.gain > b.gain; });

    for (const op& o : ru_ops) {
      if (locked[wr] || locked[o.target_ru] || o.gain <= 0.0) continue;

      const unsigned A = owner[wr];
      if (o.type == 1) {
        const unsigned B = o.target_user;
        // Swap ownership of wr and target_ru between A and B.
        replace_in(user_rus_[A], wr, o.target_ru);
        replace_in(user_rus_[B], o.target_ru, wr);
        owner[wr]          = B;
        owner[o.target_ru] = A;
        ++stats.swaps;
      } else { // type 2: A moves onto the spare, wr becomes spare.
        replace_in(user_rus_[A], wr, o.target_ru);
        owner[wr]          = UNASSIGNED;
        owner[o.target_ru] = A;
        ++stats.replaces;
      }
      locked[wr]          = 1;
      locked[o.target_ru] = 1;
      break;
    }
  }

  return stats;
}

// ---------------------------------------------------------------------------
// WUF — weakest USER first. Same lightweight pairwise machine, but:
//   * the needy unit is the user with the smallest served sum (not the worst RU)
//   * a candidate is ranked by the resulting max-min floor (the objective
//     itself), min(served_A', served_B'), rather than normalised gain
//   * epsilon = how many of the weakest users we try to lift this period
// Reduces to the WRF *target* at rus_per_user == 1 (the weakest user owns the
// worst RU); diverges once a user holds >= 2 RUs. Mirrors tools/swap_policies.py
// policy_wuf exactly so the offline comparator predicts this live behaviour.
// ---------------------------------------------------------------------------
realloc_stats swap_allocator::decide_wuf_(const csi_grid& csi) {
  realloc_stats stats;
  stats.ran = true;

  const unsigned U = params_.num_users;
  const unsigned R = params_.num_rus();

  // served[u] = sum of the user's CSI over the RUs it currently holds.
  auto served_of = [&](unsigned u) {
    double s = 0.0;
    for (unsigned r : user_rus_[u]) s += csi[u][r];
    return s;
  };

  // Spare RUs (unowned).
  std::vector<char> is_owned(R, 0);
  for (unsigned u = 0; u < U; ++u)
    for (unsigned r : user_rus_[u]) is_owned[r] = 1;
  std::vector<unsigned> spares;
  for (unsigned r = 0; r < R; ++r)
    if (!is_owned[r]) spares.push_back(r);

  // The epsilon weakest users, ascending served.
  std::vector<unsigned> users(U);
  std::iota(users.begin(), users.end(), 0u);
  std::sort(users.begin(), users.end(),
            [&](unsigned a, unsigned b) { return served_of(a) < served_of(b); });
  const unsigned num_weak = std::max<unsigned>(1u, std::min<unsigned>(params_.epsilon, U));
  std::vector<unsigned> weak(users.begin(), users.begin() + num_weak);

  std::vector<char> locked(R, 0);

  for (unsigned A : weak) {
    if (user_rus_[A].empty()) continue;

    const double served_A = served_of(A);  // refreshed each weak-user turn

    struct op {
      int      type;        // 1 = swap A's ra with B's rb, 2 = take a spare
      unsigned ra;          // A's RU being given up
      unsigned target_ru;   // rb (type 1) or spare (type 2)
      double   score;       // resulting max-min floor to rank by (higher better)
      unsigned target_user; // B, type 1 only
    };
    std::vector<op> ops;

    for (unsigned ra : user_rus_[A]) {
      if (locked[ra]) continue;

      // Type 1: swap A's ra with user B's rb if it raises min(served_A,served_B).
      for (unsigned B = 0; B < U; ++B) {
        if (B == A || user_rus_[B].empty()) continue;
        const double served_B = served_of(B);
        for (unsigned rb : user_rus_[B]) {
          if (locked[rb]) continue;
          const double sA2   = served_A - csi[A][ra] + csi[A][rb];
          const double sB2   = served_B - csi[B][rb] + csi[B][ra];
          const double dswap = std::min(sA2, sB2);
          const double dcur  = std::min(served_A, served_B);
          if (dswap > dcur) {
            ops.push_back(op{1, ra, rb, dswap, B});
          }
        }
      }

      // Type 2: move A off ra onto a better spare (only helps A, hurts nobody).
      for (unsigned sr : spares) {
        if (locked[sr]) continue;
        if (csi[A][sr] > csi[A][ra]) {
          const double sA2 = served_A - csi[A][ra] + csi[A][sr];
          ops.push_back(op{2, ra, sr, sA2, UNASSIGNED});
        }
      }
    }

    if (ops.empty()) continue;

    // Pick the op that lifts the weakest user's floor the most.
    std::sort(ops.begin(), ops.end(),
              [](const op& a, const op& b) { return a.score > b.score; });
    const op& o = ops.front();
    const unsigned ra = o.ra, tr = o.target_ru;

    if (o.type == 1) {
      const unsigned B = o.target_user;
      replace_in(user_rus_[A], ra, tr);
      replace_in(user_rus_[B], tr, ra);
      ++stats.swaps;
    } else { // type 2: A takes the spare, ra becomes a spare.
      replace_in(user_rus_[A], ra, tr);
      // keep the spare set current for any later weak user this period
      spares.erase(std::remove(spares.begin(), spares.end(), tr), spares.end());
      spares.push_back(ra);
      ++stats.replaces;
    }
    locked[ra] = 1;
    locked[tr] = 1;
  }

  return stats;
}

// ---------------------------------------------------------------------------

std::vector<double> user_served_csi(const std::vector<std::vector<unsigned>>& assignment,
                                    const csi_grid&                            csi) {
  std::vector<double> served(assignment.size(), 0.0);
  for (std::size_t u = 0; u < assignment.size(); ++u) {
    double s = 0.0;
    for (unsigned r : assignment[u]) s += csi[u][r];
    served[u] = s;
  }
  return served;
}

double weakest_user_csi(const std::vector<std::vector<unsigned>>& assignment,
                        const csi_grid&                            csi) {
  double worst = -1.0;
  for (std::size_t u = 0; u < assignment.size(); ++u) {
    if (assignment[u].empty()) continue;
    double s = 0.0;
    for (unsigned r : assignment[u]) s += csi[u][r];
    if (worst < 0.0 || s < worst) worst = s;
  }
  return worst; // -1 if nobody holds resources
}

// policy name (yaml string, case-insensitive) -> enum. Unknown -> WRF.
swap_policy policy_from_string(const std::string& s) {
  std::string t;
  for (char c : s) t += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  if (t == "wuf" || t == "weakest_user" || t == "weakest-user") return swap_policy::WUF;
  return swap_policy::WRF;
}

} // namespace swap_sched
