/*
 * swap_allocator.cpp  -  implementation of the periodic worst-first swap.
 *
 * Faithful port of periodic_swap_min_csi.m. The control flow mirrors the
 * MATLAB:  identify epsilon worst RUs  ->  generate candidate Type-1 (swap)
 * and Type-2 (replace-from-spare) operations  ->  apply them worst-first,
 * highest-gain-first, locking each RU once it has been touched this period.
 */
#include "swap_allocator.h"

#include <algorithm>
#include <numeric>
#include <random>

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
  stats.ran = true;

  // CSI snapshot used for the decision (possibly stale by allocation_delay).
  const std::size_t ref = static_cast<std::size_t>(std::max<long>(1, t_alloc));
  const csi_grid&   csi = csi_history_[ref - 1];

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

} // namespace swap_sched
