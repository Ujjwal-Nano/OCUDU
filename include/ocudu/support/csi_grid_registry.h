// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI

#pragma once

#include "ocudu/ran/rnti.h"
#include <mutex>
#include <unordered_map>
#include <vector>

namespace ocudu {

/// Process-wide store for per-UE, SRS-derived per-RU channel power grids.
/// Written by the PHY (SRS results path), read by the scheduler. This is a
/// research-grade side channel that bypasses FAPI; not intended for upstreaming.
class csi_grid_registry
{
public:
  using grid_type = std::vector<std::vector<float>>; // [rx_port][ru]

  static csi_grid_registry& instance()
  {
    static csi_grid_registry inst;
    return inst;
  }

  /// Store (overwrite) the latest grid for a UE.
  void update(rnti_t rnti, grid_type grid)
  {
    std::lock_guard<std::mutex> lock(mtx);
    grids[rnti] = std::move(grid);
  }

  /// Latest grid for a UE, or empty if none yet.
  grid_type get(rnti_t rnti) const
  {
    std::lock_guard<std::mutex> lock(mtx);
    auto                        it = grids.find(rnti);
    return (it != grids.end()) ? it->second : grid_type{};
  }

  /// Remove a UE (call on UE release if you like).
  void erase(rnti_t rnti)
  {
    std::lock_guard<std::mutex> lock(mtx);
    grids.erase(rnti);
  }

private:
  csi_grid_registry() = default;

  mutable std::mutex                    mtx;
  std::unordered_map<rnti_t, grid_type> grids;
};

} // namespace ocudu