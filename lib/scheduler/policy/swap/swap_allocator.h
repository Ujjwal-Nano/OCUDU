/*
 * swap_allocator.h
 *
 * Standalone C++ port of the MATLAB "Periodic Swap - Worst-Resource-First
 * (min_csi)" resource-allocation algorithm.
 *
 * Design goal: framework-agnostic so it can be unit-tested on its own (Step 1
 * of the OCUDU integration roadmap) and later wrapped by an OCUDU scheduler
 * policy class (sibling of scheduler_time_rr / qos) without changing this core.
 *
 * The only input it needs each period is a per-(user, resource-unit) CSI grid:
 *      csi[u][r] = link quality user u would experience on resource-unit r
 * which is exactly the [users x resources] matrix an SRS-derived channel
 * estimate provides. Values are linear and strictly positive (e.g. received
 * power or linear SINR), matching the MATLAB user_csi{u}(ru, t).
 */
#pragma once

#include <cstdint>
#include <vector>

namespace swap_sched {

/// Parameters, mirroring the MATLAB script's "System Parameters" block.
struct swap_params {
  unsigned num_users         = 10;  ///< number of UEs
  unsigned rus_per_user      = 1;   ///< resource-units owned per UE
  unsigned num_redun         = 0;   ///< spare (unowned) resource-units
  unsigned epsilon           = 4;   ///< worst RUs reconsidered each period
  unsigned allocation_period = 1;   ///< reallocate every N periods
                                    ///< (tie to the SRS period in OCUDU)
  unsigned allocation_delay  = 0;   ///< CSI staleness, in periods

  unsigned num_rus() const { return num_users * rus_per_user + num_redun; }
};

/// csi[u][r] = quality of user u on resource-unit r (linear, > 0).
using csi_grid = std::vector<std::vector<double>>;

/// Outcome of a single reallocation period.
struct realloc_stats {
  unsigned swaps    = 0;  ///< Type-1 user<->user RU swaps applied
  unsigned replaces = 0;  ///< Type-2 replacements from the spare pool
  bool     ran      = false; ///< whether reallocation was due this period
};

/// Sentinel owner id meaning "resource-unit is unassigned / spare".
constexpr unsigned UNASSIGNED = UINT32_MAX;

class swap_allocator {
public:
  explicit swap_allocator(swap_params p);

  /// First-period assignment: random one-RU-per-user from the free pool.
  /// Call once before the first step(). Seed is fixed for reproducible A/B.
  void init_assignment(uint64_t seed = 1);

  /// Advance one period using the per-(user, RU) CSI snapshot for "now".
  /// Past snapshots are buffered internally so allocation_delay can decide on
  /// stale CSI (matching the MATLAB csi_ref_t = t - allocation_delay).
  /// The internal assignment is updated in place; returns what happened.
  realloc_stats step(const csi_grid& csi_now);

  /// Current assignment: user -> list of owned resource-units.
  const std::vector<std::vector<unsigned>>& assignment() const { return user_rus_; }

  /// Owner of each resource-unit (UNASSIGNED for spares).
  std::vector<unsigned> ru_to_user() const;

private:
  swap_params                        params_;
  std::vector<std::vector<unsigned>> user_rus_;     ///< [user] -> RUs
  std::vector<csi_grid>              csi_history_;   ///< snapshots, for delay
  unsigned                           period_idx_ = 0;
};

// ---- Free helpers for the comparison metric (weakest-user analysis) --------

/// Per-user served quality at a given snapshot: sum of the user's RU CSI.
/// Users with no RUs report 0 (excluded from the weakest-user statistic).
std::vector<double> user_served_csi(const std::vector<std::vector<unsigned>>& assignment,
                                    const csi_grid&                            csi);

/// Weakest served quality across users that actually hold resources.
/// This is the quantity whose outage CCDF the MATLAB script reports.
double weakest_user_csi(const std::vector<std::vector<unsigned>>& assignment,
                        const csi_grid&                            csi);

} // namespace swap_sched
