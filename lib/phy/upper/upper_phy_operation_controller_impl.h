// SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
// SPDX-License-Identifier: BSD-3-Clause-Open-MPI
// Portions of this file may implement 3GPP specifications, which may be subject to additional licensing requirements.

#pragma once

#include "ocudu/phy/upper/upper_phy_operation_controller.h"
#include <functional>
#include <utility>

namespace ocudu {

/// Concrete upper PHY operation controller used in the monolithic DU.
///
/// The actual gating mechanism (toggling the per-sector slot-indication delivery in the FAPI P7
/// translator) lives outside the upper PHY layer, so this controller stores the activation target
/// as a callback set by the factory after the FAPI adaptor has been constructed. Until the target
/// is wired the controller is a no-op so that the upper PHY can be safely used in isolation
/// (e.g. unit tests).
class upper_phy_operation_controller_impl : public upper_phy_operation_controller
{
public:
  void set_active_target(std::function<void(bool)> target) { active_target = std::move(target); }

  void start() override
  {
    if (active_target) {
      active_target(true);
    }
  }

  void stop() override
  {
    if (active_target) {
      active_target(false);
    }
  }

private:
  std::function<void(bool)> active_target;
};

} // namespace ocudu
